// servidor_dns_nuevo.c - Servidor DNS para el sistema de archivos distribuido
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include "common_nuevo.h"

// Variables globales
EntradaElemento tablaElementos[MAX_FILES];
int contadorElementos = 0;
pthread_mutex_t mutex_tabla = PTHREAD_MUTEX_INITIALIZER;

void mostrar_tabla_elementos();
int registrar_elemento(const char* nombre, const char* ip, int puerto, TipoElemento tipo);
int desbloquear_elemento(const char* nombre);
void* manejar_cliente(void* arg);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s <ip> <puerto>\n", argv[0]);
        printf("Ejemplo: %s 127.0.0.1 9000\n", argv[0]);
        return 1;
    }
    
    const char* ip = argv[1];
    int puerto = atoi(argv[2]);
    
    int socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor < 0) {
        perror("Error creando socket");
        return 1;
    }
    
    // Permitir reutilizar la dirección
    int opt = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in direccion = {0};
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);
    inet_pton(AF_INET, ip, &direccion.sin_addr);
    
    if (bind(socket_servidor, (struct sockaddr*)&direccion, sizeof(direccion)) < 0) {
        perror("Error en bind");
        return 1;
    }
    
    listen(socket_servidor, 10);
    printf("[DNS] Servidor DNS ejecutándose en %s:%d\n", ip, puerto);
    
    while (1) {
        struct sockaddr_in direccion_cliente;
        socklen_t tam_direccion = sizeof(direccion_cliente);
        int socket_cliente = accept(socket_servidor, (struct sockaddr*)&direccion_cliente, &tam_direccion);
        
        if (socket_cliente < 0) {
            perror("Error en accept");
            continue;
        }
        
        // Crear hilo para manejar cliente
        DatosHilo* datos = malloc(sizeof(DatosHilo));
        datos->socket_cliente = socket_cliente;
        
        pthread_t hilo;
        pthread_create(&hilo, NULL, manejar_cliente, datos);
        pthread_detach(hilo);
    }
    
    close(socket_servidor);
    return 0;
} 


// Función para mostrar la tabla de elementos
void mostrar_tabla_elementos() {
    pthread_mutex_lock(&mutex_tabla);
    printf("\n=== Tabla Actual de Elementos ===\n");
    printf("%-25s %-16s %-6s %-4s %-8s\n", "Nombre", "IP", "Puerto", "Bloq", "Tipo");
    printf("----------------------------------------------------------------\n");
    for (int i = 0; i < contadorElementos; i++) {
        printf("%-25s %-16s %-6d %-4d %-8s\n", 
               tablaElementos[i].nombre, 
               tablaElementos[i].ip, 
               tablaElementos[i].puerto, 
               tablaElementos[i].bloqueado,
               (tablaElementos[i].tipo == TIPO_ARCHIVO) ? "ARCHIVO" : "CARPETA");
    }
    printf("----------------------------------------------------------------\n");
    pthread_mutex_unlock(&mutex_tabla);
}

// Función para registrar un elemento (archivo o carpeta)
int registrar_elemento(const char* nombre, const char* ip, int puerto, TipoElemento tipo) {
    pthread_mutex_lock(&mutex_tabla);
    
    // Verificar duplicados
    for (int i = 0; i < contadorElementos; i++) {
        if (strcmp(tablaElementos[i].nombre, nombre) == 0) {
            // Permitir re-registro si es el mismo servidor y está desbloqueado
            if (tablaElementos[i].bloqueado == 0 && 
                strcmp(tablaElementos[i].ip, ip) == 0 && 
                tablaElementos[i].puerto == puerto) {
                printf("[DNS] Re-registrando elemento: %s en %s:%d\n", nombre, ip, puerto);
                pthread_mutex_unlock(&mutex_tabla);
                return 1;
            } else {
                printf("[DNS] Elemento duplicado: %s\n", nombre);
                pthread_mutex_unlock(&mutex_tabla);
                return 0;
            }
        }
    }
    
    // Registrar nuevo elemento
    if (contadorElementos < MAX_FILES) {
        strcpy(tablaElementos[contadorElementos].nombre, nombre);
        strcpy(tablaElementos[contadorElementos].ip, ip);
        tablaElementos[contadorElementos].puerto = puerto;
        tablaElementos[contadorElementos].bloqueado = 0;
        tablaElementos[contadorElementos].tipo = tipo;
        contadorElementos++;
        printf("[DNS] Elemento registrado: %s (%s) en %s:%d\n", 
               nombre, (tipo == TIPO_ARCHIVO) ? "archivo" : "carpeta", ip, puerto);
        pthread_mutex_unlock(&mutex_tabla);
        return 1;
    }
    
    pthread_mutex_unlock(&mutex_tabla);
    return 0;
}

