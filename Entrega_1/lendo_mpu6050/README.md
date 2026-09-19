# Leitura do acelerômetro MPU6050 — Entrega 1

Projeto mínimo para o item 3 da Entrega 1 de Instrumentação Eletrônica: ler o
acelerômetro do MPU6050 e registrar os três eixos **em unidades de gravidade
(g)**, para montar o gráfico do sensor nas posições alinhadas com o vetor da
gravidade.

O firmware usa só a biblioteca `MPU6050` e imprime CSV na serial. Nada de
giroscópio, calibração ou filtro — só a leitura crua do acelerômetro.

---

## 1. Hardware e ligação

| Item | Observação |
|---|---|
| ESP32 DevKit v1 | `board = esp32doit-devkit-v1` |
| Módulo MPU6050 (GY-521) | já traz regulador e pull-ups no I2C |
| Cabo micro-USB **de dados** | cabo só de carga não cria a porta COM |

| MPU6050 | ESP32 DevKit v1 | Função |
|---|---|---|
| `VCC` | `3V3` | alimentação |
| `GND` | `GND` | referência comum (obrigatório) |
| `SDA` | `GPIO21` | dado do I2C |
| `SCL` | `GPIO22` | clock do I2C |
| `AD0` | `GND` | endereço `0x68` |

Alimente pelo **3V3**, não pelo 5V: os pull-ups do módulo puxam SDA/SCL até a
tensão do VCC, e os GPIOs do ESP32 não são 5 V tolerantes.

---

## 2. Estrutura

```
lendo_mpu6050/
├── platformio.ini   # placa, biblioteca e baud do monitor
├── src/
│   └── main.cpp     # ~55 linhas: inicializa o sensor e imprime CSV
├── capturar.py      # grava o CSV lendo a porta serial
├── plot_dados.py    # gera o gráfico a partir do CSV
├── explicacao_codigo.tex  # documento que explica o código bloco a bloco
└── README.md
```

---

## 3. Instalando o PlatformIO

Uma das duas opções basta.

**a) Extensão do VS Code (recomendado)**

1. Instale o [VS Code](https://code.visualstudio.com/).
2. Extensões (`Ctrl+Shift+X`) → busque **PlatformIO IDE** → *Install*.
3. Reinicie e abra **esta pasta** (a que contém o `platformio.ini`).
4. Use os ícones da barra inferior: ✓ compilar, → gravar, 🔌 monitor serial.

**b) Linha de comando**

```powershell
pip install -U platformio
pio --version
```

> **Primeira compilação demora.** O PlatformIO baixa sozinho a plataforma
> `espressif32`, o core Arduino e o toolchain xtensa (centenas de MB). Depois
> disso o build leva poucos segundos.

---

## 4. Compilar, gravar e monitorar

Rode os comandos dentro da pasta do projeto.

```powershell
# conferir em qual porta a placa apareceu
pio device list

# compilar
pio run

# gravar na ESP32
pio run --target upload

# abrir o monitor serial (115200 bps)
pio device monitor

# gravar e já abrir o monitor, de uma vez
pio run --target upload --target monitor

# apagar os arquivos compilados
pio run --target clean
```

Para sair do monitor serial: **Ctrl + C**.

Se o PlatformIO não achar a porta sozinho, fixe no `platformio.ini`:

```ini
upload_port  = COM11
monitor_port = COM11
```

Se a gravação falhar com `Wrong boot mode detected`: rode o upload e, quando
aparecer `Connecting........`, segure o botão **BOOT** até começar o
`Writing at 0x...`.

---

## 5. Saída

```
# MPU6050 ok | accel = +-2g | 16384 LSB/g | 20 Hz
t_ms,ax_g,ay_g,az_g
1050,-0.0032,0.0041,0.9987
1100,-0.0028,0.0039,0.9991
```

| Coluna | Unidade | Descrição |
|---|---|---|
| `t_ms` | ms | tempo desde o reset da placa |
| `ax_g`, `ay_g`, `az_g` | g | aceleração em cada eixo (1 g = 9,80665 m/s²) |

A primeira linha é o cabeçalho; o `plot_dados.py` a descarta sozinho.

---

## 6. O ensaio do item 3

