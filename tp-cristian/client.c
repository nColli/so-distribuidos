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

#define BUFFER_SIZE 1024
#define TIME_REQUEST "TIME_REQUEST"

typedef struct {
    int socket;
    char* server_ip;
    int server_port;
    int sync_interval;
    int running;
    pthread_mutex_t sync_mutex;
} client_state_t;

client_state_t client_state = {0}; //inicializo todo en 0

long long getTimeSec();
long long getTimeUsec();
void corregir_tiempo(long long nuevoTiempo);
void* cristian_sync(void* arg);
void* time_display(void* arg);
void signal_handler(int signum);

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Uso: %s <server_ip> <server_port> <sync_interval_seconds>\n", argv[0]);
        printf("Algoritmo de Cristian - sincronizacion con servidor de tiempo\n");
        printf("Modificar la hora: sudo date -s '@100000' (para setear con unix time) \n");
        printf("\nEJECUTAR COMO SUDO - modifica la hora real del sistema\n");
        printf("Run with: sudo %s <server_ip> <server_port> <sync_interval>\n", argv[0]);
        return 1;
    }

    if (geteuid() != 0) { //getuid = get user identity  
        printf("WARNING: Correr como SUDO para modificar la hora real del sistema\n");
        return 1;
    }
    
    client_state.server_ip = argv[1];
    client_state.server_port = atoi(argv[2]);
    client_state.sync_interval = atoi(argv[3]);
    client_state.running = 1;
    client_state.socket = -1;
    
    if (client_state.server_port <= 0 || client_state.sync_interval <= 0) {
        printf("Puerto o intervalo de sincronizacion invalidos\n");
        return 1;
    }
    
    // inicializar mutex - como semaforo binario - exclusion mutua 
    // exclusion mutua - tecnica sincro hilo acceda a un recurso compartido a la vez
    pthread_mutex_init(&client_state.sync_mutex, NULL);
    
    // conf handler de señales - cerrar sin errores con CTRL C
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    client_state.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_state.socket < 0) {
        perror("Failed to create socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client_state.server_port);
    
    if (inet_pton(AF_INET, client_state.server_ip, &server_addr.sin_addr) <= 0) {
        printf("Dir IP invalida: %s\n", client_state.server_ip);
        close(client_state.socket);
        return 1;
    }
    
    printf("Conectando al daemon en %s:%d...\n", client_state.server_ip, client_state.server_port);
    
    if (connect(client_state.socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectarse con el servidor");
        close(client_state.socket);
        return 1;
    }

    printf("Tiempo inicial del sistema: %lld seconds\n", getTimeSec());
    printf("Intervalo de sincronizacion: %d segundos\n", client_state.sync_interval);
    
    // inicializar hilo - sincronizacion Cristian
    pthread_t hilo_sync;
    if (pthread_create(&hilo_sync, NULL, cristian_sync, NULL) != 0) {
        perror("Error al crear hilo de sincronizacion");
        close(client_state.socket);
        return 1;
    }
    
    // mostrar tiempo
    pthread_t hilo_display;
    if (pthread_create(&hilo_display, NULL, time_display, NULL) != 0) {
        perror("Error al crear hilo de display");
        client_state.running = 0;
        pthread_join(hilo_sync, NULL);
        close(client_state.socket);
        return 1;
    }
    
    // Main loop
    printf("\nCliente Cristian corriendo...\n");
    printf("Se sincronizara cada %d segundos con el daemon\n", client_state.sync_interval);
    printf("Ctrl+C para salir.\n\n");
    
    while (client_state.running) {
        sleep(1);
    }
    
    printf("Cerrando...\n");
    pthread_cancel(hilo_sync);
    pthread_cancel(hilo_display);
    // espero a que terminen
    pthread_join(hilo_sync, NULL);
    pthread_join(hilo_display, NULL);
    // cerrar socket aunque lo puede haber cerrado el manejador de señales
    if (client_state.socket >= 0) {
        close(client_state.socket);
        client_state.socket = -1;
    }    
    // eliminar mutex
    pthread_mutex_destroy(&client_state.sync_mutex);
    printf("Cliente cerrado\n");
    return 0;
}

// Retorna Unix timestamp en segundos
long long getTimeSec() {
    return (long long)time(NULL);
}

// Retorna tiempo en microsegundos para medir latencia
long long getTimeUsec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec * 1000000LL + tv.tv_usec);
}

