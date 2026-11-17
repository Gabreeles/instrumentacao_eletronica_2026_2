# Calibração e ângulo de inclinação com o MPU6050 — Entrega 2

Continuação do `Entrega_1/lendo_mpu6050`, com a mesma ligação, placa e
biblioteca. O que muda:

1. **Calibração estática** (item 2.1 do roteiro): ganho e offset de cada eixo.
2. **Ângulo de inclinação** (item 2.2): calculado no ESP32 e mostrado na serial.
3. **Gráfico de erro** (item 3c): ângulo do transferidor − ângulo medido.

```
medindo_angulo/
├── platformio.ini   # igual ao da Entrega 1 + build fora do OneDrive
├── src/main.cpp     # leitura, correção pela calibração e ângulo
├── capturar.py      # grava a serial num CSV (pode parar sozinho após N leituras)
├── calibrar.py      # 6 CSVs -> ganho e offset de cada eixo
└── erro_angulo.py   # CSVs por ângulo -> tabela e grafico_erro.png
```

A ligação do MPU6050 ao ESP32 é a mesma da Entrega 1 (SDA no GPIO21, SCL no
GPIO22, VCC no 3V3).

Saída da serial (20 leituras por segundo):

```
t_ms,ax_g,ay_g,az_g,angulo_graus
2039,0.0088,0.9971,0.0412,2.37
```

`ax_g`, `ay_g` e `az_g` são as leituras **brutas** em g, usadas na calibração.
O `angulo_graus` já sai calculado com os valores **corrigidos**.

---

## Passo 1 — Gravar o firmware

```powershell
pio run --target upload
```

Na primeira vez, deixe `GANHO = 1` e `OFFSET = 0` no topo do `src/main.cpp`.

> O `platformio.ini` manda a pasta `.pio` para `%USERPROFILE%\.pio_medindo_angulo`.
> Dentro do OneDrive o caminho passa de 260 caracteres e a instalação da
> biblioteca falha.

## Passo 2 — Calibração (6 posições, 200 leituras cada)

O roteiro pede no mínimo 100 leituras por posição; 200 leituras = 10 s.
Apoie o sensor numa superfície nivelada (confira com um nível) e rode um
comando para cada posição:

```powershell
python capturar.py COM3 calibracao/x_pos.csv 200   # eixo X para cima
python capturar.py COM3 calibracao/x_neg.csv 200   # eixo X para baixo
python capturar.py COM3 calibracao/y_pos.csv 200
python capturar.py COM3 calibracao/y_neg.csv 200
python capturar.py COM3 calibracao/z_pos.csv 200   # componentes para cima
python capturar.py COM3 calibracao/z_neg.csv 200
```

Depois:

```powershell
python calibrar.py
```

Ele mostra a média de cada posição e resolve, para cada eixo, o sistema da
eq. 2 do roteiro:

```
média(+1 g) = Ganho · (+1) + Offset
média(−1 g) = Ganho · (−1) + Offset

subtraindo:  Ganho  = (média(+1 g) − média(−1 g)) / 2
somando:     Offset = (média(+1 g) + média(−1 g)) / 2
```

A saída já traz as linhas `#define` para colar no `main.cpp` e as linhas da
Tabela II do relatório. Cole os valores e grave o firmware de novo.

## Passo 3 — Conferir a montagem no berço

O cálculo supõe que o **eixo X do sensor está alinhado com o eixo de giro** do
berço e que, com o braço em 0°, **o eixo Y aponta para cima**. Com o braço em 0°,
abra o monitor (`pio device monitor`) e confira:

- `ay_g` perto de +1 e `angulo_graus` perto de 0 → está certo;
- girando o braço no sentido positivo do transferidor, o ângulo deve **crescer**.
  Se diminuir, troque `atan2(cz, cy)` por `atan2(-cz, cy)`;
- se for outro eixo que marca +1 g em 0°, troque as letras no `atan2`
  (o eixo que aponta para cima vai no segundo argumento).

## Passo 4 — Ensaio de erro

Para cada ângulo do transferidor (por exemplo, de 15 em 15°, de 0 a 180°),
trave o braço e capture 100 leituras. O número no nome do arquivo é o ângulo de
referência:

```powershell
python capturar.py COM3 angulos/ang_0.csv 100
python capturar.py COM3 angulos/ang_15.csv 100
python capturar.py COM3 angulos/ang_30.csv 100
...
python capturar.py COM3 angulos/ang_180.csv 100
```

Depois:

```powershell
python erro_angulo.py
```

Ele imprime a tabela (referência, medido, erro), o erro médio e o maior erro, e
salva o `grafico_erro.png`. Copie o gráfico para
`Entrega_2/Relatorio_IEEE/Figuras/`.

## Passo 5 — Demonstração

```powershell
pio device monitor
```

Mexa o braço; a última coluna é o ângulo em graus.

---

## Como o ângulo é calculado

Parado, o acelerômetro só mede a gravidade (1 g). Como o eixo X está no eixo
de giro, ele não muda. A gravidade gira apenas no plano Y–Z do sensor:

```
        Y (para cima em 0°)
        ^
        |   / g
        |  /
        | / θ
        |/________> Z
```

- em 0°: `cy = 1`, `cz = 0`
- girando de θ: `cy = cos θ`, `cz = sen θ`
- logo **θ = atan2(cz, cy)**

O `atan2` usa os dois sinais e cobre a volta inteira. Ele devolve valores de −180°
a 180°; o firmware soma 360° quando o valor fica abaixo de −90°, para que a
faixa de 0 a 180° do transferidor não tenha salto.

Referências do método:

- M. Pedley, *Tilt Sensing Using a Three-Axis Accelerometer*, Freescale
  Semiconductor, AN3461, rev. 6, 2013.
- C. J. Fisher, *Using an Accelerometer for Inclination Sensing*, Analog
  Devices, AN-1057, 2010.

---

## Roteiro de 10 minutos para a apresentação

| Tempo | O que mostrar |
|---|---|
| 1 min | Objetivo: medir inclinação com o acelerômetro e comparar com o transferidor |
| 1 min | Montagem: ESP32 + MPU6050 (I²C) e a base com o berço |
| 2 min | Calibração: 6 posições, eq. 1 e 2, tabela de ganho e offset |
| 2 min | Ângulo: desenho Y–Z acima e `θ = atan2(cz, cy)` |
| 2 min | Gráfico de erro e discussão (onde o erro é maior e por quê) |
| 2 min | Demonstração ao vivo: girar o braço e ler o ângulo na serial |
