#include "mpu6050.h"
#include "i2c_bus.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mpu6050";

/* ---- Mapa de registradores usados ------------------------------------- */
#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_ACCEL_XOUT_H  0x3B  /* inicio do bloco de 14 bytes de dados */
#define REG_PWR_MGMT_1    0x6B
#define REG_WHO_AM_I      0x75

#define PWR_MGMT_1_DEVICE_RESET  0x80
#define PWR_MGMT_1_SLEEP         0x40
#define PWR_MGMT_1_CLK_PLL_XGYRO 0x01  /* PLL do giro X: mais estavel que o oscilador interno */

/* ---- Estado do driver -------------------------------------------------- */
static struct {
    i2c_port_t port;
    uint8_t    addr;
    float      accel_lsb_per_g;
    float      gyro_lsb_per_dps;
    float      accel_bias[3];  /* em g */
    float      gyro_bias[3];   /* em graus/s */
    bool       ready;
} s_dev;

/* Sensibilidades da tabela do datasheet (secoes 6.2 e 6.3). */
static const float ACCEL_SENS[4] = { 16384.0f, 8192.0f, 4096.0f, 2048.0f };
static const float GYRO_SENS[4]  = {   131.0f,   65.5f,   32.8f,   16.4f };

/* Junta dois bytes (MSB primeiro) em um inteiro de 16 bits com sinal. */
static inline int16_t be16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | p[1]);
}

esp_err_t mpu6050_who_am_i(uint8_t *out_id)
{
    if (out_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_bus_read_regs(s_dev.port, s_dev.addr, REG_WHO_AM_I, out_id, 1);
}

esp_err_t mpu6050_init(const mpu6050_config_t *cfg)
{
    if (cfg == NULL || cfg->sample_rate_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_dev, 0, sizeof(s_dev));
    s_dev.port = cfg->port;
    s_dev.addr = cfg->addr;

    /* 1) Reset por software e espera o chip religar. */
    esp_err_t err = i2c_bus_write_reg(s_dev.port, s_dev.addr,
                                      REG_PWR_MGMT_1, PWR_MGMT_1_DEVICE_RESET);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sem resposta em 0x%02X (%s) - confira SDA/SCL/3V3",
                 s_dev.addr, esp_err_to_name(err));
        return ESP_ERR_NOT_FOUND;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 2) Confere a identidade do chip. */
    uint8_t id = 0;
    err = mpu6050_who_am_i(&id);
    if (err != ESP_OK) {
        return err;
    }
    if (id != MPU6050_WHO_AM_I_VALUE) {
        /* Nao aborta: clones MPU6500/9250 usam o mesmo mapa basico. */
        ESP_LOGW(TAG, "WHO_AM_I = 0x%02X (esperado 0x%02X) - provavel clone", id,
                 MPU6050_WHO_AM_I_VALUE);
    }

    /* 3) Acorda o sensor e seleciona o PLL do giroscopio X como clock. */
    err = i2c_bus_write_reg(s_dev.port, s_dev.addr,
                            REG_PWR_MGMT_1, PWR_MGMT_1_CLK_PLL_XGYRO);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 4) Filtro passa-baixas digital. */
    err = i2c_bus_write_reg(s_dev.port, s_dev.addr, REG_CONFIG, (uint8_t)cfg->dlpf);
    if (err != ESP_OK) {
        return err;
    }

    /* 5) Taxa de amostragem: fs = gyro_rate / (1 + SMPLRT_DIV).
     *    gyro_rate = 8 kHz com DLPF desligado, 1 kHz com DLPF ligado. */
    const uint32_t gyro_rate = (cfg->dlpf == MPU6050_DLPF_260HZ) ? 8000U : 1000U;
    uint32_t div = gyro_rate / cfg->sample_rate_hz;
    if (div == 0) {
        div = 1;
    }
    if (div > 256) {
        div = 256;
    }
    err = i2c_bus_write_reg(s_dev.port, s_dev.addr, REG_SMPLRT_DIV, (uint8_t)(div - 1));
    if (err != ESP_OK) {
        return err;
    }

    /* 6) Fundos de escala (bits 4:3 de cada registrador). */
    err = i2c_bus_write_reg(s_dev.port, s_dev.addr,
                            REG_GYRO_CONFIG, (uint8_t)(cfg->gyro_fs << 3));
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_bus_write_reg(s_dev.port, s_dev.addr,
                            REG_ACCEL_CONFIG, (uint8_t)(cfg->accel_fs << 3));
    if (err != ESP_OK) {
        return err;
    }

    s_dev.accel_lsb_per_g  = ACCEL_SENS[cfg->accel_fs];
    s_dev.gyro_lsb_per_dps = GYRO_SENS[cfg->gyro_fs];
    s_dev.ready            = true;

    ESP_LOGI(TAG, "inicializado: accel=+/-%dg, gyro=+/-%ddps, fs=%u Hz, DLPF=%d",
             2 << cfg->accel_fs, 250 << cfg->gyro_fs,
             (unsigned)(gyro_rate / div), (int)cfg->dlpf);
    return ESP_OK;
}

