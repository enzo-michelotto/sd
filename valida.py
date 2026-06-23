import sys
import re

def valida(n, r):
    print("=== VALIDANDO resultado.txt ===")
    try:
        with open("resultado.txt", "r") as f:
            linhas = f.readlines()
    except FileNotFoundError:
        print("ERRO: resultado.txt não encontrado!")
        return

    esperado = n * r
    print(f"Linhas encontradas: {len(linhas)}. Esperado: {esperado}")
    if len(linhas) != esperado:
        print("FALHA: O arquivo não tem o número correto de linhas.")
    
    timestamps = []
    process_counts = {}
    
    for i, linha in enumerate(linhas):
        partes = linha.strip().split()
        if len(partes) != 2:
            print(f"FALHA: Linha {i+1} mal formatada: {linha}")
            continue
        pid = int(partes[0])
        ts = float(partes[1])
        timestamps.append(ts)
        process_counts[pid] = process_counts.get(pid, 0) + 1
        
    ordenado = True
    for i in range(1, len(timestamps)):
        if timestamps[i] < timestamps[i-1]:
            print(f"FALHA: Timestamp na linha {i+1} ({timestamps[i]}) é menor que o anterior ({timestamps[i-1]}).")
            ordenado = False
    
    if ordenado:
        print("SUCESSO: Os acessos na Região Crítica respeitam a evolução do relógio (ordem crescente).")
        
    repeticoes_certas = True
    for pid, count in process_counts.items():
        if count != r:
            print(f"FALHA: Processo {pid} executou {count} vezes. Esperado: {r}.")
            repeticoes_certas = False
            
    if repeticoes_certas:
        print(f"SUCESSO: Todos os {n} processos acessaram a RC exatamente {r} vezes.")

    print("\n=== VALIDANDO log_coordenador.txt ===")
    try:
        with open("log_coordenador.txt", "r") as f:
            log_linhas = f.readlines()
    except FileNotFoundError:
        print("ERRO: log_coordenador.txt não encontrado!")
        return

    padrao = re.compile(r"MSG: (\w+) - Processo: (\d+)")
    
    mensagens = []
    for l in log_linhas:
        m = padrao.search(l)
        if m:
            tipo = m.group(1)
            pid = int(m.group(2))
            mensagens.append((tipo, pid))

    alternancia_valida = True
    estado_livre = True
    processo_na_rc = None
    
    for tipo, pid in mensagens:
        if tipo == "GRANT":
            if not estado_livre:
                print(f"FALHA: GRANT concedido para o processo {pid} enquanto RC não estava livre (último foi {processo_na_rc})")
                alternancia_valida = False
            estado_livre = False
            processo_na_rc = pid
        elif tipo == "RELEASE":
            if estado_livre:
                print(f"FALHA: RELEASE do processo {pid} mas a RC já estava livre.")
                alternancia_valida = False
            elif processo_na_rc != pid:
                print(f"FALHA: RELEASE do processo {pid} mas a RC estava ocupada pelo {processo_na_rc}.")
                alternancia_valida = False
            estado_livre = True
            processo_na_rc = None

    if alternancia_valida:
        print("SUCESSO: A alternância GRANT/RELEASE foi estrita. Não houve violação de exclusão mútua no Log.")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Uso: python3 valida.py <N> <R>")
    else:
        valida(int(sys.argv[1]), int(sys.argv[2]))