// Función para desbloquear un elemento
int desbloquear_elemento(const char* nombre) {
    pthread_mutex_lock(&mutex_tabla);
    for (int i = 0; i < contadorElementos; i++) {
        if (strcmp(tablaElementos[i].nombre, nombre) == 0) {
            tablaElementos[i].bloqueado = 0;
            printf("[DNS] Elemento desbloqueado: %s\n", nombre);
            pthread_mutex_unlock(&mutex_tabla);
            return 1;
        }
    }
    pthread_mutex_unlock(&mutex_tabla);
    return 0;
}

// Función para manejar clientes en hilos separados
void* manejar_cliente(void* arg) {
    DatosHilo* datos = (DatosHilo*)arg;
    int socket_cliente = datos->socket_cliente;
    char buffer[BUF_SIZE];
    
    int n = recv(socket_cliente, buffer, BUF_SIZE - 1, 0);
    if (n <= 0) {
        close(socket_cliente);
        free(datos);
        return NULL;
    }
    
    buffer[n] = '\0';
    printf("[DNS] Solicitud recibida: %s\n", buffer);
    
    if (strncmp(buffer, "REGISTRAR_ELEMENTO", 18) == 0) {
        // Formato: REGISTRAR_ELEMENTO nombre ip puerto tipo
        char nombre[MAX_FILENAME], temp_ip[MAX_IP], tipo_str[16];
        int puerto;
        sscanf(buffer, "REGISTRAR_ELEMENTO %s %s %d %s", nombre, temp_ip, &puerto, tipo_str);
        
        // Por defecto, usar IP local
        const char* ip = "127.0.0.1";
        
        TipoElemento tipo = (strcmp(tipo_str, "CARPETA") == 0) ? TIPO_CARPETA : TIPO_ARCHIVO;
        
        if (registrar_elemento(nombre, ip, puerto, tipo)) {
            send(socket_cliente, "OK\n", 3, 0);
        } else {
            send(socket_cliente, "DUPLICADO\n", 10, 0);
        }
        mostrar_tabla_elementos();
        
    } else if (strncmp(buffer, "SOLICITAR_ELEMENTO", 18) == 0) {
        // Formato: SOLICITAR_ELEMENTO nombre comando
        char nombre[MAX_FILENAME], comando[8];
        sscanf(buffer, "SOLICITAR_ELEMENTO %s %s", nombre, comando);
        printf("[DNS] Cliente solicita elemento '%s' con comando '%s'\n", nombre, comando);
        
        pthread_mutex_lock(&mutex_tabla);
        for (int i = 0; i < contadorElementos; i++) {
            if (strcmp(tablaElementos[i].nombre, nombre) == 0) {
                char respuesta[BUF_SIZE];
                if (strcmp(comando, "ESCRIBIR") == 0 && tablaElementos[i].bloqueado == 0) {
                    tablaElementos[i].bloqueado = 1;
                    snprintf(respuesta, BUF_SIZE, "OK %s %d %s\n", 
                            tablaElementos[i].ip, 
                            tablaElementos[i].puerto,
                            (tablaElementos[i].tipo == TIPO_ARCHIVO) ? "ARCHIVO" : "CARPETA");
                } else if (strcmp(comando, "LEER") == 0) {
                    snprintf(respuesta, BUF_SIZE, "OK %s %d %s\n", 
                            tablaElementos[i].ip, 
                            tablaElementos[i].puerto,
                            (tablaElementos[i].tipo == TIPO_ARCHIVO) ? "ARCHIVO" : "CARPETA");
                } else {
                    snprintf(respuesta, BUF_SIZE, "BLOQUEADO\n");
                }
                send(socket_cliente, respuesta, strlen(respuesta), 0);
                printf("[DNS] Respuesta enviada: %s", respuesta);
                pthread_mutex_unlock(&mutex_tabla);
                mostrar_tabla_elementos();
                close(socket_cliente);
                free(datos);
                return NULL;
            }
        }
        pthread_mutex_unlock(&mutex_tabla);
        send(socket_cliente, "NO_ENCONTRADO\n", 14, 0);
        
    } else if (strncmp(buffer, "DESBLOQUEAR_ELEMENTO", 20) == 0) {
        // Formato: DESBLOQUEAR_ELEMENTO nombre
        char nombre[MAX_FILENAME];
        sscanf(buffer, "DESBLOQUEAR_ELEMENTO %s", nombre);
        
        if (desbloquear_elemento(nombre)) {
            send(socket_cliente, "DESBLOQUEADO\n", 13, 0);
        } else {
            send(socket_cliente, "NO_ENCONTRADO\n", 14, 0);
        }
        mostrar_tabla_elementos();
    }
    
    close(socket_cliente);
    free(datos);
    return NULL;
}