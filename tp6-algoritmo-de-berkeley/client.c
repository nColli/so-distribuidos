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
#define TIME_SYNC "TIME_SYNC"

typedef struct {
    int socket;
    char* server_ip;
    int server_port;
    int running;
    pthread_mutex_t sync_mutex;
} client_state_t;

client_state_t client_state = {0}; //inicializo todo en 0

long long getTimeSec();
void corregir_tiempo(long long nuevoTiempo);
void* handler_mensajes(void* arg);
void* time_display(void* arg);
void signal_handler(int signum);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s <server_ip> <server_port>\n", argv[0]);
        printf("Modificar la hora: sudo date -s '@100000' (para setear con unix time) \n");
        printf("\nEJECUTAR COMO SUDO - modifica la hora real del sistema\n");
        printf("Run with: sudo %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }

    if (geteuid() != 0) { //getuid = get user identity  
        printf("WARNING: Correr en usuario para modificar la hora real del sistema\n");
        return 1;
    }
    
    client_state.server_ip = argv[1];
    client_state.server_port = atoi(argv[2]);
    client_state.running = 1;
    client_state.socket = -1;
    
    if (client_state.server_port <= 0) {
        printf("Port invalido\n");
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
    
    printf("Conectando al demonio en %s:%d...\n", client_state.server_ip, client_state.server_port);
    
    if (connect(client_state.socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectarse con el servidor");
        close(client_state.socket);
        return 1;
    }

    printf("Tiempo inicial del sistema: %lld seconds\n", getTimeSec());
    
    // inicializar hilo - manejador de mensaje - aca modifico el tiempo del servidor
    // encargado de toda la comunicación del servidor y modiicar lo que se indique
    pthread_t hilo_mensajes;
    if (pthread_create(&hilo_mensajes, NULL, handler_mensajes, NULL) != 0) {
        perror("Error al crear hilo de mensajes");
        close(client_state.socket);
        return 1;
    }
    
    // mostrar tiempo
    pthread_t hilo_display;
    if (pthread_create(&hilo_display, NULL, time_display, NULL) != 0) {
        perror("Error al crear hilo de display");
        client_state.running = 0;
        pthread_join(hilo_mensajes, NULL); //el otro hilo ya esta creado lo tengo que cerrar tambien
        close(client_state.socket);
        return 1;
    }
    
    // Main loop - keep the client alive
    printf("\nCliente corriendo..\n");
    printf("Se sincronizara el tiempo con el demonio\n");
    printf("Ctrl+C para salir.\n\n");
    
    while (client_state.running) {
        sleep(1);
    }
    
    printf("Cerrando...\n");
    pthread_cancel(hilo_mensajes);
    pthread_cancel(hilo_display);
    // espero a que terminen
    pthread_join(hilo_mensajes, NULL);
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


// Retorna Unix timestamp
long long getTimeSec() {
    return (long long)time(NULL);
}

void corregir_tiempo(long long nuevoTiempo) {
    pthread_mutex_lock(&client_state.sync_mutex);

    struct timeval current_tv;
    if (gettimeofday(&current_tv, NULL) != 0) {
        perror("Error al obt tiempo");
        pthread_mutex_unlock(&client_state.sync_mutex);
        return;
    }

    struct timeval new_tv;
    new_tv.tv_sec = nuevoTiempo;
    new_tv.tv_usec = current_tv.tv_usec;
    
    //printf("Tiempo actual del sistema: %ld seg\n", current_tv.tv_sec);
    //printf("Nuevo tiempo: %ld seg \n", new_tv.tv_sec);
    
    if (settimeofday(&new_tv, NULL) != 0) {
        //si es != 0 => hay error de alg tipo
        if (errno == EPERM) {
            printf("ERROR: correr programa con sudo\n");
        } else {
            perror("Error desc al modificar hora");
        }
        pthread_mutex_unlock(&client_state.sync_mutex);
        return;
    }

    //printf("Tiempo del sistema modificado: %lld seg\n", getTimeSec());
    
    pthread_mutex_unlock(&client_state.sync_mutex);
}

// manejar mensajes del servidor
void* handler_mensajes(void* arg) {
    char buffer[BUFFER_SIZE];
    
    while (client_state.running) { //loop infinito para esperar recibir info del server
        int bytes_received = recv(client_state.socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            if (client_state.running) {
                printf("Conexion perdida\n");
            } else {
                break;
            }
            client_state.running = 0;
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        printf("Mensaje del demonio: '%s' (length: %d)\n", buffer, bytes_received);
        
        // comandos que envia el servidor
        // TIME_REQUEST -> devuelvo el tiempo
        // TIME_SYNC -> señal para sincronizar reloj
        if (strncmp(buffer, TIME_REQUEST, strlen(TIME_REQUEST)) == 0) {
            printf("TIME_REQUEST del servidor\n");
            long long current_time = getTimeSec();
            
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "%lld", current_time);
            
            printf("Tiempo actual: '%s'\n", response);
            
            if (send(client_state.socket, response, strlen(response), 0) < 0) {
                perror("Error al enviar tiempo al servidor");
                client_state.running = 0;
                break;
            }
            
            printf("Tiempo enviado al servidor con exito: %lld seconds\n", current_time);
            
        } else if (strncmp(buffer, TIME_SYNC, strlen(TIME_SYNC)) == 0) {
            char* nuevoTiempoStr = buffer + strlen(TIME_SYNC) + 1; //tiempo viene despues del comando
            long long nuevoTiempo = atoll(nuevoTiempoStr);
            
            printf("Nuevo ajuste de tiempo recibido del demonio\n");
            time_t nuevo_sec = (time_t)nuevoTiempo;
            struct tm* nuevo_tm = localtime(&nuevo_sec);
            printf("Nuevo tiempo: %02d/%02d/%04d %02d:%02d:%02d (%lld)\n", 
                   nuevo_tm->tm_mday, nuevo_tm->tm_mon + 1, nuevo_tm->tm_year + 1900,
                   nuevo_tm->tm_hour, nuevo_tm->tm_min, nuevo_tm->tm_sec, nuevoTiempo);
            corregir_tiempo(nuevoTiempo);
            
        } else {
            printf("Mensaje desconocido del servidor: %s\n", buffer);
        }
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