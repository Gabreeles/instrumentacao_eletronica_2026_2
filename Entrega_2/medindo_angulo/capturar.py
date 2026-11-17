# Le a porta serial e grava o CSV.
# Uso: python capturar.py [porta] [arquivo] [n_leituras]
#   python capturar.py COM3 calibracao/x_pos.csv 200
# Sem n_leituras, grava ate apertar Ctrl+C.

import os
import sys
import time
import serial

porta = sys.argv[1] if len(sys.argv) > 1 else "COM3"
saida = sys.argv[2] if len(sys.argv) > 2 else "dados.csv"
n_max = int(sys.argv[3]) if len(sys.argv) > 3 else 0

pasta = os.path.dirname(saida)
if pasta:
    os.makedirs(pasta, exist_ok=True)

# O DTR e o RTS dessa placa vao no GPIO0 e no EN. Se o pyserial ativar
# os dois na abertura, a ESP32 cai em modo de gravacao e nao envia nada.
ser = serial.Serial()
ser.port = porta
ser.baudrate = 115200
ser.timeout = 1
ser.dtr = False
ser.rts = False
ser.open()

# pulso no EN para a placa reiniciar e a captura comecar do inicio
ser.rts = True
time.sleep(0.1)
ser.rts = False
ser.reset_input_buffer()

print("lendo", porta, "->", saida, " (Ctrl+C para parar)")

arquivo = open(saida, "w")
n = 0

try:
    while n_max == 0 or n < n_max:
        linha = ser.readline().decode(errors="replace").strip()
        if linha.count(",") != 4:   # descarta as mensagens de boot da ESP32
            continue
        if linha.startswith("t_ms") and n > 0:
            continue
        arquivo.write(linha + "\n")
        arquivo.flush()
        if not linha.startswith("t_ms"):
            n += 1
        print(n, linha, end="      \r")
except KeyboardInterrupt:
    pass

arquivo.close()
ser.close()
print("\n", n, "leituras gravadas em", saida)