void corregir_tiempo(long long nuevoTiempo) {
    pthread_mutex_lock(&client_state.sync_mutex);

    struct timeval current_tv;
    if (gettimeofday(&current_tv, NULL) != 0) {
        perror("Error al obtener tiempo");
        pthread_mutex_unlock(&client_state.sync_mutex);
        return;
    }

    struct timeval new_tv;
    new_tv.tv_sec = nuevoTiempo;
    new_tv.tv_usec = current_tv.tv_usec;
    
    if (settimeofday(&new_tv, NULL) != 0) {
        //si es != 0 => hay error de alg tipo
        if (errno == EPERM) {
            printf("ERROR: correr programa con sudo\n");
        } else {
            perror("Error desconocido al modificar hora");
        }
        pthread_mutex_unlock(&client_state.sync_mutex);
        return;
    }

    printf("Tiempo del sistema sincronizado: %lld\n", nuevoTiempo);
    
    pthread_mutex_unlock(&client_state.sync_mutex);
}

// Algoritmo de Cristian - solicitar tiempo y calcular latencia
void* cristian_sync(void* arg) {
    while (client_state.running) {
        printf("\n=== Iniciando sincronizacion Cristian ===\n");
        
        // Tomar tiempo de inicio (T1)
        long long start_time_usec = getTimeUsec();
        
        // Enviar solicitud de tiempo
        if (send(client_state.socket, TIME_REQUEST, strlen(TIME_REQUEST), 0) < 0) {
            perror("Error al enviar solicitud de tiempo");
            client_state.running = 0;
            break;
        }
        
        printf("Solicitud TIME_REQUEST enviada al daemon\n");
        
        // Recibir respuesta del servidor
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(client_state.socket, buffer, sizeof(buffer) - 1, 0);
        
        // Tomar tiempo de fin (T2)
        long long end_time_usec = getTimeUsec();
        
        if (bytes_received <= 0) {
            printf("Error al recibir tiempo del daemon o conexion perdida\n");
            client_state.running = 0;
            break;
        }
        
        buffer[bytes_received] = '\0';
        long long server_time = atoll(buffer);
        
        // Calcular latencia (RTT - Round Trip Time)
        long long rtt_usec = end_time_usec - start_time_usec;
        long long rtt_sec = rtt_usec / 1000000LL;
        long long estimated_latency_sec = rtt_sec / 2; // Latencia estimada (RTT/2)
        
        printf("Tiempo del servidor recibido: %lld\n", server_time);
        printf("RTT (Round Trip Time): %lld microsegundos (%lld.%06lld segundos)\n", 
               rtt_usec, rtt_sec, rtt_usec % 1000000);
        printf("Latencia estimada: %lld segundos\n", estimated_latency_sec);
        
        // Ajustar tiempo: tiempo_servidor + latencia_estimada
        long long adjusted_time = server_time + estimated_latency_sec;
        
        // Mostrar informacion de sincronizacion
        time_t adj_sec = (time_t)adjusted_time;
        struct tm* adj_tm = localtime(&adj_sec);
        printf("Tiempo ajustado a aplicar: %02d/%02d/%04d %02d:%02d:%02d (%lld)\n", 
               adj_tm->tm_mday, adj_tm->tm_mon + 1, adj_tm->tm_year + 1900,
               adj_tm->tm_hour, adj_tm->tm_min, adj_tm->tm_sec, adjusted_time);
        
        // Aplicar el nuevo tiempo
        corregir_tiempo(adjusted_time);
        
        printf("=== Sincronizacion Cristian completada ===\n");
        
        // Esperar el intervalo especificado antes de la siguiente sincronizacion
        printf("Esperando %d segundos para la proxima sincronizacion...\n", client_state.sync_interval);
        sleep(client_state.sync_interval);
    }
    
    return NULL;
}

// mostrar tiempo cada cierto tiempo
void* time_display(void* arg) {
    while (client_state.running) {
        sleep(3); // Update every 3 seconds
        
        long long current_time = getTimeSec();
        
        // convertir a formato legible
        time_t current_sec = (time_t)current_time;
        struct tm* current_tm = localtime(&current_sec);
        
        printf("\n--- Tiempo actual del sistema ---\n");
        printf("Fecha y tiempo del sistema: %02d/%02d/%04d %02d:%02d:%02d (%lld)\n", 
               current_tm->tm_mday, current_tm->tm_mon + 1, current_tm->tm_year + 1900,
               current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec, current_time);
        printf("----------------------------------\n\n");
    }
    
    return NULL;
}

// Para evitar errores al cerrar con ctrl C
void signal_handler(int signum) {
    printf("\nCerrando cliente..\n");
    client_state.running = 0; //var estado chequea para mandar y recv request
    
    // Cerrar socket para interrumpir recvs
    if (client_state.socket >= 0) {
        close(client_state.socket);
        client_state.socket = -1;
    }
}