O enunciado pede a leitura **com o sensor em posições coincidentes com o vetor
aceleração da gravidade**. Na prática são seis posições, uma para cada sentido
dos três eixos. Com o sensor parado, o eixo que estiver na vertical marca ±1 g
e os outros dois ficam perto de zero:

| Posição do módulo | X (g) | Y (g) | Z (g) |
|---|---|---|---|
| deitado, componentes para cima | 0 | 0 | **+1** |
| deitado, de cabeça para baixo | 0 | 0 | **−1** |
| apoiado na borda, X para baixo | **+1** | 0 | 0 |
| borda oposta, X para cima | **−1** | 0 | 0 |
| apoiado na borda, Y para baixo | 0 | **+1** | 0 |
| borda oposta, Y para cima | 0 | **−1** | 0 |

O sinal exato depende de como o módulo está virado; a serigrafia da placa
indica o sentido positivo dos eixos.

**Capturando os dados:**

```powershell
python capturar.py
```

O script acha a porta sozinho, reinicia a placa e grava tudo em `dados.csv`.
Deixe rodando, passe pelas seis posições segurando cada uma por uns 5 segundos,
e encerre com **Ctrl + C**.

Para fixar a porta ou o nome do arquivo:

```powershell
python capturar.py COM3 ensaio.csv
```

> **Por que não `pio device monitor > dados.csv`?** O PlatformIO recusa o
> redirecionamento (`requires an interactive terminal on stdin`), e quando
> funcionava o `>` do PowerShell gravava o arquivo em UTF-16 com os avisos do
> PlatformIO no meio. O `capturar.py` usa a pyserial direto e grava UTF-8 limpo.

**Gerando o gráfico:**

```powershell
pip install pyserial matplotlib
python plot_dados.py dados.csv
```

O script salva `grafico.png` com os três eixos sobrepostos em função do tempo e
linhas de referência em −1, 0 e +1 g. Os patamares do gráfico são exatamente o
que a tabela acima prevê.

---

## 7. Faixa de medição

O código fixa ±2 g, que é a melhor resolução e o suficiente para medir a
gravidade. Para trocar, mude a constante no `setup()` e o `LSB_POR_G` no topo do
`src/main.cpp`:

| Faixa | Constante | LSB/g | Resolução |
|---|---|---|---|
| ±2 g | `MPU6050_ACCEL_FS_2` | 16384 | 61 µg |
| ±4 g | `MPU6050_ACCEL_FS_4` | 8192 | 122 µg |
| ±8 g | `MPU6050_ACCEL_FS_8` | 4096 | 244 µg |
| ±16 g | `MPU6050_ACCEL_FS_16` | 2048 | 488 µg |

---

## 8. Problemas frequentes

| Sintoma | Causa provável |
|---|---|
| Só sai o cabeçalho, sem nenhuma amostra | SDA e SCL trocados, jumper solto, ou falta de GND comum |
| Todos os eixos em 0,00 ou em −1,00 | o sensor não respondeu no endereço `0x68`. Aterre o pino AD0 |
| `ModuleNotFoundError: matplotlib` | o terminal do VS Code usa o Python do PlatformIO. Rode `python -m pip install matplotlib` nesse mesmo terminal |
| Monitor mostra caracteres estranhos | baud errado — tem que ser 115200 |
| Nada aparece no monitor | segure o BOOT durante a gravação; confira se é cabo de dados |
| `boot:0x3 (DOWNLOAD_BOOT)` e `waiting for download` | o BOOT ficou pressionado no reset. Solte-o e aperte **EN** |
| Leituras congeladas num valor só | alimentação em 5V ou GND não comum |
| `Obsolete PIO Core v6.1.19` | há dois PlatformIO Core instalados na máquina (o do `pip` e o da extensão do VS Code). Não impede a compilação |
| Z marca ~0,98 em vez de 1,00 | normal: é o erro de offset/ganho de fábrica, sem calibração |

---

## 9. Referências

- [MPU-6000/MPU-6050 Product Specification, rev. 3.4](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf) — InvenSense/TDK: sensibilidades, ruído, faixa de operação
- [MPU-6000/MPU-6050 Register Map and Descriptions, rev. 4.2](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf) — InvenSense/TDK
- [Biblioteca MPU6050 — Electronic Cats](https://github.com/ElectronicCats/mpu6050)
- [PlatformIO — Espressif 32](https://docs.platformio.org/en/latest/platforms/espressif32.html)
