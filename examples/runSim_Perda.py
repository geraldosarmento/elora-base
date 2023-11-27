#!/usr/bin/python3
import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from scipy.stats import t
import time
import json


novaSimulacao = True
energyModel = True
modoCpsr = False
rootPath = "scratch/output/"
numRep = 5
numED = 500
expPerda = [0.0, 0.892, 1.785, 2.267, 3.570]
#expPerda = [0.892, 3.570]
raio = 6000
energiaInicial = 10000
adrDic = {"ns3::AdrLorawan":"ADR", "ns3::AdrPlus":"ADR+", "ns3::AdrCentral":"ADR-C"}
#adrDic = {"ns3::AdrPlus":"ADR+", "ns3::AdrCentral":"ADR-C"}


# Lista genérica usada para testar diversos modelos dentro da simulação
#cenarios = ["true", "false"]
cenarios = ["true"]


amostras = []
amostrasCpsr = []
amostrasEnergy = []
dfED = pd.DataFrame()
dfEDCpsr = pd.DataFrame()
dfEnergy = pd.DataFrame()
pivot = 0  #controla as linhas do DF
pivotCpsr = 0
pivotEnergy = 0

ultimaChave = list(adrDic.keys())[-1]

def executarSim(): 
    global mediaED, dfED
    rodCont = 1     # Conta a quantidade de rodadas
    tempoAcum = 0
    apagarArqs(rootPath, '.csv')    #Limpa arquivos de saída anteriores
    reiniciarDF()
    
    #for m in mobileProb:
    for cen in cenarios:
        for exps in expPerda:
            for esq in adrDic.keys():   #esquemas            
                for rep in range(numRep):
                    print("=============================================================")
                    print("Executando esquema:",esq," - Rodada #",rodCont," de ",len(expPerda)*len(adrDic)*numRep*len(cenarios))
                    print("=============================================================")
                    cmd = f"./ns3 run \"littoral  --adrType={esq} --nED={numED} --radius={raio} --okumura=false --mobility=true --pathLossExp={exps}\" --quiet"
                    # --confMode={cen} --baseSeed={ensCont} --EDadrEnabled={cen} --okumura={cen} --poisson={cen} --mobility={cen} --simTime=1200
                    # Sample: ./ns3 run "littoral --adrType=ns3::AdrPlus"
                    # exec(open('/opt/simuladores/ns-allinone-3.40-ELORA/ns-3.40/contrib/elora/examples/runSim_Escalabilidade.py').read())
                    inicio = time.time()
                    os.system(cmd)
                    fim = time.time()
                    tempoDecor = fim - inicio
                    tempoAcum += tempoDecor
                    print(f"Tempo de execução desta rodada: {round(tempoDecor,5)} s") 
                    print(f"Tempo estimado de término da simulação: {round( ((tempoAcum/rodCont) * (len(expPerda)*len(adrDic)*numRep*len(cenarios)-rodCont))/60 , 5)} min \n") 

                    rodCont += 1                    
                    atualizarDados(esq) 
                    atualizarDadosCpsr(esq) if modoCpsr else None
                    atualizarDadosEnergia(esq) if energyModel else None
        plotarGrafico(cen)
        plotarGraficoEnergy(cen) if energyModel else None
        if (cen == "true"):
            salvarDadosEmArquivo(dfED, rootPath+'PDRPerda-Cen1.json')
            salvarDadosEmArquivo(dfEDCpsr, rootPath+'PDRPerdaCpsr-Cen1.json') if modoCpsr else None
            salvarDadosEmArquivo(dfEnergy, rootPath+'EnergyPerda-Cen1.json') if energyModel else None
        else:
            salvarDadosEmArquivo(dfED, rootPath+'PDRPerda-Cen2.json')
            salvarDadosEmArquivo(dfEDCpsr, rootPath+'PDRPerdaCpsr-Cen2.json') if modoCpsr else None 
            salvarDadosEmArquivo(dfEnergy, rootPath+'EnergyPerda-Cen2.json') if energyModel else None      
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
    global dfED, dfEDCpsr, dfEnergy, pivot, pivotCpsr, pivotEnergy

    dfED = pd.DataFrame()
    dfED['expPerda'] = expPerda
    dfED['expPerda'] = dfED['expPerda'].astype(float)
    for esq in adrDic.keys():
        dfED[adrDic[esq]] = None
    pivot = 0

    if (modoCpsr):
        dfEDCpsr = pd.DataFrame()
        dfEDCpsr['expPerda'] = expPerda
        dfEDCpsr['expPerda'] = dfEDCpsr['expPerda'].astype(float)
        for esq in adrDic.keys():
            dfEDCpsr[adrDic[esq]+"-CPSR"] = None
        pivotCpsr = 0

    if (energyModel):
        dfEnergy = pd.DataFrame()
        dfEnergy['expPerda'] = expPerda
        dfEnergy['expPerda'] = dfEnergy['expPerda'].astype(float)
        for esq in adrDic.keys():
            dfEnergy[adrDic[esq]] = None
        pivotEnergy = 0


# Recebe o nome de um ensaio e uma determinada iteração do experimento
def atualizarDados(esq):
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

