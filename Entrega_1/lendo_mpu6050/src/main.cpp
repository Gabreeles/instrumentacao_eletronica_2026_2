#include <Arduino.h>
#include <Wire.h>
#include <MPU6050.h>

#define SDA_PIN 21
#define SCL_PIN 22

// na faixa de +-2g o sensor entrega 16384 contagens por g
#define LSB_POR_G 16384.0

MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin(SDA_PIN, SCL_PIN);
  mpu.initialize();
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);

  Serial.println("t_ms,ax_g,ay_g,az_g");
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  Serial.print(millis());
  Serial.print(",");
  Serial.print(ax / LSB_POR_G, 4);
  Serial.print(",");
  Serial.print(ay / LSB_POR_G, 4);
  Serial.print(",");
  Serial.println(az / LSB_POR_G, 4);

  delay(50);
}
