# Grafico de erro do angulo de inclinacao (item 3c do roteiro).
# Uso: python erro_angulo.py
#
# Le a pasta angulos/, com um arquivo por angulo do transferidor:
#   ang_0.csv, ang_15.csv, ang_30.csv, ...   (angulo de referencia no nome)
# Para cada arquivo:
#   medido = media da coluna angulo_graus
#   erro   = referencia - medido
# e desenha o erro em funcao do angulo de referencia.

import glob
import os
import matplotlib.pyplot as plt

PASTA = "angulos"

pontos = []
for arquivo in glob.glob(PASTA + "/ang_*.csv"):
    nome = os.path.basename(arquivo)                  # ex.: ang_45.csv
    referencia = float(nome[4:-4])

    angulos = []
    for linha in open(arquivo):
        campos = linha.strip().split(",")
        try:
            angulos.append(float(campos[4]))
        except (ValueError, IndexError):
            continue   # pula o cabecalho

    medido = sum(angulos) / len(angulos)
    erro = referencia - medido
    pontos.append((referencia, medido, erro, len(angulos)))

if len(pontos) == 0:
    raise SystemExit("nenhum arquivo ang_*.csv em " + PASTA)

pontos.sort()
ref = [p[0] for p in pontos]
med = [p[1] for p in pontos]
err = [p[2] for p in pontos]

print()
print(" ref (graus)   medido (graus)   erro (graus)   leituras")
for r, m, e, n in pontos:
    print(f"   {r:6.1f}        {m:8.2f}        {e:+6.2f}        {n}")
print()
print(f"erro medio: {sum(err) / len(err):+.2f} graus")
print(f"maior erro: {max(err, key=abs):+.2f} graus")

plt.figure(figsize=(7, 4))
plt.plot(ref, err, "o-")
plt.axhline(0, color="gray", linewidth=0.8)
plt.xlabel("Ângulo de referência do transferidor (graus)")
plt.ylabel("Erro: referência − medido (graus)")
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig("grafico_erro.png", dpi=200)
print("grafico salvo em grafico_erro.png")
plt.show()
