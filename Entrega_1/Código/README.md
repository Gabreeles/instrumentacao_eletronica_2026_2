# Leitura do MPU6050 com ESP32-S NodeMCU

Firmware em **C** (arquivos `.c`, compilado pelo PlatformIO) que lê o
acelerômetro/giroscópio **MPU6050** por I2C e envia as medidas em CSV pela porta
serial.

O código fala diretamente com os registradores do sensor — nada de bibliotecas
MPU6050 prontas — para que cada etapa da cadeia de medição (fundo de escala,
filtro anti-aliasing, taxa de amostragem, offset) fique explícita.

---

## 1. Hardware necessário

| Item | Observação |
|---|---|
| ESP32-S NodeMCU (ESP32-WROOM-32, 38 pinos) | conversor USB-serial CP2102 ou CH340 |
| Módulo MPU6050 (GY-521) | já traz regulador 3V3 e pull-ups de 2,2 kΩ no I2C |
| Cabo micro-USB **de dados** | cabo só de carga não enumera a porta COM |
| 4 jumpers macho-fêmea | |

---

## 2. Ligações

A comunicação é I2C, então bastam quatro fios (mais o AD0, que define o
endereço).

| MPU6050 (GY-521) | ESP32-S NodeMCU | Função |
|---|---|---|
| `VCC` | `3V3` | alimentação |
| `GND` | `GND` | referência comum (**obrigatório**) |
| `SCL` | `GPIO22` (D22) | clock do I2C |
| `SDA` | `GPIO21` (D21) | dado do I2C |
| `AD0` | `GND` | seleciona o endereço `0x68` |
| `XDA`, `XCL`, `INT` | não conectar | barramento auxiliar / interrupção |

```
   ESP32-S NodeMCU                 MPU6050 (GY-521)
  ┌───────────────┐               ┌───────────────┐
  │           3V3 ├───────────────┤ VCC           │
  │           GND ├───────┬───────┤ GND           │
  │  GPIO22 (SCL) ├───────┼───────┤ SCL           │
  │  GPIO21 (SDA) ├───────┼───────┤ SDA           │
  │               │       └───────┤ AD0           │
  │           USB │               │ XDA XCL INT   │ (livres)
  └───────┬───────┘               └───────────────┘
          │
      ao computador
```

**Pontos de atenção**

- Alimente pelo **3V3**. O GY-521 aceita 5V no VCC (tem regulador), mas as
  linhas SDA/SCL são puxadas pelos pull-ups até a tensão do VCC — em 5V isso
  aplica 5 V nos GPIOs do ESP32, que **não são 5 V tolerantes**.
- `AD0` em GND → endereço **0x68**. `AD0` em 3V3 → **0x69**; nesse caso troque
  `MPU_ADDR` em [src/main.c](src/main.c) para `MPU6050_ADDR_AD0_HIGH`.
- O módulo já tem pull-ups. Não precisa de resistores externos, e
  `USE_INTERNAL_PULLUP` fica em `false`.
- Fios longos (> 20 cm) ou protoboard ruim: baixe `I2C_FREQ_HZ` de `400000`
  para `100000`.

---

## 3. Ambiente de compilação

O [platformio.ini](platformio.ini) define **dois ambientes que compilam os
mesmos arquivos `.c`**:

| Ambiente | Framework | Quando usar |
|---|---|---|
| `nodemcu-32s` (**padrão**) | `arduino` | é o que funciona nesta pasta — veja a ressalva abaixo |
| `nodemcu-32s-idf` | `espidf` | C puro sem o core Arduino; exige caminho sem espaços/acentos |

> **Por que o padrão não é o ESP-IDF?**
> O build do ESP-IDF aborta com `Error: Detected a whitespace character in
> project paths.` — o caminho deste trabalho tem espaços (`OneDrive - unb.br`,
> `Faculdade FGA (UnB)`, `8° Semestre`...) e acentos. Para usar o ambiente
> `nodemcu-32s-idf`, copie a pasta do projeto para um caminho curto e sem
> espaços, por exemplo `C:\esp\mpu6050`, e rode `pio run -e nodemcu-32s-idf`
> de lá.
>
> O ambiente Arduino não tem essa restrição, e o código de aplicação continua
> sendo o mesmo C: o core entra apenas como camada de inicialização, pelo
> arquivo [src/arduino_entry.cpp](src/arduino_entry.cpp) (~20 linhas).

### Instalação

**a) VS Code (recomendado)**
1. Instale o [VS Code](https://code.visualstudio.com/).
2. Extensões → busque **PlatformIO IDE** → *Install*.
3. `File > Open Folder…` e abra **esta pasta** (a que contém o `platformio.ini`).
4. Use os ícones na barra inferior: ✓ (Build), → (Upload), 🔌 (Serial Monitor).

**b) Linha de comando** — abra o PowerShell nesta pasta e use os comandos da
seção 4.

