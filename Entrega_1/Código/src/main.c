/*
 * main.c - Leitura do acelerometro/giroscopio MPU6050 com ESP32-S NodeMCU.
 *
 * Trabalho Final - Instrumentacao Eletronica para Engenharia (FGA/UnB)
 *
 * O firmware:
 *   1) inicializa o barramento I2C e varre os enderecos presentes;
 *   2) configura o MPU6050 (fundo de escala, filtro digital, taxa de amostragem);
 *   3) estima os offsets com o sensor parado;
 *   4) amostra periodicamente e imprime os dados em CSV pela porta serial.
 *
 * A saida em CSV pode ser gravada em arquivo com:
 *   pio device monitor > dados.csv
 */

#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c_bus.h"
#include "mpu6050.h"

/* ---- Parametros do ensaio (ajuste aqui) -------------------------------- */
#define I2C_PORT            I2C_NUM_0
#define PIN_SDA             21          /* GPIO21 - pino D21 do NodeMCU-32S */
#define PIN_SCL             22          /* GPIO22 - pino D22 do NodeMCU-32S */
#define I2C_FREQ_HZ         400000      /* fast mode; use 100000 com fios longos */
#define USE_INTERNAL_PULLUP false       /* true se o modulo nao tiver pull-ups */

#define MPU_ADDR            MPU6050_ADDR_AD0_LOW  /* AD0 em GND -> 0x68 */
#define SAMPLE_RATE_HZ      100         /* taxa de amostragem (Hz) */
#define CALIB_SAMPLES       500         /* amostras usadas na estimativa do offset */

/* Peso do acelerometro no filtro complementar de inclinacao.
 * ALPHA proximo de 1 confia mais no giroscopio (menos ruido, mais deriva). */
#define ALPHA               0.98f

#define G_TO_MS2            9.80665f
#define RAD_TO_DEG          57.29577951f

static const char *TAG = "main";

/* Angulos de inclinacao estimados pelo filtro complementar. */
static float s_roll_deg  = 0.0f;
static float s_pitch_deg = 0.0f;

/* Inclinacao instantanea medida apenas pelo vetor gravidade.
 * Valida somente quando a aceleracao externa e pequena. */
static void tilt_from_accel(const mpu6050_data_t *d, float *roll, float *pitch)
{
    *roll  = atan2f(d->accel_y, d->accel_z) * RAD_TO_DEG;
    *pitch = atan2f(-d->accel_x,
                    sqrtf(d->accel_y * d->accel_y + d->accel_z * d->accel_z)) * RAD_TO_DEG;
}

/* Funde a integracao do giroscopio (rapida, com deriva) com a inclinacao do
 * acelerometro (sem deriva, porem ruidosa). dt em segundos. */
static void complementary_filter(const mpu6050_data_t *d, float dt)
{
    float roll_acc, pitch_acc;
    tilt_from_accel(d, &roll_acc, &pitch_acc);

    s_roll_deg  = ALPHA * (s_roll_deg  + d->gyro_x * dt) + (1.0f - ALPHA) * roll_acc;
    s_pitch_deg = ALPHA * (s_pitch_deg + d->gyro_y * dt) + (1.0f - ALPHA) * pitch_acc;
}

/* Ponto de entrada da aplicacao. No ambiente ESP-IDF e chamado direto pelo
 * app_main(); no ambiente Arduino e chamado pelo setup() em arduino_entry.cpp. */
void mpu_app_start(void)
{
    ESP_LOGI(TAG, "=== MPU6050 + ESP32-S NodeMCU ===");

    /* 1) Barramento I2C. */
    ESP_ERROR_CHECK(i2c_bus_init(I2C_PORT, PIN_SDA, PIN_SCL,
                                 I2C_FREQ_HZ, USE_INTERNAL_PULLUP));
    i2c_bus_scan(I2C_PORT);

    /* 2) Sensor. */
    const mpu6050_config_t cfg = {
        .port           = I2C_PORT,
        .addr           = MPU_ADDR,
        .accel_fs       = MPU6050_ACCEL_FS_2G,   /* +/-2 g: melhor resolucao */
        .gyro_fs        = MPU6050_GYRO_FS_250DPS,
        .dlpf           = MPU6050_DLPF_44HZ,
        .sample_rate_hz = SAMPLE_RATE_HZ,
    };

    esp_err_t err = mpu6050_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha ao inicializar o MPU6050 (%s).", esp_err_to_name(err));
        ESP_LOGE(TAG, "Verifique: 3V3, GND, SDA=GPIO%d, SCL=GPIO%d e o pino AD0.",
                 PIN_SDA, PIN_SCL);
        return;  /* nao adianta seguir sem sensor */
    }

    uint8_t id = 0;
    mpu6050_who_am_i(&id);
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X | sensibilidades: %.0f LSB/g, %.1f LSB/(graus/s)",
             id, mpu6050_accel_lsb_per_g(), mpu6050_gyro_lsb_per_dps());

    /* 3) Offsets. Mantenha a placa parada e nivelada durante esta etapa. */
    ESP_LOGW(TAG, "Calibrando: deixe o sensor parado e nivelado por ~2 s...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_ERROR_CHECK(mpu6050_calibrate(CALIB_SAMPLES));

    /* Alinha o filtro com a posicao inicial para evitar transitorio. */
    mpu6050_data_t d;
    if (mpu6050_read(&d) == ESP_OK) {
        tilt_from_accel(&d, &s_roll_deg, &s_pitch_deg);
    }

    /* 4) Aquisicao periodica. Cabecalho no formato CSV. */
    printf("\n# fs=%d Hz | accel=+/-2g | gyro=+/-250dps | DLPF=44Hz\n", SAMPLE_RATE_HZ);
    printf("t_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,a_res_ms2,roll_deg,pitch_deg,temp_c\n");

    const TickType_t periodo = pdMS_TO_TICKS(1000 / SAMPLE_RATE_HZ);
    const float dt = 1.0f / (float)SAMPLE_RATE_HZ;

    TickType_t ultima = xTaskGetTickCount();
    uint32_t t_ms = 0;

    while (1) {
        /* vTaskDelayUntil mantem o periodo constante mesmo que o corpo do laco
         * varie de duracao: isso preserva a base de tempo da amostragem. */
        vTaskDelayUntil(&ultima, periodo);
        t_ms += 1000 / SAMPLE_RATE_HZ;

        err = mpu6050_read(&d);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "leitura falhou: %s", esp_err_to_name(err));
            continue;
        }

        complementary_filter(&d, dt);

        /* Modulo do vetor aceleracao, em m/s^2 (parado deve ficar perto de 9,81). */
        const float a_res = sqrtf(d.accel_x * d.accel_x +
                                  d.accel_y * d.accel_y +
                                  d.accel_z * d.accel_z) * G_TO_MS2;

        printf("%lu,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f\n",
               (unsigned long)t_ms,
               d.accel_x, d.accel_y, d.accel_z,
               d.gyro_x, d.gyro_y, d.gyro_z,
               a_res, s_roll_deg, s_pitch_deg, d.temp_c);
    }
}

#ifndef ARDUINO
/* Entrada padrao do ESP-IDF. No build Arduino o app_main() pertence ao core,
 * entao a aplicacao entra por arduino_entry.cpp. */
void app_main(void)
{
    mpu_app_start();
}
#endif
