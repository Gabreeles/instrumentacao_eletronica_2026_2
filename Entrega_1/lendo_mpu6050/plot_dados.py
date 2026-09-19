# Desenha o grafico dos tres eixos a partir do CSV.
# Uso: python plot_dados.py [arquivo]

import sys
import matplotlib.pyplot as plt

arquivo = sys.argv[1] if len(sys.argv) > 1 else "dados.csv"

t = []
x = []
y = []
z = []

for linha in open(arquivo):
    campos = linha.strip().split(",")
    if len(campos) != 4:
        continue
    try:
        ms = int(campos[0])
        gx = float(campos[1])
        gy = float(campos[2])
        gz = float(campos[3])
    except ValueError:
        continue   # pula a linha do cabecalho

    t.append(ms / 1000.0)
    x.append(gx)
    y.append(gy)
    z.append(gz)

if len(t) == 0:
    sys.exit("nenhum dado encontrado em " + arquivo)

inicio = t[0]
t = [v - inicio for v in t]

plt.figure(figsize=(10, 5))
plt.plot(t, x, label="X")
plt.plot(t, y, label="Y")
plt.plot(t, z, label="Z")
plt.axhline(1.0, color="gray", linestyle="--", linewidth=0.8)
plt.axhline(0.0, color="gray", linestyle="--", linewidth=0.8)
plt.axhline(-1.0, color="gray", linestyle="--", linewidth=0.8)
plt.xlabel("tempo (s)")
plt.ylabel("aceleracao (g)")
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig("grafico.png", dpi=150)
print(len(t), "amostras ->", "grafico.png")
plt.show()
