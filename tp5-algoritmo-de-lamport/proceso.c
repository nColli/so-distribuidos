#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>

#define MAX_MSG 128

pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;
int logical_clock;

void error(const char *msg);
int max(int a, int b);
void* clock_thread(void* arg);
void *server_thread(void *arg);
void *input_thread(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <puerto> <reloj_inicial> <intervalo_reloj>\n", argv[0]);
        exit(1);
    }
    int port = atoi(argv[1]);
    logical_clock = atoi(argv[2]);
    int interval = atoi(argv[3]);

    pthread_t clock_tid;
    pthread_create(&clock_tid, NULL, clock_thread, &interval);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[MAX_MSG];

    // Crear socket de servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) error("socket failed");
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        error("setsockopt");
        close(server_fd);
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        error("bind error");
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 3) < 0) {
        error("error listen");
        close(server_fd);
        return 1;
    }

    //stdin no bloqueante
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);

    printf("Proceso iniciado en el puerto %d con logical clock %d\n", port, logical_clock);
    printf("Ingresa un numero de puerto y presiona enter para enviar el mensaje.\n");

    pthread_t server_tid, input_tid;
    
    if (pthread_create(&server_tid, NULL, server_thread, &server_fd) != 0) {
        error("pthread_create server_thread");
    }
    
    if (pthread_create(&input_tid, NULL, input_thread, NULL) != 0) {
        error("pthread_create input_thread");
    }
    
    pthread_join(server_tid, NULL);
    pthread_join(input_tid, NULL);

    pthread_join(clock_tid, NULL);
    close(server_fd);
    return 0;
}


void error(const char *msg) {
    perror(msg);
    exit(1);
}

int max(int a, int b) {
    return a > b ? a : b;
}

void* clock_thread(void* arg) {
    int interval = *((int*)arg);
    while(1) {
        sleep(interval);
        pthread_mutex_lock(&clock_mutex);
        int old_clock = logical_clock;
        logical_clock++;
        printf("[TICK] Clock automatico: %d -> %d\n", old_clock, logical_clock);
        pthread_mutex_unlock(&clock_mutex);
    }
    return NULL;
}

void *server_thread(void *arg) {
    int server_fd = *(int*)arg;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[MAX_MSG];
    int new_socket;
    
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        memset(buffer, 0, MAX_MSG);
        int valread = read(new_socket, buffer, MAX_MSG);
        if (valread > 0) {
            int recv_clock;
            sscanf(buffer, "%d", &recv_clock);
            pthread_mutex_lock(&clock_mutex);
            int old_clock = logical_clock;
            logical_clock = max(logical_clock, recv_clock) + 1;
            printf("[RECV] Clock recibido=%d | Mi clock: %d -> %d\n", recv_clock, old_clock, logical_clock);
            pthread_mutex_unlock(&clock_mutex);
        }
        close(new_socket);
    }
    return NULL;
}

void *input_thread(void *arg) {
    char input[32];
    while (1) {
        if (fgets(input, sizeof(input), stdin)) {
            int dest_port = atoi(input);
            if (dest_port > 0) {
                int sock = 0;
                struct sockaddr_in serv_addr;
                char msg[MAX_MSG];
                if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    printf("Error al crear el socket\n");
                    continue;
                }
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(dest_port);
                serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                    printf("Conexion port %d fallo\n", dest_port);
                    close(sock);
                    continue;
                }
                pthread_mutex_lock(&clock_mutex);
                logical_clock++;
                snprintf(msg, MAX_MSG, "%d", logical_clock);
                printf("[SEND] A puerto %d: clock=%d\n", dest_port, logical_clock);
                pthread_mutex_unlock(&clock_mutex);
                send(sock, msg, strlen(msg), 0);
                close(sock);
            }
        }
    }
    return NULL;
}