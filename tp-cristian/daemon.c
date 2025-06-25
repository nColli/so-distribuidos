#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define TIME_REQUEST "TIME_REQUEST"

typedef struct {
    int socket;
    struct sockaddr_in address;
    int active;
    pthread_t thread;
} client_info_t;

typedef struct {
    client_info_t clients[MAX_CLIENTS];
    int server_socket;
    int running;
    pthread_mutex_t clients_mutex;
} server_state_t;

server_state_t server_state = {0};

long long getTimeSec();
void* handle_client(void* arg);
void signal_handler(int signum);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s <port>\n", argv[0]);
        printf("Daemon para algoritmo de Cristian - responde con tiempo actual\n");
        return 1;
    }
    
    int port = atoi(argv[1]);
    server_state.running = 1;
    
    if (port <= 0) {
        printf("Puerto invalido\n");
        return 1;
    }

    server_state.server_socket = -1;
    pthread_mutex_init(&server_state.clients_mutex, NULL);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        server_state.clients[i].active = 0;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    server_state.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_state.server_socket < 0) {
        perror("Error al crear socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_state.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt fallo");
        close(server_state.server_socket);
        return 1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_state.server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error bind socket");
        close(server_state.server_socket);
        return 1;
    }

    if (listen(server_state.server_socket, MAX_CLIENTS) < 0) {
        perror("Error listen socket");
        close(server_state.server_socket);
        return 1;
    }
    
    printf("Daemon Cristian iniciado en puerto %d\n", port);
    
    long long initial_time = getTimeSec();
    time_t initial_sec = (time_t)initial_time;
    struct tm* initial_tm = localtime(&initial_sec);
    printf("Tiempo inicial del servidor: %02d/%02d/%04d %02d:%02d:%02d (%lld)\n", 
           initial_tm->tm_mday, initial_tm->tm_mon + 1, initial_tm->tm_year + 1900,
           initial_tm->tm_hour, initial_tm->tm_min, initial_tm->tm_sec, initial_time);
    
    // Aceptar conexiones de los clientes
    while (server_state.running) {
        socklen_t client_len = sizeof(struct sockaddr_in);
        struct sockaddr_in client_addr;
        
        int client_socket = accept(server_state.server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (server_state.running) {
                perror("Error al aceptar la conexion");
            } else {
                break;
            }
            continue;
        }

        pthread_mutex_lock(&server_state.clients_mutex);
        
        // Encontrar lugar para almacenar nuevo cliente
        int client_index = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!server_state.clients[i].active) {
                client_index = i;
                break;
            }
        }
        
        if (client_index == -1) {
            printf("Numero maximo de clientes alcanzados, conexion rechazada\n");
            close(client_socket);
            pthread_mutex_unlock(&server_state.clients_mutex);
            continue;
        }
        
        // Configurar cliente
        server_state.clients[client_index].socket = client_socket;
        server_state.clients[client_index].address = client_addr;
        server_state.clients[client_index].active = 1;
        
        printf("Cliente %d conectado desde %s\n", client_index, inet_ntoa(client_addr.sin_addr));
        
        // Crear hilo para manejar este cliente
        if (pthread_create(&server_state.clients[client_index].thread, NULL, handle_client, &server_state.clients[client_index]) != 0) {
            perror("Error crear hilo para cliente");
            close(client_socket);
            server_state.clients[client_index].active = 0;
        }
        
        pthread_mutex_unlock(&server_state.clients_mutex);
    }

    printf("Cerrando daemon...\n");

    // Cerrar todas las conexiones de los clientes
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server_state.clients[i].active) {
            close(server_state.clients[i].socket);
            pthread_cancel(server_state.clients[i].thread);
            pthread_join(server_state.clients[i].thread, NULL);
        }
    }
    
    if (server_state.server_socket >= 0) {
        close(server_state.server_socket);
        server_state.server_socket = -1;
    }
    
    pthread_mutex_destroy(&server_state.clients_mutex);
    
    printf("Daemon cerrado\n");
    return 0;
}

long long getTimeSec() {
    return (long long)time(NULL);
}

void* handle_client(void* arg) {
    client_info_t* client = (client_info_t*)arg;
    char buffer[BUFFER_SIZE];
    
    while (server_state.running && client->active) {
        int bytes_received = recv(client->socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Cliente desconectado\n");
            } else {
                perror("Error al recibir datos del cliente");
            }
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        // Procesar solicitud de tiempo
        if (strncmp(buffer, TIME_REQUEST, strlen(TIME_REQUEST)) == 0) {
            long long current_time = getTimeSec();
            
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "%lld", current_time);
            
            if (send(client->socket, response, strlen(response), 0) < 0) {
                perror("Error al enviar tiempo al cliente");
                break;
            }
            
            printf("Tiempo enviado a cliente: %lld\n", current_time);
        } else {
            printf("Solicitud desconocida del cliente: %s\n", buffer);
        }
    }
    
    // Limpiar cliente
    close(client->socket);
    client->active = 0;
    
    return NULL;
}

void signal_handler(int signum) {
    printf("\nCerrando servidor...\n");
    server_state.running = 0;

    if (server_state.server_socket >= 0) {
        close(server_state.server_socket);
        server_state.server_socket = -1;
    }
}
