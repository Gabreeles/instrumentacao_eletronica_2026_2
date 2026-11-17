# Calibracao estatica do acelerometro (item 2.1 do roteiro).
# Uso: python calibrar.py
#
# Le os 6 arquivos da pasta calibracao/, um para cada posicao:
#   x_pos.csv  eixo X apontando para cima  (+1 g)
#   x_neg.csv  eixo X apontando para baixo (-1 g)
#   y_pos.csv, y_neg.csv, z_pos.csv, z_neg.csv  (idem)
#
# Para cada eixo resolve o sistema da eq. 2 do roteiro:
#   media_pos = Ganho * (+1) + Offset
#   media_neg = Ganho * (-1) + Offset
# Somando e subtraindo as duas equacoes:
#   Ganho  = (media_pos - media_neg) / 2
#   Offset = (media_pos + media_neg) / 2

PASTA = "calibracao_sem_nivel"
COLUNA = {"x": 1, "y": 2, "z": 3}   # posicao de cada eixo no CSV


def media_do_eixo(arquivo, coluna):
    valores = []
    for linha in open(arquivo):
        campos = linha.strip().split(",")
        try:
            valores.append(float(campos[coluna]))
        except (ValueError, IndexError):
            continue   # pula o cabecalho
    if len(valores) < 100:
        print("AVISO:", arquivo, "tem so", len(valores), "leituras (minimo 100)")
    return sum(valores) / len(valores), len(valores)


resultado = {}
print()
print("eixo   n+   media(+1g)    n-   media(-1g)    ganho     offset(g)")
for eixo in "xyz":
    pos, n_pos = media_do_eixo(PASTA + "/" + eixo + "_pos.csv", COLUNA[eixo])
    neg, n_neg = media_do_eixo(PASTA + "/" + eixo + "_neg.csv", COLUNA[eixo])

    ganho = (pos - neg) / 2
    offset = (pos + neg) / 2
    resultado[eixo] = (pos, neg, ganho, offset)

    print(f" {eixo.upper()}   {n_pos:4d}  {pos:+.4f}     {n_neg:4d}  {neg:+.4f}    "
          f"{ganho:.4f}   {offset:+.4f}")

print()
print("Copie para o topo do src/main.cpp:")
print()
for eixo in "xyz":
    print(f"#define GANHO_{eixo.upper()}  {resultado[eixo][2]:.4f}")
print()
for eixo in "xyz":
    print(f"#define OFFSET_{eixo.upper()} {resultado[eixo][3]:.4f}")

print()
print("Linhas para a Tabela II do relatorio:")
print()
for eixo in "xyz":
    pos, neg, ganho, offset = resultado[eixo]
    linha = f"{eixo.upper()} & {pos:.4f} & {neg:.4f} & {ganho:.4f} & {offset:.4f} \\\\ \\hline"
    print("\t\t" + linha.replace(".", "{,}"))