esp_err_t mpu6050_read_raw(mpu6050_raw_t *out_raw)
{
    if (out_raw == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Leitura em rajada: ACCEL_X..GYRO_Z sao 14 bytes contiguos. Ler tudo de
     * uma vez evita misturar eixos de amostras diferentes. */
    uint8_t buf[14];
    esp_err_t err = i2c_bus_read_regs(s_dev.port, s_dev.addr,
                                      REG_ACCEL_XOUT_H, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    out_raw->accel_x = be16(&buf[0]);
    out_raw->accel_y = be16(&buf[2]);
    out_raw->accel_z = be16(&buf[4]);
    out_raw->temp    = be16(&buf[6]);
    out_raw->gyro_x  = be16(&buf[8]);
    out_raw->gyro_y  = be16(&buf[10]);
    out_raw->gyro_z  = be16(&buf[12]);
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_data_t *out_data)
{
    if (out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_dev.ready) {
        return ESP_ERR_INVALID_STATE;
    }

    mpu6050_raw_t raw;
    esp_err_t err = mpu6050_read_raw(&raw);
    if (err != ESP_OK) {
        return err;
    }

    out_data->accel_x = raw.accel_x / s_dev.accel_lsb_per_g - s_dev.accel_bias[0];
    out_data->accel_y = raw.accel_y / s_dev.accel_lsb_per_g - s_dev.accel_bias[1];
    out_data->accel_z = raw.accel_z / s_dev.accel_lsb_per_g - s_dev.accel_bias[2];

    out_data->gyro_x = raw.gyro_x / s_dev.gyro_lsb_per_dps - s_dev.gyro_bias[0];
    out_data->gyro_y = raw.gyro_y / s_dev.gyro_lsb_per_dps - s_dev.gyro_bias[1];
    out_data->gyro_z = raw.gyro_z / s_dev.gyro_lsb_per_dps - s_dev.gyro_bias[2];

    /* Datasheet, secao 4.18: T[C] = raw/340 + 36,53 */
    out_data->temp_c = raw.temp / 340.0f + 36.53f;
    return ESP_OK;
}

esp_err_t mpu6050_calibrate(uint16_t samples)
{
    if (!s_dev.ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Zera os offsets antigos para medir o bias absoluto. */
    memset(s_dev.accel_bias, 0, sizeof(s_dev.accel_bias));
    memset(s_dev.gyro_bias, 0, sizeof(s_dev.gyro_bias));

    double soma[6] = { 0 };
    uint16_t validas = 0;

    for (uint16_t i = 0; i < samples; i++) {
        mpu6050_raw_t raw;
        if (mpu6050_read_raw(&raw) == ESP_OK) {
            soma[0] += raw.accel_x;
            soma[1] += raw.accel_y;
            soma[2] += raw.accel_z;
            soma[3] += raw.gyro_x;
            soma[4] += raw.gyro_y;
            soma[5] += raw.gyro_z;
            validas++;
        }
        vTaskDelay(pdMS_TO_TICKS(3));
    }

    if (validas == 0) {
        return ESP_FAIL;
    }

    for (int i = 0; i < 3; i++) {
        s_dev.accel_bias[i] = (float)(soma[i] / validas) / s_dev.accel_lsb_per_g;
        s_dev.gyro_bias[i]  = (float)(soma[3 + i] / validas) / s_dev.gyro_lsb_per_dps;
    }

    /* Com o modulo parado e nivelado, Z deve marcar +1 g. O offset de Z e,
     * portanto, o desvio em relacao a 1 g, e nao a leitura inteira. */
    s_dev.accel_bias[2] -= 1.0f;

    ESP_LOGI(TAG, "offsets (%u amostras): accel=[%.4f %.4f %.4f] g  gyro=[%.3f %.3f %.3f] dps",
             validas,
             s_dev.accel_bias[0], s_dev.accel_bias[1], s_dev.accel_bias[2],
             s_dev.gyro_bias[0], s_dev.gyro_bias[1], s_dev.gyro_bias[2]);
    return ESP_OK;
}

float mpu6050_accel_lsb_per_g(void)  { return s_dev.accel_lsb_per_g; }
float mpu6050_gyro_lsb_per_dps(void) { return s_dev.gyro_lsb_per_dps; }
