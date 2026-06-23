# Relatório: Exclusão Mútua Centralizada em Sistemas Distribuídos

## 1. Introdução
Este trabalho descreve o projeto e a implementação de um algoritmo centralizado para exclusão mútua em sistemas distribuídos. O sistema foi desenvolvido em linguagem C utilizando a API POSIX para threads e Sockets TCP para comunicação de rede. A arquitetura consiste em um servidor coordenador e múltiplos processos clientes que disputam o acesso a uma Região Crítica (RC).

## 2. Decisões de Projeto

### 2.1. Protocolo de Comunicação
Foi escolhido o protocolo **TCP (Transmission Control Protocol)**. Embora o UDP fosse uma opção, o TCP garante a entrega ordenada e confiável das mensagens, o que reduz substancialmente a complexidade do tratamento de perda de pacotes e reconexões, garantindo que o fluxo do algoritmo de exclusão mútua não seja quebrado por falhas transientes da rede local.

### 2.2. Formato das Mensagens
Para simplificar o _parsing_ (interpretação) pelo coordenador e pelos clientes, adotou-se um protocolo de mensagens de tamanho estritamente fixo de **16 bytes**. 
O formato estabelecido foi: `TIPO|ID_PROCESSO|PADDING`.
- `TIPO`: Um caractere definindo a intenção (`1` para REQUEST, `2` para GRANT, `3` para RELEASE).
- `ID_PROCESSO`: Identificador numérico do processo (até tamanho estourar os bytes restantes).
- `PADDING`: Preenchimento com caracteres `0` para que toda mensagem na rede trafegue exatamente 16 bytes, evitando problemas de fragmentação de stream típicos do TCP.

### 2.3. Arquitetura do Coordenador
Para respeitar as diretrizes do projeto, o Coordenador é multi-threaded, sendo suportado por três *threads* principais:
1. **Thread de Recepção (Conexões e Leitura)**: Faz o gerenciamento dos Sockets dos clientes utilizando a função `select()`. Isso permite ler mensagens (`REQUEST` e `RELEASE`) de forma assíncrona, enfileirando os pedidos sem bloquear o sistema.
2. **Thread do Algoritmo**: Um laço que observa o estado atual da Região Crítica e a fila. Quando a Região Crítica encontra-se livre, esta thread desenfileira o primeiro processo, marca a RC como ocupada, contabiliza os atendimentos, e envia a mensagem `GRANT`.
3. **Thread de Interface (Principal)**: Uma thread interativa (via `stdin`) que permite a observação do estado em tempo real: imprimir fila e contar atendimentos por processo.

**Sincronização**: Como a Fila e a variável de estado da Região Crítica são compartilhadas entre a Thread de Recepção e a Thread do Algoritmo, utilizamos **Mutexes** (`pthread_mutex_t`) para garantir acesso atômico e prevenir Condições de Corrida (Race Conditions).

### 2.4. Arquitetura do Processo Cliente
O processo cliente é _single-threaded_. Ele opera em um loop determinístico de `r` iterações: solicita acesso enviando um `REQUEST`, fica bloqueado em `recv()` aguardando o `GRANT`, executa a seção crítica (registro do log `resultado.txt` e `sleep(k)`), e envia um `RELEASE` ao sair. 

## 3. Avaliação e Casos de Uso

### 3.1. Cenário de Testes
Para avaliar o sistema, foi desenvolvido um _shell script_ (`teste.sh`) que sobe o coordenador no _background_ e inicializa `n` processos clientes simultaneamente, todos tentando acessar o coordenador ao mesmo tempo. Isso garante alta contenção e um teste severo para a fila do coordenador.
Adicionalmente, foi implementado um script em Python (`valida.py`) para avaliar automaticamente a corretude da execução a partir de duas fontes de verdade:
1. O arquivo `resultado.txt` gerado pela colisão dos processos.
2. O arquivo de rastreio `log_coordenador.txt` mantido pelo próprio coordenador.

### 3.2. Análise de Corretude
O script validador certificou as duas propriedades fundamentais de um algoritmo de exclusão mútua:
- **Safety (Segurança)**: O Log demonstra de forma inequívoca que a sequência de mensagens sempre respeita o padrão temporal `[, GRANT(x), RELEASE(x), GRANT(y), ]`. Nunca houve concessão dupla. As linhas em `resultado.txt` mantêm a estrita evolução monotônica de *timestamps*.
- **Liveness (Progresso)**: Todos os `n` processos registraram exatamente `r` iterações no log final, sem entrar em inanição (_starvation_) ou impasse (_deadlock_). O enfileiramento linear (FIFO) garante que todo pedido será eventualmente servido (Bounded Waiting).

## 4. Conclusão
O desenvolvimento do algoritmo de exclusão mútua centralizado alcançou total êxito. A escolha por Sockets TCP unida à estruturação do código em C via Pthreads forneceu uma fundação sólida, livre de _race conditions_, escalável para múltiplos processos clientes em um ambiente POSIX, confirmada pela rigorosa etapa de testes automatizados.
