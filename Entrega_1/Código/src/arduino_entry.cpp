/*
 * arduino_entry.cpp - Ponte entre o core Arduino e a aplicacao em C.
 *
 * Este arquivo SO e compilado no ambiente "nodemcu-32s" (framework arduino).
 * No ambiente "nodemcu-32s-idf" quem chama mpu_app_start() e o app_main()
 * definido no final de main.c.
 *
 * Existe porque o core Arduino ja define app_main() e espera encontrar
 * setup()/loop() com ligacao C++. Todo o resto do firmware continua em C puro.
 */

#include <Arduino.h>
#include "esp_log.h"

extern "C" void mpu_app_start(void);

void setup(void)
{
    Serial.begin(115200);
    delay(300);  /* deixa o terminal do PC enumerar antes do primeiro log */

    /* O core Arduino sobe com o nivel de log em ERROR: liberamos INFO para que
     * a varredura do I2C e os offsets aparecam no monitor serial. */
    esp_log_level_set("*", ESP_LOG_INFO);

    mpu_app_start();  /* nao retorna: contem o laco de aquisicao */
}

void loop(void)
{
    /* Inalcancavel - mpu_app_start() so retorna se o sensor nao responder. */
    delay(1000);
}
