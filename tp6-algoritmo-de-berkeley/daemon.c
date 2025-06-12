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

#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024
#define TIME_REQUEST "TIME_REQUEST"
#define TIME_SYNC "TIME_SYNC"

typedef struct {
    int socket;
    struct sockaddr_in address;
    int active;
    pthread_mutex_t mutex;
} client_info_t;

typedef struct {
    client_info_t clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t clients_mutex;
    int server_socket;
    int sync_interval;
    int running;
    pthread_mutex_t time_mutex;
} server_state_t;

server_state_t server_state = {0};

long long getTimeSec();
void modificar_tiempo_servidor(long long newTime);
void send_time_sync(int client_socket, long long newTime);
long long request_client_time(int client_socket);
void* synchronize_time(void* arg);
void setup_client_connection(int client_index);
void signal_handler(int signum);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s <port> <intervalo_sincro_seg>\n", argv[0]);
        printf("\nNota: ejecutar con SUDO\n");
        return 1;
    }
    
    if (geteuid() != 0) {
        printf("WARNING: Correr como SUDO\n");
        return 1;
    }
    
    int port = atoi(argv[1]);
    server_state.sync_interval = atoi(argv[2]);
    server_state.running = 1;
    
    if (port <= 0 || server_state.sync_interval <= 0) {
        printf("Puerto o intervalo de sincro invalidos\n");
        return 1;
    }

    server_state.server_socket = -1;
    pthread_mutex_init(&server_state.clients_mutex, NULL);
    pthread_mutex_init(&server_state.time_mutex, NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_mutex_init(&server_state.clients[i].mutex, NULL);
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
    
    printf("Servidor iniciado en %d con intervalo de sincronismo de %d seg\n", port, server_state.sync_interval);
    
    long long initial_time = getTimeSec();
    time_t initial_sec = (time_t)initial_time;
    struct tm* initial_tm = localtime(&initial_sec);
    printf("Tiempo inicial del servidor: %02d/%02d/%04d %02d:%02d:%02d (%lld)\n", 
           initial_tm->tm_mday, initial_tm->tm_mon + 1, initial_tm->tm_year + 1900,
           initial_tm->tm_hour, initial_tm->tm_min, initial_tm->tm_sec, initial_time);
    
    // Iniciar hilo de sincronizacion
    pthread_t sync_thread;
    if (pthread_create(&sync_thread, NULL, synchronize_time, NULL) != 0) {
        perror("Error crear hilo de sincronizacion");
        close(server_state.server_socket);
        return 1;
    }
    
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
        
        // Encontrar lugar para almacenar nuevo cliente activo
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
        
        // guardo iunfo del cliente - para eso necesito bloquear acceso con mutex clientes
        server_state.clients[client_index].socket = client_socket;
        server_state.clients[client_index].address = client_addr;
        server_state.clients[client_index].active = 1;
        
        if (client_index >= server_state.client_count) {
            server_state.client_count = client_index + 1;
        }
        
        setup_client_connection(client_index);
        
        pthread_mutex_unlock(&server_state.clients_mutex);
    }

    printf("Cerrando conexion..\n");

    if (server_state.server_socket >= 0) {
        close(server_state.server_socket);
        server_state.server_socket = -1;
    }
    pthread_join(sync_thread, NULL);
    
    // Cerrar todas las conexiones de los clientes
    for (int i = 0; i < server_state.client_count; i++) {
        if (server_state.clients[i].active) {
            close(server_state.clients[i].socket);
        }
        pthread_mutex_destroy(&server_state.clients[i].mutex);
    }
    
    pthread_mutex_destroy(&server_state.clients_mutex);
    pthread_mutex_destroy(&server_state.time_mutex);
    
    printf("Servidor cerrado\n");
    return 0;
}

long long getTimeSec() {
    return (long long)time(NULL);
}

void modificar_tiempo_servidor(long long nuevoTiempo) {
    pthread_mutex_lock(&server_state.time_mutex);
    
    struct timeval current_tv;
    if (gettimeofday(&current_tv, NULL) != 0) {
        perror("Error obtener tiempo actual del servidor");
        pthread_mutex_unlock(&server_state.time_mutex);
        return;
    }

    struct timeval new_tv;
    new_tv.tv_sec = nuevoTiempo;
    new_tv.tv_usec = current_tv.tv_usec;
    
    //printf("Tiempo actual del servidor: %ld seg\n", current_tv.tv_sec);
    //printf("Aplicando a servidor nuevo timepo: %lld seg\n", new_tv.tv_sec);

    if (settimeofday(&new_tv, NULL) != 0) {
        if (errno == EPERM) {
            printf("ERROR: ejecutar como sudo\n");
        } else {
            perror("Error config tiempo del servidor");
        }
        pthread_mutex_unlock(&server_state.time_mutex);
        return;
    }
    
    printf("Tiempo del servidor modificado\n");

    //long long new_system_time = getTimeSec();
    //printf("Verificacion nuevo tiempo: %lld seg\n", new_system_time);

    long long current_time = getTimeSec();
    time_t current_sec = (time_t)current_time;
    struct tm* current_tm = localtime(&current_sec);
    printf("Tiempo del servidor: %02d/%02d/%04d %02d:%02d:%02d (%lld)\n", 
           current_tm->tm_mday, current_tm->tm_mon + 1, current_tm->tm_year + 1900,
           current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec, current_time);
    
    pthread_mutex_unlock(&server_state.time_mutex);
}

void send_time_sync(int client_socket, long long newTime) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s %lld", TIME_SYNC, newTime); //TIME_SYNC 1032131312 - comando nuevoTiempo
    
    if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
        perror("Error enviar tiempo");
    }
}

