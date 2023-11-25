#!/usr/bin/python3
import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from scipy.stats import t
import time
import json


rootPath = "scratch/output/"
numRep = 4
numED = [200,400,600,800,1000]
#numED = [600,800]
raio = 6000
adrDic = {"ns3::AdrLorawan":"ADR", "ns3::AdrPlus":"ADR+"}
#adrDic = {"ns3::AdrLorawan":"ADR", "ns3::AdrPlus":"ADR+", "ns3::AdrCentral":"ADR-C", "ns3::AdrCentralPlus":"ADR-C+"}


# Lista genérica usada para testar diversos modelos dentro da simulação
cenarios = ["true", "false"]
#cenarios = ["true"]


amostras = []
dfED = pd.DataFrame()
ultimaChave = list(adrDic.keys())[-1]
pivot = 0  #controla as linhas do DF

def executarSim(): 
    global mediaED, dfED
    rodCont = 1     # Conta a quantidade de rodadas
    apagarArqs(rootPath, '.csv')    #Limpa arquivos de saída anteriores
    reiniciarDF()
    
    #for m in mobileProb:
    for cen in cenarios:
        for eds in numED:
            for esq in adrDic.keys():   #esquemas            
                for rep in range(numRep):
                    print("=============================================================")
                    print("Executando esquema #",esq,". Rodada #",rodCont," de ",len(numED)*len(adrDic)*numRep*len(cenarios))
                    print("=============================================================")
                    cmd = f"./ns3 run \"littoral  --adrType={esq} --nED={eds} --radius={raio} --mobility={cen}\" --quiet"
                    # --confMode={cen} --baseSeed={ensCont} --EDadrEnabled={cen} --okumura={cen}
                    # Sample: ./ns3 run "littoral --adrType=ns3::AdrPlus"
                    # exec(open('/opt/simuladores/ns-allinone-3.40-ELORA/ns-3.40/contrib/elora/examples/runSim_Escalabilidade.py').read())
                    os.system(cmd)

                    rodCont += 1                    
                    atualizarDados(esq,rodCont)                              
        plotarGrafico(cen)  
        reiniciarDF() 
           

def compilar():
	cmd = f'./ns3 configure --enable-examples'
	os.system(cmd)

def apagarArqs(pasta, extensao=None):
    try:
        for arquivo in os.listdir(pasta):
            caminho_arquivo = os.path.join(pasta, arquivo)
            if os.path.isfile(caminho_arquivo):
                if extensao is None or arquivo.endswith(extensao):
                    os.remove(caminho_arquivo)
        #if extensao is None:
        #    print(f"Todos os arquivos em {pasta} foram excluídos com sucesso.")
        #else:
        #    print(f"Todos os arquivos com a extensão {extensao} em {pasta} foram excluídos com sucesso.")
    except Exception as e:
        print(f"Ocorreu um erro ao excluir os arquivos: {e}")


def reiniciarDF():
    global dfED, pivot

    dfED = pd.DataFrame()
    dfED['numED'] = numED
    dfED['numED'] = dfED['numED'].astype(int)
    for esq in adrDic.keys():
        dfED[adrDic[esq]] = None
    pivot = 0


# Recebe o nome de um ensaio e uma determinada iteração do experimento
def atualizarDados(esq,rod):
    global dfED, amostras, pivot

    arquivo = rootPath + 'GlobalPacketCount-' + esq + '.csv'    
    # Ler o arquivo CSV
    arq = pd.read_csv(arquivo, header=None, sep=' ')
    amostras.append(arq.iloc[0,2])  #Coluna com o PDR

    if (len(amostras) == numRep):        
        media = sum(amostras) / len(amostras)
        dfED.loc[pivot,adrDic[esq]] = media
        amostras = []
        if (esq == ultimaChave):
            pivot += 1

def plotarGrafico(cen):
    eixo_x = dfED.iloc[:, 0]
    linhas_grafico = dfED.iloc[:, 1:]  # Seleciona da coluna 1 em diante
    
    marcadores = ['*','+','x','^','p','o']

    # Criando o gráfico
    plt.figure(figsize=(10, 6))  # Definindo o tamanho do gráfico

    # Plotando as linhas do gráfico para as últimas 4 colunas
    for i, coluna in enumerate(linhas_grafico.columns):
        plt.plot(eixo_x, linhas_grafico[coluna], marker=marcadores[i % len(marcadores)], label=coluna)  # Adiciona uma linha para cada coluna


    plt.xticks(eixo_x)
    plt.xlabel('Número de EDs')  # Define o rótulo do eixo X
    plt.ylabel('PDR')  # Define o rótulo do eixo Y
    plt.title('Escalabilidade dos esquemas ADR')  # Define o título do gráfico
    plt.grid(True)
    plt.legend()  # Adiciona a legenda com base nos nomes das colunas

    if (cen == "true"):
        plt.title('PDR Médio - Ensaio 1')
        plt.savefig(rootPath+'escala-Ensaio1.png')
    else:
        plt.title('PDR Médio - Ensaio 2')
        plt.savefig(rootPath+'escala-Ensaio2.png')
    #plt.show()  # Exibe o gráfico


# Função para salvar um dicionário em um arquivo JSON
def salvarDictEmArquivo(dicionario, nome_arquivo):
    with open(nome_arquivo, 'w') as arquivo:
        json.dump(dicionario, arquivo)

# Função para carregar um dicionário de um arquivo JSON
def carregarDictDeArquivo(nome_arquivo):
    with open(nome_arquivo, 'r') as arquivo:
        dicionario = json.load(arquivo)
        return dicionario



def main():    
    inicio = time.time()
    executarSim()
    fim = time.time()
    tempo_decorrido = fim - inicio
    tempo_decorrido /= 60
    print(f"Tempo total de simulação: {round(tempo_decorrido,5)} min") 

if __name__ == '__main__':
    main()
