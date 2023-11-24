
REQUISITOS DE CONFIGURAÇÃO:
- Dentro de ns-3.xx/scratch, deve ser criada a pasta output para armazenar os arquivos de saída da simulação 
- Certifique-se de que o arquivo examples/CMakeLists.txt contem a entrada para o código-fonte da simulação:
    build_lib_example(
        NAME littoral
        SOURCE_FILES littoral.cc
        LIBRARIES_TO_LINK
            ${libcore}
            ${liblorawan}
    )


- IMPORTANTE: executar tudo a partir da raiz do ns-3  (!)


SEQUÊNCIA DE EXECUÇÃO:
- Configurando e executando o build:
    ./ns3 configure --enable-examples  (necessário na 1a vez)
    ./ns3 build
  OU
    sh confLittoral.sh

- Ou invocar cada cenário independentemente em linha de comando:
    ./ns3 run littoral
