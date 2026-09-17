/*
 * i2c_bus.h - Wrapper simples sobre o driver I2C do ESP-IDF.
 *
 * Isola a configuracao do barramento (pinos, clock, pull-ups) do codigo do
 * sensor, de modo que outros dispositivos I2C possam compartilhar o mesmo bus.
 */
#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

/* Inicializa o barramento I2C como mestre.
 *
 * port      : I2C_NUM_0 ou I2C_NUM_1
 * sda_gpio  : GPIO usado como SDA (21 no NodeMCU-32S)
 * scl_gpio  : GPIO usado como SCL (22 no NodeMCU-32S)
 * freq_hz   : clock do barramento (100000 = modo padrao, 400000 = fast mode)
 * pullup_en : habilita os pull-ups internos (~45 kOhm). O modulo GY-521 ja
 *             possui pull-ups de 2,2 kOhm, entao normalmente pode ser false.
 */
esp_err_t i2c_bus_init(i2c_port_t port, int sda_gpio, int scl_gpio,
                       uint32_t freq_hz, bool pullup_en);

/* Escreve um byte em um registrador do escravo. */
esp_err_t i2c_bus_write_reg(i2c_port_t port, uint8_t dev_addr,
                            uint8_t reg, uint8_t value);

/* Le 'len' bytes consecutivos a partir de 'reg' (leitura com repeated start). */
esp_err_t i2c_bus_read_regs(i2c_port_t port, uint8_t dev_addr,
                            uint8_t reg, uint8_t *buf, size_t len);

/* Varre os enderecos 0x08..0x77 e imprime os dispositivos que responderam.
 * Util para confirmar a fiacao antes de falar com o sensor. */
void i2c_bus_scan(i2c_port_t port);

#endif /* I2C_BUS_H */
