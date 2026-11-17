#include <Arduino.h>
#include <Wire.h>
#include <MPU6050.h>

#define SDA_PIN 21
#define SCL_PIN 22

// na faixa de +-2g o sensor entrega 16384 contagens por g
#define LSB_POR_G 16384.0

// ---------------------------------------------------------------
// Calibracao: A_saida = Ganho * A_ref + A_offset   (eq. 1 do roteiro)
// Os valores saem do calibrar.py. Antes de calibrar, deixe
// ganho = 1 e offset = 0.
// ---------------------------------------------------------------
#define GANHO_X  1.0
#define GANHO_Y  1.0
#define GANHO_Z  1.0

#define OFFSET_X 0.0
#define OFFSET_Y 0.0
#define OFFSET_Z 0.0

MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin(SDA_PIN, SCL_PIN);
  mpu.initialize();
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);

  Serial.println("t_ms,ax_g,ay_g,az_g,angulo_graus");
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // 1) leitura bruta em g (e o que o calibrar.py usa)
  float gx = ax / LSB_POR_G;
  float gy = ay / LSB_POR_G;
  float gz = az / LSB_POR_G;

  // 2) leitura corrigida: isola A_ref na eq. 1
  float cx = (gx - OFFSET_X) / GANHO_X;
  float cy = (gy - OFFSET_Y) / GANHO_Y;
  float cz = (gz - OFFSET_Z) / GANHO_Z;

  // 3) angulo de inclinacao
  // O eixo X do sensor fica alinhado com o eixo de giro do berco,
  // entao a gravidade so muda nos eixos Y e Z.
  // Com o braco em 0 graus, Y aponta para cima: cy = 1 g e cz = 0.
  // Girando o braco de theta: cy = cos(theta) e cz = sin(theta).
  float angulo = atan2(cz, cy) * 180.0 / PI;

  // atan2 devolve de -180 a 180 e salta de sinal em 180 graus.
  // Passando a faixa para -90 a 270, o giro de 0 a 180 fica continuo.
  if (angulo < -90) {
    angulo = angulo + 360;
  }

  Serial.print(millis());
  Serial.print(",");
  Serial.print(gx, 4);
  Serial.print(",");
  Serial.print(gy, 4);
  Serial.print(",");
  Serial.print(gz, 4);
  Serial.print(",");
  Serial.println(angulo, 2);

  delay(50);
}
