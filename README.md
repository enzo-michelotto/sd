# Exclusão Mútua Centralizada

## 1. Acessar a pasta pelo WSL (Ubuntu)

Abra seu terminal do WSL e rode:

```bash
cd /mnt/c/Users/Enzo/Desktop/sd
```

## 2. Teste Automatizado

O script `teste.sh` compila o código, sobe o coordenador, dispara vários processos simultaneamente e usa o `valida.py` para provar a corretude.

Dê permissão de execução:

```bash
chmod +x teste.sh
```

Execute o teste (Formato: `<Num_Processos> <Repeticoes_por_Processo> <Segundos_na_RC>`):

```bash
./teste.sh 5 3 0
```

*(No exemplo acima: 5 processos, 3 repetições cada, 0 segundos dentro da região crítica para teste rápido).*

## 3. Teste Manual (Interativo)

Se quiser apresentar para o professor interagindo em tempo real, use duas janelas do terminal do WSL.

**Terminal 1 (Coordenador):**

```bash
gcc coordenador.c -o coordenador -pthread
./coordenador
```

*(Digite `1` para ver a fila, `2` para ver os atendimentos ou `3` para sair)*

**Terminal 2 (Processos):**

```bash
gcc processo.c -o processo
./processo 127.0.0.1 8080 99 5 2
```

*(No exemplo acima: Inicia o processo de ID '99', que pedirá acesso 5 vezes e dormirá 2 segundos cada vez que acessar a RC)*