> **Você tem dois PlatformIO Core instalados** (6.1.19 e 6.2.0), e o `pio` do
> PATH é o 6.1.19, instalado sobre o Python da Microsoft Store. Isso já causou
> uma falha de ambiente virtual no build do ESP-IDF. Se quiser limpar, remova o
> Core antigo com:
> ```powershell
> python -m pip uninstall platformio
> ```
> e use o Core que vem com a extensão do VS Code (`~/.platformio/penv`). Para o
> ambiente Arduino isso não é impeditivo.

> **Primeira compilação demora.** O PlatformIO baixa a plataforma
> `espressif32`, o core e o toolchain xtensa (centenas de MB). Só acontece uma
> vez.

---

## 4. Compilar, gravar e monitorar

Sua placa foi detectada em **COM11** (Silicon Labs CP210x — driver já
instalado).

```powershell
# 1) Conferir a porta
pio device list

# 2) Compilar
pio run

# 3) Gravar na ESP32
pio run --target upload

# 4) Abrir o monitor serial (115200 bps)
pio device monitor

# Gravar e já abrir o monitor, em um comando só
pio run --target upload --target monitor

# Apagar a flash inteira (útil se a placa estiver com firmware antigo travado)
pio run --target erase
```

Para sair do monitor serial: **Ctrl + ]**.

Se o PlatformIO não achar a porta sozinho, descomente no
[platformio.ini](platformio.ini), na seção `[env]`:

```ini
upload_port  = COM11
monitor_port = COM11
```

### Se aparecer `Failed to connect to ESP32: ... Wrong boot mode detected`

Algumas placas NodeMCU não entram em modo de gravação sozinhas:

1. Rode `pio run -t upload`.
2. Quando aparecer `Connecting........`, **segure o botão BOOT**.
3. Solte quando começar a escrever (`Writing at 0x...`).

Se a gravação falhar no meio ou o USB for instável, reduza a velocidade no
`platformio.ini`: `upload_speed = 460800`.

---

## 5. Saída esperada

Nos primeiros segundos o firmware imprime o log de inicialização (varredura do
barramento, `WHO_AM_I`, offsets) e depois passa a emitir CSV contínuo:

```
I (312) i2c_bus: I2C0 pronto (SDA=GPIO21, SCL=GPIO22, 400000 Hz)
I (318) i2c_bus: Varrendo o barramento I2C...
I (330) i2c_bus:   dispositivo encontrado em 0x68
I (440) mpu6050: inicializado: accel=+/-2g, gyro=+/-250dps, fs=100 Hz, DLPF=3
I (445) main: WHO_AM_I = 0x68 | sensibilidades: 16384 LSB/g, 131.0 LSB/(graus/s)
W (450) main: Calibrando: deixe o sensor parado e nivelado por ~2 s...
I (2100) mpu6050: offsets (500 amostras): accel=[0.0121 -0.0068 0.0043] g  gyro=[-1.842 0.377 0.915] dps

# fs=100 Hz | accel=+/-2g | gyro=+/-250dps | DLPF=44Hz
t_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,a_res_ms2,roll_deg,pitch_deg,temp_c
10,-0.0032,0.0041,0.9987,0.012,-0.037,0.004,9.794,0.24,0.18,31.42
20,-0.0028,0.0039,0.9991,0.008,-0.041,0.011,9.798,0.23,0.18,31.42
```

**Colunas**

| Coluna | Unidade | Descrição |
|---|---|---|
| `t_ms` | ms | tempo desde o início da aquisição |
| `ax_g`, `ay_g`, `az_g` | g | aceleração nos três eixos (1 g = 9,80665 m/s²) |
| `gx_dps`, `gy_dps`, `gz_dps` | °/s | velocidade angular |
| `a_res_ms2` | m/s² | módulo do vetor aceleração (parado ≈ 9,81) |
| `roll_deg`, `pitch_deg` | ° | inclinação estimada por filtro complementar |
| `temp_c` | °C | sensor de temperatura interno do chip |

**Teste rápido de sanidade:** com a placa parada sobre a mesa, `az_g ≈ 1,00`,
`ax_g` e `ay_g` ≈ 0 e os três giros ≈ 0. Virando o módulo de cabeça para baixo,
`az_g ≈ -1,00`.

### Gravando os dados para análise

```powershell
pio device monitor --quiet > dados.csv
```

Deixe rodando durante o ensaio e encerre com **Ctrl + ]**. O arquivo abre
direto no Excel, Octave ou pandas (as linhas iniciadas por `#` são comentário).

