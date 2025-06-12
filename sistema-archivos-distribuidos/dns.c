// dns.c - Servidor DNS para sistema de archivos distribuido
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

#define MAX_SERVERS 50
#define MAX_FILES 1000
#define MAX_FILENAME 256
#define MAX_IP 16
#define BUF_SIZE 1024

// Estructura para servidores de archivos
typedef struct {
    char ip[MAX_IP];
    int puerto;
    int disponible;  // 1 = disponible, 0 = no disponible
    int indice;      // índice en la tabla
} ServidorArchivos;

// Estructura para archivos y directorios
typedef struct {
    char nombre[MAX_FILENAME];
    int lock;                // 1 = bloqueado, 0 = libre
    int indice_servidor;     // índice del servidor que lo contiene
    int es_directorio;       // 1 = directorio, 0 = archivo
} ArchivoDirectorio;

// Estructura para datos del hilo
typedef struct {
    int socket_cliente;
} DatosHilo;

// Variables globales
ServidorArchivos tabla_servidores[MAX_SERVERS];
ArchivoDirectorio tabla_archivos[MAX_FILES];
int contador_servidores = 0;
int contador_archivos = 0;
pthread_mutex_t mutex_servidores = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_archivos = PTHREAD_MUTEX_INITIALIZER;

// Declaración de funciones
void mostrar_tabla_servidores();
void mostrar_tabla_archivos();
int registrar_servidor(const char* ip, int puerto);
int registrar_archivo_directorio(const char* nombre, int indice_servidor, int es_directorio);
void* manejar_cliente(void* arg);
int verificar_servidor_disponible(int indice_servidor);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s <ip> <puerto>\n", argv[0]);
        printf("Ejemplo: %s 127.0.0.1 9000\n", argv[0]);
        return 1;
    }
    
    const char* ip = argv[1];
    int puerto = atoi(argv[2]);
    
    // Crear socket del servidor
    int socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor < 0) {
        perror("Error creando socket");
        return 1;
    }
    
    // Permitir reutilizar la dirección
    int opt = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configurar dirección del servidor
    struct sockaddr_in direccion = {0};
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);
    inet_pton(AF_INET, ip, &direccion.sin_addr);
    
    // Bind del socket
    if (bind(socket_servidor, (struct sockaddr*)&direccion, sizeof(direccion)) < 0) {
        perror("Error en bind");
        close(socket_servidor);
        return 1;
    }
    
    // Escuchar conexiones
    if (listen(socket_servidor, 10) < 0) {
        perror("Error en listen");
        close(socket_servidor);
        return 1;
    }
    
    printf("[DNS] Servidor DNS ejecutándose en %s:%d\n", ip, puerto);
    printf("[DNS] Esperando conexiones...\n");
    
    // Bucle principal para aceptar clientes
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

// Función para mostrar la tabla de servidores
void mostrar_tabla_servidores() {
    pthread_mutex_lock(&mutex_servidores);
    printf("\n=== Tabla de Servidores ===\n");
    printf("%-8s %-16s %-8s %-12s\n", "Índice", "IP", "Puerto", "Disponible");
    printf("-----------------------------------------------\n");
    for (int i = 0; i < contador_servidores; i++) {
        printf("%-8d %-16s %-8d %-12s\n", 
               tabla_servidores[i].indice,
               tabla_servidores[i].ip, 
               tabla_servidores[i].puerto,
               tabla_servidores[i].disponible ? "SÍ" : "NO");
    }
    printf("-----------------------------------------------\n");
    pthread_mutex_unlock(&mutex_servidores);
}

// Función para mostrar la tabla de archivos y directorios
void mostrar_tabla_archivos() {
    pthread_mutex_lock(&mutex_archivos);
    printf("\n=== Tabla de Archivos y Directorios ===\n");
    printf("%-30s %-6s %-8s %-12s\n", "Nombre", "Lock", "Servidor", "Tipo");
    printf("--------------------------------------------------------\n");
    for (int i = 0; i < contador_archivos; i++) {
        printf("%-30s %-6s %-8d %-12s\n", 
               tabla_archivos[i].nombre,
               tabla_archivos[i].lock ? "SÍ" : "NO",
               tabla_archivos[i].indice_servidor,
               tabla_archivos[i].es_directorio ? "DIRECTORIO" : "ARCHIVO");
    }
    printf("--------------------------------------------------------\n");
    pthread_mutex_unlock(&mutex_archivos);
}

