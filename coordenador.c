#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

#define MSG_SIZE 16
#define MAX_CLIENTS 1000

#define REQ '1'
#define GRA '2'
#define REL '3'

typedef struct Node {
    int process_id;
    int socket_fd;
    struct Node* next;
} Node;

Node* queue_head = NULL;
Node* queue_tail = NULL;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

int atendimentos[MAX_CLIENTS] = {0};
pthread_mutex_t atendimentos_mutex = PTHREAD_MUTEX_INITIALIZER;

int rc_livre = 1;
pthread_mutex_t rc_mutex = PTHREAD_MUTEX_INITIALIZER;

FILE* log_file;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int server_fd;
int running = 1;

void write_log(const char* type, int proc_id) {
    pthread_mutex_lock(&log_mutex);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    fprintf(log_file, "[%ld.%06ld] MSG: %s - Processo: %d\n", tv.tv_sec, tv.tv_usec, type, proc_id);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}

void enqueue(int proc_id, int socket_fd) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->process_id = proc_id;
    new_node->socket_fd = socket_fd;
    new_node->next = NULL;
    
    pthread_mutex_lock(&queue_mutex);
    if (queue_tail == NULL) {
        queue_head = new_node;
        queue_tail = new_node;
    } else {
        queue_tail->next = new_node;
        queue_tail = new_node;
    }
    pthread_mutex_unlock(&queue_mutex);
}

Node* dequeue() {
    pthread_mutex_lock(&queue_mutex);
    if (queue_head == NULL) {
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }
    Node* node = queue_head;
    queue_head = queue_head->next;
    if (queue_head == NULL) {
        queue_tail = NULL;
    }
    pthread_mutex_unlock(&queue_mutex);
    return node;
}

void print_queue() {
    pthread_mutex_lock(&queue_mutex);
    printf("Fila atual: ");
    Node* curr = queue_head;
    if (curr == NULL) {
        printf("Vazia");
    }
    while (curr != NULL) {
        printf("[%d] ", curr->process_id);
        curr = curr->next;
    }
    printf("\n");
    pthread_mutex_unlock(&queue_mutex);
}

void print_atendimentos() {
    pthread_mutex_lock(&atendimentos_mutex);
    printf("Atendimentos por processo:\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (atendimentos[i] > 0) {
            printf("Processo %d: %d vezes\n", i, atendimentos[i]);
        }
    }
    pthread_mutex_unlock(&atendimentos_mutex);
}

void* thread_recepcao(void* arg) {
    int port = *(int*)arg;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket falhou");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind falhou");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 1000) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    int client_sockets[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) client_sockets[i] = 0;

    fd_set readfds;

    while (running) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
        if (!running) break;

        if (activity < 0) continue;

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket;
            int addrlen = sizeof(address);
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] == 0) {
                        client_sockets[i] = new_socket;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char buffer[MSG_SIZE + 1];
                memset(buffer, 0, sizeof(buffer));
                
                int bytes_read = 0;
                while (bytes_read < MSG_SIZE) {
                    int b = read(sd, buffer + bytes_read, MSG_SIZE - bytes_read);
                    if (b <= 0) break;
                    bytes_read += b;
                }
                
                if (bytes_read < MSG_SIZE) {
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    char type = buffer[0];
                    char id_str[16];
                    int c = 0;
                    for (int j = 2; j < MSG_SIZE; j++) {
                        if (buffer[j] == '|') break;
                        id_str[c++] = buffer[j];
                    }
                    id_str[c] = '\0';
                    int proc_id = atoi(id_str);

                    if (type == REQ) {
                        write_log("REQUEST", proc_id);
                        enqueue(proc_id, sd);
                    } else if (type == REL) {
                        write_log("RELEASE", proc_id);
                        pthread_mutex_lock(&rc_mutex);
                        rc_livre = 1;
                        pthread_mutex_unlock(&rc_mutex);
                    }
                }
            }
        }
    }
    return NULL;
}

void* thread_algoritmo(void* arg) {
    while (running) {
        pthread_mutex_lock(&rc_mutex);
        if (rc_livre) {
            Node* next = dequeue();
            if (next != NULL) {
                rc_livre = 0; 
                
                pthread_mutex_lock(&atendimentos_mutex);
                if (next->process_id < MAX_CLIENTS) {
                    atendimentos[next->process_id]++;
                }
                pthread_mutex_unlock(&atendimentos_mutex);

                char msg[MSG_SIZE];
                memset(msg, '0', MSG_SIZE);
                char temp[MSG_SIZE];
                snprintf(temp, MSG_SIZE, "2|%d|", next->process_id);
                memcpy(msg, temp, strlen(temp));
                
                send(next->socket_fd, msg, MSG_SIZE, 0);
                write_log("GRANT", next->process_id);
                
                free(next);
            }
        }
        pthread_mutex_unlock(&rc_mutex);
        usleep(5000); 
    }
    return NULL;
}

int main() {
    log_file = fopen("log_coordenador.txt", "w");
    if (!log_file) {
        perror("Erro ao abrir log");
        return 1;
    }

    int port = 8080;
    pthread_t t_rec, t_alg;

    pthread_create(&t_rec, NULL, thread_recepcao, &port);
    pthread_create(&t_alg, NULL, thread_algoritmo, NULL);

    printf("Coordenador iniciado na porta %d.\n", port);
    printf("Comandos:\n 1 - Imprimir Fila\n 2 - Imprimir Atendimentos\n 3 - Encerrar\n> ");
    fflush(stdout);

    int cmd;
    while (running) {
        int res = scanf("%d", &cmd);
        if (res == 1) {
            if (cmd == 1) {
                print_queue();
            } else if (cmd == 2) {
                print_atendimentos();
            } else if (cmd == 3) {
                running = 0;
                printf("Encerrando\n");
                break;
            } else {
                printf("Comando invalido.\n");
            }
            printf("> ");
            fflush(stdout);
        } else if (res == EOF) {

            sleep(1);
        } else {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
    }

    pthread_join(t_rec, NULL);
    pthread_join(t_alg, NULL);
    
    close(server_fd);
    fclose(log_file);
    return 0;
}
