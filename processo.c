#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MSG_SIZE 16

void format_msg(char type, int id, char* buffer) {
    memset(buffer, '0', MSG_SIZE);
    char temp[MSG_SIZE];
    snprintf(temp, MSG_SIZE, "%c|%d|", type, id);
    memcpy(buffer, temp, strlen(temp));
}

int main(int argc, char const *argv[]) {
    if (argc != 6) {
        printf("Uso: %s <IP> <Porta> <ID> <r_repeticoes> <k_segundos_na_rc>\n", argv[0]);
        return -1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int my_id = atoi(argv[3]);
    int r = atoi(argv[4]);
    int k = atoi(argv[5]);

    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Erro na criacao do socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("\n Endereco invalido ou nao suportado \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n Falha na Conexao. O coordenador esta rodando? \n");
        return -1;
    }

    char msg[MSG_SIZE];
    char buffer[MSG_SIZE + 1];

    for (int i = 0; i < r; i++) {
        format_msg('1', my_id, msg);
        send(sock, msg, MSG_SIZE, 0);

        memset(buffer, 0, sizeof(buffer));
        int read_bytes = 0;
        while(read_bytes < MSG_SIZE) {
            int b = read(sock, buffer + read_bytes, MSG_SIZE - read_bytes);
            if (b <= 0) {
                printf("Erro de leitura ou conexao fechada prematuramente.\n");
                exit(1);
            }
            read_bytes += b;
        }

        if (buffer[0] == '2') {
            FILE* f = fopen("resultado.txt", "a");
            if (f) {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                fprintf(f, "%d %ld.%06ld\n", my_id, tv.tv_sec, tv.tv_usec);
                fclose(f);
            }

            sleep(k);

            format_msg('3', my_id, msg);
            send(sock, msg, MSG_SIZE, 0);
        } else {
            printf("Processo %d recebeu mensagem inesperada: %c\n", my_id, buffer[0]);
        }
    }

    close(sock);
    return 0;
}
