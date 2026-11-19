# Grafico de erro do angulo de inclinacao (item 3c do roteiro).
# Uso: python erro_angulo.py
#
# Le a pasta angulos/, com um arquivo por angulo do transferidor:
#   ang_0.csv, ang_15.csv, ang_30.csv, ...   (angulo de referencia no nome)
# Para cada arquivo:
#   medido = media da coluna angulo_graus (angulo com calibracao)
#   erro   = referencia - medido
# Para comparar, refaz a mesma conta do firmware com as leituras brutas
# (colunas ay_g e az_g), ou seja, sem a calibracao.
# Desenha os dois erros em funcao do angulo de referencia.

import glob
import math
import os
import matplotlib.pyplot as plt

PASTA = "angulos"


def angulo_bruto(ay, az):
    # mesma conta do firmware, sem tirar offset nem dividir pelo ganho
    a = math.degrees(math.atan2(az, ay))
    if a < -90:
        a = a + 360
    return a


pontos = []
for arquivo in glob.glob(PASTA + "/ang_*.csv"):
    nome = os.path.basename(arquivo)                  # ex.: ang_45.csv
    referencia = float(nome[4:-4])

    angulos = []
    brutos = []
    for linha in open(arquivo):
        campos = linha.strip().split(",")
        try:
            angulos.append(float(campos[4]))
            brutos.append(angulo_bruto(float(campos[2]), float(campos[3])))
        except (ValueError, IndexError):
            continue   # pula o cabecalho

    medido = sum(angulos) / len(angulos)
    sem_cal = sum(brutos) / len(brutos)
    pontos.append((referencia, medido, referencia - medido, referencia - sem_cal, len(angulos)))

if len(pontos) == 0:
    raise SystemExit("nenhum arquivo ang_*.csv em " + PASTA)

pontos.sort()
ref = [p[0] for p in pontos]
err = [p[2] for p in pontos]
err_sem = [p[3] for p in pontos]

print()
print(" ref (graus)   medido (graus)   erro (graus)   erro sem calib.   leituras")
for r, m, e, es, n in pontos:
    print(f"   {r:6.1f}        {m:8.2f}        {e:+6.2f}          {es:+6.2f}         {n}")
print()
print(f"com calibracao: erro medio {sum(err) / len(err):+.2f}, maior erro {max(err, key=abs):+.2f} graus")
print(f"sem calibracao: erro medio {sum(err_sem) / len(err_sem):+.2f}, maior erro {max(err_sem, key=abs):+.2f} graus")

# tamanho de uma coluna do relatorio IEEE (3,5 polegadas de largura)
plt.rcParams.update({"font.size": 8, "font.family": "serif"})
plt.figure(figsize=(3.5, 2.4))
plt.plot(ref, err, "o-", markersize=3.5, label="com calibração")
plt.plot(ref, err_sem, "s--", markersize=3, label="sem calibração")
plt.axhline(0, color="gray", linewidth=0.8)
plt.xticks(range(0, 181, 30))
plt.xlabel("Ângulo de referência (graus)")
plt.ylabel("Erro (graus)")
plt.legend(loc="lower right")
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig("grafico_erro.png", dpi=300)
print("grafico salvo em grafico_erro.png")
plt.show()