---

## 6. Estrutura do projeto

```
Código/
├── platformio.ini          # os dois ambientes (arduino e espidf)
├── CMakeLists.txt          # projeto ESP-IDF (usado só pelo env espidf)
├── sdkconfig.defaults      # baud do console, tick de 1 ms (usado só pelo env espidf)
└── src/
    ├── CMakeLists.txt      # lista dos .c do componente (env espidf)
    ├── i2c_bus.h/.c        # configuração do barramento I2C + scanner
    ├── mpu6050.h/.c        # driver do sensor (registradores, escalas, offsets)
    ├── main.c              # aplicação: inicializa, calibra, amostra e imprime
    └── arduino_entry.cpp   # ponte setup()/loop() → mpu_app_start() (env arduino)
```

Toda a lógica de medição está nos `.c`. O `arduino_entry.cpp` existe apenas
porque o core Arduino já define `app_main()` e exige `setup()`/`loop()` com
ligação C++; no ambiente ESP-IDF esse arquivo é ignorado e a entrada é o
`app_main()` no fim de [src/main.c](src/main.c).

Pastas geradas na compilação (`.pio/`, `sdkconfig.*`) não precisam ser
versionadas.

---

## 7. Ajustes comuns

Tudo o que costuma mudar entre ensaios está no topo de [src/main.c](src/main.c):

| Constante | Padrão | Para que serve |
|---|---|---|
| `PIN_SDA` / `PIN_SCL` | 21 / 22 | mudar os pinos do I2C |
| `I2C_FREQ_HZ` | 400000 | reduza para 100000 se houver erro de comunicação |
| `MPU_ADDR` | `..._AD0_LOW` (0x68) | troque se ligar AD0 em 3V3 |
| `SAMPLE_RATE_HZ` | 100 | taxa de amostragem (4 a 1000 Hz) |
| `CALIB_SAMPLES` | 500 | número de amostras da estimativa de offset |
| `ALPHA` | 0.98 | peso do giroscópio no filtro complementar |

E na struct `cfg`, dentro de `mpu_app_start()`:

- `accel_fs`: `MPU6050_ACCEL_FS_2G` … `_16G`. Use ±2 g para inclinação
  (resolução de 61 µg/LSB); use ±8 g ou ±16 g para impacto e vibração forte.
- `gyro_fs`: `MPU6050_GYRO_FS_250DPS` … `_2000DPS`.
- `dlpf`: filtro passa-baixas interno. Pelo critério de Nyquist, a banda do
  filtro deve ficar abaixo de `SAMPLE_RATE_HZ / 2` — com 100 Hz de amostragem,
  `MPU6050_DLPF_44HZ` (padrão) ou `MPU6050_DLPF_21HZ` são as escolhas coerentes.

---

## 8. Problemas frequentes

| Sintoma | Causa provável |
|---|---|
| `Detected a whitespace character in project paths` | build do ESP-IDF em caminho com espaços — use o ambiente padrão (`pio run`) ou copie o projeto para `C:\esp\mpu6050` |
| `Failed to create a proper virtual environment. Missing the pip binary!` | ESP-IDF sobre o Python da Microsoft Store — mesma solução: use o ambiente Arduino, ou instale o Python do python.org |
| `Obsolete PIO Core v6.1.19 is used` | dois PlatformIO Core instalados (veja a seção 3) |
| `nenhum dispositivo respondeu` na varredura | VCC/GND trocados, jumper solto, ou SDA e SCL invertidos |
| Scanner acha `0x69` em vez de `0x68` | AD0 está em 3V3 (ou solto e flutuando) — aterre-o ou troque `MPU_ADDR` |
| `WHO_AM_I = 0x98 / 0x70` | módulo é um clone (MPU6500/MPU9250); o código segue funcionando |
| Leituras congeladas em um valor | alimentação em 5V, ou falta de GND comum entre as placas |
| `az_g` fica em ~0,5 g parado | calibração feita com a placa inclinada — reinicie nivelado |
| Giroscópio marca ~2 °/s parado | normal antes da calibração; o offset é descontado depois dela |
| Monitor mostra caracteres estranhos | baud errado: precisa ser 115200 (`monitor_speed` no `platformio.ini`) |

---

## 9. Referências

- [MPU-6000/MPU-6050 Register Map and Descriptions, rev. 4.2](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf) — InvenSense/TDK
- [MPU-6050 Product Specification, rev. 3.4](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf) — sensibilidades, ruído, faixa de operação
- [ESP-IDF — I2C driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)
- [PlatformIO — Espressif 32](https://docs.platformio.org/en/latest/platforms/espressif32.html)
