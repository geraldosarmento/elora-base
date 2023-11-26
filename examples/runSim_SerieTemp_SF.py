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
rootPath = "scratch/output/"
numRep = 5  
numED = 500
raio = 6000
#adrDic = {"ns3::AdrLorawan":"ADR", "ns3::AdrPlus":"ADR+"}
###adrDic = {"ns3::AdrLorawan":"ADR", "ns3::AdrPlus":"ADR+", "ns3::AdrGaussian":"G-ADR", "ns3::AdrEMA":"EMA-ADR"}
adrDic = {"ns3::AdrPlus":"ADR+", "ns3::AdrCentral":"ADR-C", "ns3::AdrCentralPlus":"ADR-C+"}

# Lista genérica usada para testar diversos modelos dentro da simulação
#cenarios = ["true", "false"]
cenarios = ["true"]

mediaPDR = {}
tempo = []

dfPDR = pd.DataFrame()
dfSF = pd.DataFrame()

def executarSim(): 
    global mediaPDR, tempo, dfPDR
    rodCont = 1     # Conta a quantidade de rodadas
    apagarArqs(rootPath, '.csv')    #Limpa arquivos de saída anteriores 
    
    #for m in mobileProb:
    for cen in cenarios:
        for esq in adrDic.keys():   #cenários
            for rep in range(numRep):
                print("=============================================================")
                print("Executando esquema:",esq,". Rodada #",rodCont," de ",len(adrDic)*numRep*len(cenarios))
                print("=============================================================")
                cmd = f"./ns3 run \"littoral  --adrType={esq} --nED={numED} --radius={raio} --mobility=true --okumura={cen}\" --quiet"
                # --confMode={cen} --baseSeed={ensCont} --mobileProb={m} --EDadrEnabled={cen} --okumura={cen} --poisson={cen}
                # # Sample: ./ns3 run "littoral --poisson=true --quiet"
                # ./contrib/elora/examples/runSim_SerieTemp_SF.py && ./contrib/elora/examples/runSim_Escalabilidade.py
                inicio = time.time()
                os.system(cmd)
                fim = time.time()
                tempoDecor = fim - inicio
                tempoAcum += tempoDecor
                print(f"Tempo de execução desta rodada: {round(tempoDecor,5)} s") 
                print(f"Tempo estimado de término da simulação: {round( ((tempoAcum/rodCont) * (len(adrDic)*numRep*len(cenarios)-rodCont))/60 , 5)} min \n") 

               
                rodCont += 1
                atualizarSerieTemporal(esq, rep)
                rastrearSF(esq,rep)
            calculaMediaSerieTemporal()
            calcularMediaSF(esq)
            plotarSFFinalCirculo(cen, esq)
            # Adiciona os dados do DF nos dicionários
            mediaPDR[adrDic[esq]] = dfPDR["Media"].tolist()
            #Reinicia o data frame para o próximo cenário
            dfPDR = pd.DataFrame()
            dfSF = pd.DataFrame()              
        plotarSerieTemporal(cen)   
        if (cen == "true"):
            salvarDadosEmArquivo(mediaPDR, rootPath+'pdrEnsaio1.json')
        else:
            salvarDadosEmArquivo(mediaPDR, rootPath+'pdrEnsaio2.json')        
        mediaPDR = {}
        #apagarArqs(rootPath, '.csv')   #apaga apenas os arquivos .csv
        salvarDadosEmArquivo(tempo, rootPath+'tempo.json')

    

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

# Recebe o nome de um ensaio e uma determinada iteração do experimento
def atualizarSerieTemporal(ensaio, it):
    global dfPDR, tempo

    arquivo = rootPath + 'globalPerf-' + ensaio + '.csv'
    
    # Ler o arquivo CSV
    arq = pd.read_csv(arquivo, header=None, sep=' ')

    # Apaga a primeira linha do DF, pra evitar a divisão por 0
    arq = arq.drop(0)
    dfPDR['Tempo'] = arq[0]/3600
    tempo = dfPDR['Tempo'].tolist()  # Guarda o tempo para gerar o gráfico
    dfPDR['Media'] = arq[1]  # Cria a coluna Media inicialmente com valores coringa
    dfPDR['pdr'+str(it)] = arq[2]/arq[1]    

def calculaMediaSerieTemporal():
    global dfPDR
    soma = 0
    media = []

    for i in range (dfPDR.shape[0]):     #Para cada linha do DF
        linha = dfPDR.iloc[i, 2:]
        soma =+ linha.sum()
        media.append(soma/(dfPDR.shape[1]-2))  # soma/qtdColunas (exceto a 1a)   
    dfPDR['Media'] = media


########################## Cálculo do SF ########################
def rastrearSF(esquema, it):
    global dfSF

    arquivo = rootPath + 'deviceStatus-' + esquema + '.csv'
    # Ler o arquivo CSV
    arq = pd.read_csv(arquivo, header=None, sep=' ')

    # Considera apenas as ultimas numED linhas
    valoresSF = 12 - arq.iloc[-numED:, 6]
    dfSF['sf'+str(it)] = valoresSF

