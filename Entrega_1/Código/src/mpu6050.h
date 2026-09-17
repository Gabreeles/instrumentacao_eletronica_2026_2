/*
 * mpu6050.h - Driver em C puro para o acelerometro/giroscopio MPU6050.
 *
 * Acesso direto aos registradores via I2C, sem bibliotecas Arduino.
 * Datasheet: InvenSense MPU-6000/MPU-6050 Register Map, rev. 4.2.
 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

/* Endereco I2C de 7 bits: depende do pino AD0 do modulo.
 * AD0 em GND (ou solto no GY-521, que tem pull-down) -> 0x68
 * AD0 em 3V3                                          -> 0x69 */
#define MPU6050_ADDR_AD0_LOW   0x68
#define MPU6050_ADDR_AD0_HIGH  0x69

/* Valor esperado no registrador WHO_AM_I (0x75).
 * Clones baseados no MPU6500/MPU9250 podem devolver 0x70, 0x98 ou 0x12. */
#define MPU6050_WHO_AM_I_VALUE 0x68

/* Fundo de escala do acelerometro. */
typedef enum {
    MPU6050_ACCEL_FS_2G  = 0,  /* 16384 LSB/g - maior resolucao */
    MPU6050_ACCEL_FS_4G  = 1,  /*  8192 LSB/g */
    MPU6050_ACCEL_FS_8G  = 2,  /*  4096 LSB/g */
    MPU6050_ACCEL_FS_16G = 3,  /*  2048 LSB/g - maior faixa */
} mpu6050_accel_fs_t;

/* Fundo de escala do giroscopio. */
typedef enum {
    MPU6050_GYRO_FS_250DPS  = 0,  /* 131,0 LSB/(graus/s) */
    MPU6050_GYRO_FS_500DPS  = 1,  /*  65,5 LSB/(graus/s) */
    MPU6050_GYRO_FS_1000DPS = 2,  /*  32,8 LSB/(graus/s) */
    MPU6050_GYRO_FS_2000DPS = 3,  /*  16,4 LSB/(graus/s) */
} mpu6050_gyro_fs_t;

/* Filtro passa-baixas digital interno (registrador CONFIG, bits DLPF_CFG).
 * Reduz ruido de banda larga antes da amostragem, ao custo de atraso de grupo. */
typedef enum {
    MPU6050_DLPF_260HZ = 0,  /* praticamente sem filtro; taxa interna de 8 kHz */
    MPU6050_DLPF_184HZ = 1,
    MPU6050_DLPF_94HZ  = 2,
    MPU6050_DLPF_44HZ  = 3,  /* bom compromisso para medidas de vibracao/postura */
    MPU6050_DLPF_21HZ  = 4,
    MPU6050_DLPF_10HZ  = 5,
    MPU6050_DLPF_5HZ   = 6,
} mpu6050_dlpf_t;

/* Parametros de inicializacao. */
typedef struct {
    i2c_port_t         port;        /* barramento ja inicializado por i2c_bus_init() */
    uint8_t            addr;        /* MPU6050_ADDR_AD0_LOW ou _HIGH */
    mpu6050_accel_fs_t accel_fs;
    mpu6050_gyro_fs_t  gyro_fs;
    mpu6050_dlpf_t     dlpf;
    uint16_t           sample_rate_hz; /* taxa de amostragem desejada (4..1000 Hz) */
} mpu6050_config_t;

/* Amostra bruta, em contagens de 16 bits com sinal (complemento de dois). */
typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t temp;
    int16_t gyro_x, gyro_y, gyro_z;
} mpu6050_raw_t;

/* Amostra ja convertida para unidades de engenharia. */
typedef struct {
    float accel_x, accel_y, accel_z; /* g   (1 g = 9,80665 m/s^2) */
    float gyro_x,  gyro_y,  gyro_z;  /* graus/s */
    float temp_c;                    /* graus Celsius (sensor interno do chip) */
} mpu6050_data_t;

/* Tira o sensor do modo sleep e aplica a configuracao pedida.
 * Tambem valida o WHO_AM_I: retorna ESP_ERR_NOT_FOUND se o chip nao responder. */
esp_err_t mpu6050_init(const mpu6050_config_t *cfg);

/* Le o registrador WHO_AM_I (0x75). */
esp_err_t mpu6050_who_am_i(uint8_t *out_id);

/* Le os 14 bytes de dados (accel + temp + gyro) em uma unica transacao I2C,
 * garantindo que todos os eixos venham da mesma amostra. */
esp_err_t mpu6050_read_raw(mpu6050_raw_t *out_raw);

/* Igual a mpu6050_read_raw(), porem convertendo para g, graus/s e Celsius e
 * descontando o offset obtido em mpu6050_calibrate(). */
esp_err_t mpu6050_read(mpu6050_data_t *out_data);

/* Estima o offset (bias) com o sensor parado e nivelado, sobre uma media de
 * 'samples' leituras. O eixo Z do acelerometro e' corrigido para 1 g. */
esp_err_t mpu6050_calibrate(uint16_t samples);

/* Sensibilidades em uso, para conferencia/relatorio. */
float mpu6050_accel_lsb_per_g(void);
float mpu6050_gyro_lsb_per_dps(void);

#endif /* MPU6050_H */