// Función para registrar un servidor de archivos
int registrar_servidor(const char* ip, int puerto) {
    pthread_mutex_lock(&mutex_servidores);
    
    // Verificar si el servidor ya existe
    for (int i = 0; i < contador_servidores; i++) {
        if (strcmp(tabla_servidores[i].ip, ip) == 0 && tabla_servidores[i].puerto == puerto) {
            // Servidor ya existe, marcarlo como disponible
            tabla_servidores[i].disponible = 1;
            printf("[DNS] Servidor re-registrado: %s:%d (índice %d)\n", ip, puerto, i);
            pthread_mutex_unlock(&mutex_servidores);
            return i;
        }
    }
    
    // Registrar nuevo servidor
    if (contador_servidores < MAX_SERVERS) {
        int indice = contador_servidores;
        strcpy(tabla_servidores[indice].ip, ip);
        tabla_servidores[indice].puerto = puerto;
        tabla_servidores[indice].disponible = 1;
        tabla_servidores[indice].indice = indice;
        contador_servidores++;
        
        printf("[DNS] Servidor registrado: %s:%d (índice %d)\n", ip, puerto, indice);
        pthread_mutex_unlock(&mutex_servidores);
        return indice;
    }
    
    printf("[DNS] Error: Tabla de servidores llena\n");
    pthread_mutex_unlock(&mutex_servidores);
    return -1;
}

// Función para registrar un archivo o directorio
int registrar_archivo_directorio(const char* nombre, int indice_servidor, int es_directorio) {
    pthread_mutex_lock(&mutex_archivos);
    
    // Verificar que el servidor existe y está disponible
    if (indice_servidor >= contador_servidores || !tabla_servidores[indice_servidor].disponible) {
        printf("[DNS] Error: Servidor con índice %d no disponible\n", indice_servidor);
        pthread_mutex_unlock(&mutex_archivos);
        return 0;
    }
    
    // Verificar si el archivo/directorio ya existe
    for (int i = 0; i < contador_archivos; i++) {
        if (strcmp(tabla_archivos[i].nombre, nombre) == 0) {
            printf("[DNS] Error: Archivo/directorio '%s' ya existe\n", nombre);
            pthread_mutex_unlock(&mutex_archivos);
            return 0;
        }
    }
    
    // Registrar nuevo archivo/directorio
    if (contador_archivos < MAX_FILES) {
        strcpy(tabla_archivos[contador_archivos].nombre, nombre);
        tabla_archivos[contador_archivos].lock = 0;
        tabla_archivos[contador_archivos].indice_servidor = indice_servidor;
        tabla_archivos[contador_archivos].es_directorio = es_directorio;
        contador_archivos++;
        
        printf("[DNS] %s registrado: '%s' en servidor %d\n", 
               es_directorio ? "Directorio" : "Archivo", nombre, indice_servidor);
        pthread_mutex_unlock(&mutex_archivos);
        return 1;
    }
    
    printf("[DNS] Error: Tabla de archivos llena\n");
    pthread_mutex_unlock(&mutex_archivos);
    return 0;
}

// Función para verificar si un servidor está disponible
int verificar_servidor_disponible(int indice_servidor) {
    // TODO: Implementar verificación real de conexión al servidor
    // Por ahora solo verificamos el estado en la tabla
    pthread_mutex_lock(&mutex_servidores);
    if (indice_servidor < contador_servidores) {
        int disponible = tabla_servidores[indice_servidor].disponible;
        pthread_mutex_unlock(&mutex_servidores);
        return disponible;
    }
    pthread_mutex_unlock(&mutex_servidores);
    return 0;
}