def calcularMediaSF(esquema):
    colunasMedia = dfSF.loc[:, 'sf0':'sf'+str(numRep-1)]
    dfSF['media'] =  colunasMedia.mean(axis=1).astype(int)

    proporcoes = (dfSF['media'].value_counts(normalize=True) * 100).to_dict()
    proporcoesOrd = {chave: proporcoes[chave] for chave in sorted(proporcoes)}
    proporcoesOrd = {chave: np.round(valor, 4) for chave, valor in proporcoesOrd.items()}

    with open(f'{rootPath}SFfinal-{esquema}.json', 'w') as json_file:
        json_file.write(json.dumps(proporcoesOrd, indent=4))


def plotarSerieTemporal(cenario):
    marcadores = ['*','+','x','^','p','o']
    #cores = ['black', 'red', 'green', 'blue']
    
    # Plotar o gráfico
    plt.figure(figsize=(12, 6))
    #for chave, valores in pdrMedia.items():
    for i, (chave, valores) in enumerate(mediaPDR.items()):
        # Plotar a série de dados com uma cor diferente e rótulo
        #cor = cores[i % len(cores)]
        marcador = marcadores[i % len(marcadores)]
        #plt.plot(tempo, valores, linestyle='-', marker=marcador, color=cor, label=chave)
        plt.plot(tempo, valores, linestyle='-', marker=marcador, label=chave)


    eixoX = np.arange(0, max(tempo) + 1, 4)
    plt.xticks(eixoX)
    #plt.ylim(0, 0.8)
    plt.xlabel('Tempo (horas)')
    plt.ylabel('PDR Médio')    
    plt.legend()
    plt.grid(True)
    if (cenario == "true"):
        plt.title('PDR Médio - Cenário 1')
        plt.savefig(rootPath+'serieTemporal-Cenario1.png')
    else:
        plt.title('PDR Médio - Cenário 2')
        plt.savefig(rootPath+'serieTemporal-Cenario2.png')
    #plt.show()

def plotarSFFinalCirculo(cenario, esquema):
    arquivo = rootPath + 'deviceStatus-' + esquema + '.csv'
    # Ler o arquivo CSV
    arq = pd.read_csv(arquivo, header=None, sep=' ')

    # Extrai as colunas 2 (coordenada x), 3 (coordenada y) e 6 (valor de DR)
    coordX =       arq.iloc[-numED:, 2]
    coordY =       arq.iloc[-numED:, 3]
    valoresSF = 12 - arq.iloc[-numED:, 6]

    # Cria um novo DataFrame com as colunas atualizadas
    EDdf = pd.DataFrame({'X': coordX, 'Y': coordY, 'SF': valoresSF})

    # Define as cores com base nos valores de SF
    coresSF = {
        12: 'red',
        11: 'tan',
        10: 'blue',
        9: 'green',
        8: 'pink',
        7: 'black'
    }

    # Cria o gráfico
    plt.figure(figsize=(8, 8))

    # Mantém a proporção dos eixos iguais
    plt.axis('equal')

    # Plota o gateway em formato de estrela no centro
    plt.plot(0, 0, marker='*', color='gold', markersize=15, label='Gateway')

    # Plota o círculo em preto
    circulo = plt.Circle((0, 0), raio+300, color='black', fill=False, linestyle='--', linewidth=1)
    plt.gca().add_patch(circulo)

    # Plota os nós da rede com destaque em suas circunferências
    for index, row in EDdf.iterrows():
        plt.scatter(row['X'], row['Y'], s=200, color=coresSF[row['SF']], edgecolor='black', linewidth=0.5)
        
    # Adiciona legendas
    legend_elements = [Patch(color=color, label=f'SF: {sf}') for sf, color in coresSF.items()]
    plt.legend(handles=legend_elements, title='Valores de SF')
    plt.title('Topologia Final da Rede')
    plt.xlabel('Posição X (m)')
    plt.ylabel('Posição Y (m)')

    if (cenario == "true"):
        plt.title('Topologia Final da Rede - Cenário 1')
        plt.savefig(f'{rootPath}sfCircle-{esquema}-Cenario1.png')
    else:
        plt.title('Topologia Final da Rede - Cenário 2')
        plt.savefig(f'{rootPath}sfCircle-{esquema}-Cenario2.png')



# Função para salvar um dicionário em um arquivo JSON
def salvarDadosEmArquivo(dicionario, nome_arquivo):
    with open(nome_arquivo, 'w') as arquivo:
        json.dump(dicionario, arquivo)

# Função para carregar um dicionário de um arquivo JSON
def carregarDadosDeArquivo(nome_arquivo):
    with open(nome_arquivo, 'r') as arquivo:
        dicionario = json.load(arquivo)
        return dicionario



def main():
    global mediaPDR, tempo
    ###compilar()


    if (novaSimulacao):
        inicio = time.time()
        executarSim()
        fim = time.time()
        tempo_decorrido = fim - inicio
        tempo_decorrido /= 60
        print(f"Tempo total de simulação: {round(tempo_decorrido,5)} min")
    else:        # Apenas gerar os gráficos novamente
        tempo = carregarDadosDeArquivo(rootPath+'tempo.json')
        mediaPDR = {}
        mediaPDR = carregarDadosDeArquivo(rootPath+'pdrEnsaio1.json')
        plotarSerieTemporal("true")
        mediaPDR = {}
        mediaPDR = carregarDadosDeArquivo(rootPath+'pdrEnsaio2.json')
        plotarSerieTemporal("false")
   

if __name__ == '__main__':
    main()