def atualizarDadosCpsr(esq):
    global dfEDCpsr, amostrasCpsr, pivotCpsr
   
    arquivo = rootPath + 'GlobalPacketCountCpsr-' + esq + '.csv'
    arq = pd.read_csv(arquivo, header=None, sep=' ')
    amostrasCpsr.append(arq.iloc[0,2])  #Coluna com o PDR

    if (len(amostrasCpsr) == numRep):        
        media = sum(amostrasCpsr) / len(amostrasCpsr)
        dfEDCpsr.loc[pivotCpsr,adrDic[esq]+"-CPSR"] = media
        amostrasCpsr = []
        if (esq == ultimaChave):
            pivotCpsr += 1

def plotarGrafico(cen):
    eixo_x = dfED.iloc[:, 0]
    linhas_grafico = dfED.iloc[:, 1:]  # Seleciona da coluna 1 em diante
    if modoCpsr:
        linhas_grafico = pd.concat([linhas_grafico,dfEDCpsr.iloc[:, 1:]],axis=1)
        #print (f"linhas_grafico:\n{linhas_grafico}")
    
    marcadores = ['*','+','x','^','v','p','o','s']
    estilos = ['--', ':', '-.']

    # Criando o gráfico
    plt.figure(figsize=(10, 6))  # Definindo o tamanho do gráfico

    # Plotando as linhas do gráfico para as últimas 4 colunas
    for i, coluna in enumerate(linhas_grafico.columns):
        plt.plot(eixo_x, linhas_grafico[coluna], linestyle=estilos[i % len(estilos)] , marker=marcadores[i % len(marcadores)], label=coluna)  # Adiciona uma linha para cada coluna


    plt.xticks(eixo_x)
    plt.xlabel('Expoente de Perda de Propagação')  # Define o rótulo do eixo X
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

##### ENERGIA #####
def atualizarDadosEnergia(esq):
    global dfEnergy, amostrasEnergy, pivotEnergy

    with open('battery-level.txt', 'r') as arquivo:
        linhas = arquivo.readlines()  # Lê todas as linhas do arquivo
    
    ultimaLinha = linhas[-1].split()
    segundaColuna = float(ultimaLinha[1])

    amostrasEnergy.append(energiaInicial-segundaColuna)

    if (len(amostrasEnergy) == numRep):        
        media = sum(amostrasEnergy) / len(amostrasEnergy)
        dfEnergy.loc[pivotEnergy,adrDic[esq]] = media
        amostrasEnergy = []
        if (esq == ultimaChave):
            pivotEnergy += 1


def plotarGraficoEnergy(cen):
    #print(f"dfEnergy: \n {dfEnergy}")

    eixo_x = dfEnergy.iloc[:, 0]
    linhas_grafico = dfEnergy.iloc[:, 1:]  # Seleciona da coluna 1 em diante
    
    marcadores = ['*','+','x','^','v','p','o','s']
    estilos = ['--', ':', '-.']

    # Criando o gráfico
    plt.figure(figsize=(10, 6))  # Definindo o tamanho do gráfico

    # Plotando as linhas do gráfico para as últimas 4 colunas
    for i, coluna in enumerate(linhas_grafico.columns):
        plt.plot(eixo_x, linhas_grafico[coluna], linestyle=estilos[i % len(estilos)], marker=marcadores[i % len(marcadores)], label=coluna)  # Adiciona uma linha para cada coluna


    plt.xticks(eixo_x)
    plt.xlabel('Expoente de Perda de Propagação')  # Define o rótulo do eixo X
    plt.ylabel('Consumo (em J))')  # Define o rótulo do eixo Y
    plt.grid(True)
    plt.legend()  # Adiciona a legenda com base nos nomes das colunas

    if (cen == "true"):
        plt.title('Consumo Energético - Ensaio 1')
        plt.savefig(rootPath+'energia-Ensaio1.png')
    else:
        plt.title('Consumo Energético - PDR Médio - Ensaio 2')
        plt.savefig(rootPath+'energia-Ensaio2.png')
    #plt.show()  # Exibe o gráfico


##### MISC ########

# Função para salvar um DF em um arquivo JSON
def salvarDadosEmArquivo(df, nome_arquivo):
    df.to_json(nome_arquivo, orient='records')

# Função para carregar um DF de um arquivo JSON
def carregarDadosDeArquivo(nome_arquivo):
    df = pd.read_json(nome_arquivo, orient='records')
    return df


def main():    
    global dfED, dfEDCpsr, dfEnergy


    if (novaSimulacao):
        inicio = time.time()
        executarSim()
        fim = time.time()
        tempo_decorrido = fim - inicio
        tempo_decorrido /= 60
        print(f"Tempo total de simulação: {round(tempo_decorrido,5)} min") 
    else:        # Apenas gerar os gráficos novamente
        
        dfED = carregarDadosDeArquivo(rootPath+'PDRPerda-Cen1.json')
        dfEDCpsr = carregarDadosDeArquivo(rootPath+'PDRPerdaCpsr-Cen1.json') if modoCpsr else None
        dfEnergy = carregarDadosDeArquivo(rootPath+'EnergyPerda-Cen1.json') if energyModel else None
        plotarGrafico("true")
        plotarGraficoEnergy("true") if energyModel else None
        if (len(cenarios) == 2):
            dfED = carregarDadosDeArquivo(rootPath+'PDRPerda-Cen2.json')
            dfEDCpsr = carregarDadosDeArquivo(rootPath+'PDRPerdaCpsr-Cen2.json') if modoCpsr else None
            dfEnergy = carregarDadosDeArquivo(rootPath+'EnergyPerda-Cen2.json') if energyModel else None
            plotarGrafico("false")
            plotarGraficoEnergy("false") if energyModel else None
        

if __name__ == '__main__':
    main()