long long request_client_time(int client_socket) {
    char buffer[BUFFER_SIZE];
    long long client_time = 0;
    
    // temporalmente se bloquea para comunicación sincronica
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags & ~O_NONBLOCK);
    
    // Set timeout para recv para evitar bloqueo infinito
    struct timeval timeout;
    timeout.tv_sec = 3;  // 3 seg de timeout
    timeout.tv_usec = 0;
    
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Timeout conexion con cliente");
    }
    
    // Send time request
    if (send(client_socket, TIME_REQUEST, strlen(TIME_REQUEST), 0) < 0) {
        perror("Error al enviar peticion");
        return -1;
    }
    
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        client_time = atoll(buffer);
        
        // poner de nuevo en no bloqueante
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
        return client_time;
    } else if (bytes_received == 0) {
        printf("Conexion con cliente cerrada\n");
    } else {
        perror("Error al recibir tiempo del cliente");
    }
    
    // poner de nuevo en no bloqueante aunque haya error - se toma como cliente esta inactivo
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
    return -1;
}

// algoritmo de sincronizacion
void* synchronize_time(void* arg) {
    while (server_state.running) {
        printf("Esperar %d segundos\n", server_state.sync_interval);
        
        /*unsigned int remaining = server_state.sync_interval;
        while (remaining > 0 && server_state.running) {
            remaining = sleep(remaining);
            if (remaining > 0) {
                printf("Sleep interrumpido, continuando por %u segundos mas\n", remaining);
            }
        }*/

       sleep(server_state.sync_interval);
        
        pthread_mutex_lock(&server_state.clients_mutex);
        
        if (server_state.client_count == 0) {
            pthread_mutex_unlock(&server_state.clients_mutex);
            continue;
        }
        
        printf("\n=== Iniciando ronda de sincronizacion ===\n");
        
        long long local_time = getTimeSec();
        time_t local_sec = (time_t)local_time;
        struct tm* local_tm = localtime(&local_sec);
        printf("Tiempo del servidor: %02d/%02d/%04d %02d:%02d:%02d (%lld)\n", 
               local_tm->tm_mday, local_tm->tm_mon + 1, local_tm->tm_year + 1900,
               local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec, local_time);
        
        // obtener tiempos de todos los clientes activos
        long long times[MAX_CLIENTS + 1]; // +1 por el servidor
        int valid_times = 1; // empiezo a guardar en 1, porque en 0 guardo el tiempo local
        times[0] = local_time;
        
        for (int i = 0; i < server_state.client_count; i++) {
            if (server_state.clients[i].active) {
                printf("Peticion tiempo del cliente %d (socket %d)...\n", i, server_state.clients[i].socket);
                long long client_time = request_client_time(server_state.clients[i].socket);
                if (client_time != -1) {
                    times[valid_times] = client_time;
                    valid_times++;
                    printf("Tiempo del cliento %d: %lld\n", i, client_time);
                } else {
                    // si no se pudo obtener el tiempo, se toma como cliente inactivo
                    server_state.clients[i].active = 0;
                    printf("Cliente %d marcado como inactivo ERROR al obt tiempo\n", i);
                }
            } else {
                printf("Cliente %d no activo, saltea\n", i);
            }
        }
        /*
        if (valid_times < 2) {
            printf("Not enough active clients for synchronization\n");
            pthread_mutex_unlock(&server_state.clients_mutex);
            continue;
        }*/
        
        // calcular tiempo promedio
        long long total_time = 0;
        for (int i = 0; i < valid_times; i++) {
            total_time += times[i];
        }
        long long average_time = total_time / valid_times;
        
        time_t avg_sec = (time_t)average_time;
        struct tm* avg_tm = localtime(&avg_sec);
        printf("Tiempo promedio calculado: %02d/%02d/%04d %02d:%02d:%02d (%lld)\n", 
               avg_tm->tm_mday, avg_tm->tm_mon + 1, avg_tm->tm_year + 1900,
               avg_tm->tm_hour, avg_tm->tm_min, avg_tm->tm_sec, average_time);
        //printf("Numero de nodos (serv + clientes) en calculo: %d\n", valid_times);
        
        //enviar tiempo calculado y fijarlo
        int client_index = 0;
        for (int i = 0; i < server_state.client_count; i++) {
            if (server_state.clients[i].active) {
                client_index++; // index va uno adelantado porque primero es el del server
                if (client_index < valid_times) {
                    printf("Enviando correccion %lld a cliente %d\n", average_time, i);
                    send_time_sync(server_state.clients[i].socket, average_time);
                }
            }
        }

        modificar_tiempo_servidor(average_time);


        printf("=== Sincronizacion completada ===\n\n");
        
        pthread_mutex_unlock(&server_state.clients_mutex);
    }
    return NULL;
}

void setup_client_connection(int client_index) {
    printf("Cliente %d conectado desde %s\n", client_index, inet_ntoa(server_state.clients[client_index].address.sin_addr));
    // Setear socket como no bloqueante - servidor pueda manejar multiples clientes sin trabarse esperando la respuesta de uno
    // para concurrencia
    int flags = fcntl(server_state.clients[client_index].socket, F_GETFL, 0);
    fcntl(server_state.clients[client_index].socket, F_SETFL, flags | O_NONBLOCK);
}

void signal_handler(int signum) {
    printf("\nCerrando servidor...\n");
    server_state.running = 0;

    if (server_state.server_socket >= 0) {
        close(server_state.server_socket);
        server_state.server_socket = -1;
    }
}
