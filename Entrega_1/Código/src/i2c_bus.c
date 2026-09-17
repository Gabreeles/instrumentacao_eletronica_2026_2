#include "i2c_bus.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "i2c_bus";

/* Timeout generoso: o MPU6050 responde em poucos microssegundos, mas um
 * barramento mal conectado trava a transacao ate estourar esse tempo. */
#define I2C_TIMEOUT_MS 100

esp_err_t i2c_bus_init(i2c_port_t port, int sda_gpio, int scl_gpio,
                       uint32_t freq_hz, bool pullup_en)
{
    const i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = sda_gpio,
        .scl_io_num       = scl_gpio,
        .sda_pullup_en    = pullup_en ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .scl_pullup_en    = pullup_en ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .master.clk_speed = freq_hz,
    };

    esp_err_t err = i2c_param_config(port, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config falhou: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(port, cfg.mode, 0 /* rx buf */, 0 /* tx buf */, 0 /* flags */);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install falhou: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C%d pronto (SDA=GPIO%d, SCL=GPIO%d, %lu Hz)",
             (int)port, sda_gpio, scl_gpio, (unsigned long)freq_hz);
    return ESP_OK;
}

esp_err_t i2c_bus_write_reg(i2c_port_t port, uint8_t dev_addr,
                            uint8_t reg, uint8_t value)
{
    const uint8_t payload[2] = { reg, value };
    return i2c_master_write_to_device(port, dev_addr, payload, sizeof(payload),
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t i2c_bus_read_regs(i2c_port_t port, uint8_t dev_addr,
                            uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(port, dev_addr, &reg, 1, buf, len,
                                        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

void i2c_bus_scan(i2c_port_t port)
{
    ESP_LOGI(TAG, "Varrendo o barramento I2C...");

    int encontrados = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true /* espera ACK */);
        i2c_master_stop(cmd);

        esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "  dispositivo encontrado em 0x%02X", addr);
            encontrados++;
        }
    }

    if (encontrados == 0) {
        ESP_LOGW(TAG, "  nenhum dispositivo respondeu - confira a fiacao e o 3V3");
    }
}
