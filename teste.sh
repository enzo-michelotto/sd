#!/bin/bash
gcc coordenador.c -o coordenador -pthread
gcc processo.c -o processo

rm -f resultado.txt log_coordenador.txt saida_coordenador.log

N=${1:-5}
R=${2:-3}
K=${3:-1}

echo "=========================================="
echo "Iniciando Teste (N=$N processos, r=$R repetições, k=$K s)"
echo "=========================================="

echo "Subindo o coordenador"
./coordenador < /dev/null > saida_coordenador.log 2>&1 &
COORD_PID=$!

sleep 1

echo "Disparando $N processos simultaneamente"
PIDS=""
for (( i=1; i<=N; i++ ))
do
    ./processo 127.0.0.1 8080 $i $R $K &
    PIDS="$PIDS $!"
done

echo "Aguardando os processos concluírem"
wait $PIDS

echo "Processos finalizaram. Matando o coordenador"
kill -9 $COORD_PID

echo "=========================================="
python3 valida.py $N $R
