#!/usr/bin/python3
import pandas as pd

# Carregue o arquivo CSV em um DataFrame
basepath = '/opt/simuladores/ns-allinone-3.40/ns-3.40/scratch/output/'
df = pd.read_csv(basepath+'deviceStatus-ns3::AdrPlus.csv', header=None, sep=' ')

# Crie um dicionário para armazenar os resultados
resultado = {}

# Defina o valor de numED
numED = 500

# Itere pelas linhas do DataFrame original em incrementos definidos por numED
for i in range(0, len(df), numED):
    intervalo = df.iloc[i, 0]  # Valor da primeira coluna
    valores = df.iloc[i:i+numED, -6].to_list()  # Valores da coluna DR para o intervalo específico

    # Subtrai 12 de cada valor
    valores = [12 - valor for valor in valores]

    # Conte as ocorrências de cada valor e armazene em um dicionário
    contagem = {valor: valores.count(valor) for valor in valores}

    # Adicione o dicionário de contagem ao dicionário de resultados com a chave do intervalo
    resultado[intervalo] = contagem

# Exiba o dicionário de resultados e a porcentagem em relação a numED
for intervalo, contagem in resultado.items():
    print(f'Intervalo {intervalo}:')
    for valor in sorted(contagem.keys()):  # Ordena as chaves do dicionário
        ocorrencias = contagem[valor]
        percentual = (ocorrencias / numED) * 100
        print(f'Valor {valor}: {ocorrencias} ocorrências ({percentual:.2f}%) em relação a numED')