// Función para manejar clientes en hilos separados
void* manejar_cliente(void* arg) {
    DatosHilo* datos = (DatosHilo*)arg;
    int socket_cliente = datos->socket_cliente;
    char buffer[BUF_SIZE];
    
    // Recibir comando del cliente
    int n = recv(socket_cliente, buffer, BUF_SIZE - 1, 0);
    if (n <= 0) {
        close(socket_cliente);
        free(datos);
        return NULL;
    }
    
    buffer[n] = '\0';
    printf("[DNS] Comando recibido: %s\n", buffer);
    
    // Procesar comandos
    if (strncmp(buffer, "REGISTRAR_SERVIDOR", 18) == 0) {
        // Formato: REGISTRAR_SERVIDOR ip puerto
        char ip[MAX_IP];
        int puerto;
        
        if (sscanf(buffer, "REGISTRAR_SERVIDOR %s %d", ip, &puerto) == 2) {
            int indice = registrar_servidor(ip, puerto);
            if (indice >= 0) {
                char respuesta[BUF_SIZE];
                snprintf(respuesta, BUF_SIZE, "OK %d\n", indice);
                send(socket_cliente, respuesta, strlen(respuesta), 0);
                mostrar_tabla_servidores();
            } else {
                send(socket_cliente, "ERROR\n", 6, 0);
            }
        } else {
            send(socket_cliente, "FORMATO_INCORRECTO\n", 19, 0);
        }
        
    } else if (strncmp(buffer, "REGISTRAR_ARCHIVO_DIRECTORIO", 28) == 0) {
        // Formato: REGISTRAR_ARCHIVO_DIRECTORIO nombre
        char nombre[MAX_FILENAME];
        if (sscanf(buffer, "REGISTRAR_ARCHIVO_DIRECTORIO %s", nombre) == 1) {
            // Obtener la IP del cliente
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            if (getpeername(socket_cliente, (struct sockaddr*)&addr, &addr_len) == 0) {
                char ip_cliente[MAX_IP];
                inet_ntop(AF_INET, &addr.sin_addr, ip_cliente, MAX_IP);
                // Buscar el índice del servidor con esta IP
                int indice_servidor = -1;
                pthread_mutex_lock(&mutex_servidores);
                for (int i = 0; i < contador_servidores; i++) {
                    if (strcmp(tabla_servidores[i].ip, ip_cliente) == 0 && tabla_servidores[i].disponible) {
                        indice_servidor = i;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex_servidores);
                if (indice_servidor >= 0) {
                    // Por ahora, asumimos que es un archivo (no directorio)
                    int es_directorio = 0; // TODO: permitir especificar tipo en el futuro
                    if (registrar_archivo_directorio(nombre, indice_servidor, es_directorio)) {
                        send(socket_cliente, "OK\n", 3, 0);
                        mostrar_tabla_archivos();
                    } else {
                        send(socket_cliente, "ERROR\n", 6, 0);
                    }
                } else {
                    send(socket_cliente, "SERVIDOR_NO_REGISTRADO\n", 23, 0);
                }
            } else {
                send(socket_cliente, "ERROR_IP\n", 9, 0);
            }
        } else {
            send(socket_cliente, "FORMATO_INCORRECTO\n", 19, 0);
        }
        
    } else if (strncmp(buffer, "REGISTRAR_DIRECTORIO", 20) == 0) {
        // Formato: REGISTRAR_DIRECTORIO nombre
        char nombre[MAX_FILENAME];
        if (sscanf(buffer, "REGISTRAR_DIRECTORIO %s", nombre) == 1) {
            // Obtener la IP del cliente
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            if (getpeername(socket_cliente, (struct sockaddr*)&addr, &addr_len) == 0) {
                char ip_cliente[MAX_IP];
                inet_ntop(AF_INET, &addr.sin_addr, ip_cliente, MAX_IP);
                // Buscar el índice del servidor con esta IP
                int indice_servidor = -1;
                pthread_mutex_lock(&mutex_servidores);
                for (int i = 0; i < contador_servidores; i++) {
                    if (strcmp(tabla_servidores[i].ip, ip_cliente) == 0 && tabla_servidores[i].disponible) {
                        indice_servidor = i;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex_servidores);
                if (indice_servidor >= 0) {
                    int es_directorio = 1;
                    if (registrar_archivo_directorio(nombre, indice_servidor, es_directorio)) {
                        send(socket_cliente, "OK\n", 3, 0);
                        mostrar_tabla_archivos();
                    } else {
                        send(socket_cliente, "ERROR\n", 6, 0);
                    }
                } else {
                    send(socket_cliente, "SERVIDOR_NO_REGISTRADO\n", 23, 0);
                }
            } else {
                send(socket_cliente, "ERROR_IP\n", 9, 0);
            }
        } else {
            send(socket_cliente, "FORMATO_INCORRECTO\n", 19, 0);
        }
    } else {
        // Comando no reconocido
        send(socket_cliente, "COMANDO_NO_RECONOCIDO\n", 22, 0);
        printf("[DNS] Comando no reconocido: %s\n", buffer);
    }
    
    close(socket_cliente);
    free(datos);
    return NULL;
} 