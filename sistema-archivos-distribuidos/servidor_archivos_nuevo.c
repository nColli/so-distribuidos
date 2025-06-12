// servidor_archivos_nuevo.c - Servidor de archivos con interfaz de comandos y daemon
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include "common_nuevo.h"

// Variables globales
char elementos_registrados[MAX_FILES][MAX_FILENAME];
TipoElemento tipos_registrados[MAX_FILES];
int contador_registrados = 0;
pthread_mutex_t mutex_registrados = PTHREAD_MUTEX_INITIALIZER;
char ip_dns[MAX_IP];
int puerto_dns;
char ip_servidor[MAX_IP];
int puerto_servidor;

// Función para registrar un elemento en el DNS
int registrar_elemento_dns(const char* nombre, const char* ip, int puerto, TipoElemento tipo) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    
    struct sockaddr_in dns_addr = {0};
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(puerto_dns);
    inet_pton(AF_INET, ip_dns, &dns_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&dns_addr, sizeof(dns_addr)) < 0) {
        close(sock);
        return 0;
    }
    
    char buffer[BUF_SIZE];
    snprintf(buffer, BUF_SIZE, "REGISTRAR_ELEMENTO %s %s %d %s", 
             nombre, ip, puerto, (tipo == TIPO_CARPETA) ? "CARPETA" : "ARCHIVO");
    send(sock, buffer, strlen(buffer), 0);
    
    int n = recv(sock, buffer, BUF_SIZE, 0);
    buffer[n] = '\0';
    close(sock);
    
    if (strncmp(buffer, "OK", 2) == 0) {
        pthread_mutex_lock(&mutex_registrados);
        strcpy(elementos_registrados[contador_registrados], nombre);
        tipos_registrados[contador_registrados] = tipo;
        contador_registrados++;
        pthread_mutex_unlock(&mutex_registrados);
        printf("[SERVIDOR] Elemento registrado: %s (%s)\n", 
               nombre, (tipo == TIPO_ARCHIVO) ? "archivo" : "carpeta");
        return 1;
    } else if (strncmp(buffer, "DUPLICADO", 9) == 0) {
        printf("[SERVIDOR] Elemento %s ya está registrado en otro lugar. Omitiendo.\n", nombre);
        return 0;
    }
    return 0;
}

// Función para desbloquear elemento en DNS
void desbloquear_elemento_dns(const char* nombre) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dns_addr = {0};
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(puerto_dns);
    inet_pton(AF_INET, ip_dns, &dns_addr.sin_addr);
    connect(sock, (struct sockaddr*)&dns_addr, sizeof(dns_addr));
    
    char buffer[BUF_SIZE];
    snprintf(buffer, BUF_SIZE, "DESBLOQUEAR_ELEMENTO %s", nombre);
    send(sock, buffer, strlen(buffer), 0);
    
    int n = recv(sock, buffer, BUF_SIZE, 0);
    buffer[n] = '\0';
    printf("[SERVIDOR] Desbloqueo de %s en DNS, respuesta: %s\n", nombre, buffer);
    close(sock);
}

// Función para verificar si un elemento está registrado
int esta_registrado(const char* nombre) {
    pthread_mutex_lock(&mutex_registrados);
    for (int i = 0; i < contador_registrados; i++) {
        if (strcmp(elementos_registrados[i], nombre) == 0) {
            pthread_mutex_unlock(&mutex_registrados);
            return 1;
        }
    }
    pthread_mutex_unlock(&mutex_registrados);
    return 0;
}

// Función para enviar un archivo
void enviar_archivo(int socket_cliente, const char* nombre_archivo) {
    FILE* archivo = fopen(nombre_archivo, "rb");
    if (!archivo) {
        printf("[SERVIDOR] No se pudo abrir el archivo: %s\n", nombre_archivo);
        return;
    }
    
    char buffer[BUF_SIZE];
    size_t bytes_leidos;
    while ((bytes_leidos = fread(buffer, 1, BUF_SIZE, archivo)) > 0) {
        send(socket_cliente, buffer, bytes_leidos, 0);
    }
    
    fclose(archivo);
    printf("[SERVIDOR] Archivo enviado: %s\n", nombre_archivo);
}

// Función para enviar una carpeta completa
void enviar_carpeta(int socket_cliente, const char* nombre_carpeta) {
    DIR* dir = opendir(nombre_carpeta);
    if (!dir) {
        printf("[SERVIDOR] No se pudo abrir la carpeta: %s\n", nombre_carpeta);
        return;
    }
    
    struct dirent* entrada;
    char ruta_completa[MAX_FILENAME];
    char buffer[BUF_SIZE];
    
    // Enviar información de la carpeta
    while ((entrada = readdir(dir)) != NULL) {
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", nombre_carpeta, entrada->d_name);
        
        struct stat info_archivo;
        if (stat(ruta_completa, &info_archivo) == 0) {
            if (S_ISREG(info_archivo.st_mode)) {
                // Es un archivo
                snprintf(buffer, BUF_SIZE, "ARCHIVO:%s:%ld\n", entrada->d_name, info_archivo.st_size);
                send(socket_cliente, buffer, strlen(buffer), 0);
                
                // Enviar contenido del archivo
                FILE* archivo = fopen(ruta_completa, "rb");
                if (archivo) {
                    size_t bytes_leidos;
                    while ((bytes_leidos = fread(buffer, 1, BUF_SIZE, archivo)) > 0) {
                        send(socket_cliente, buffer, bytes_leidos, 0);
                    }
                    fclose(archivo);
                }
            } else if (S_ISDIR(info_archivo.st_mode)) {
                // Es una subcarpeta
                snprintf(buffer, BUF_SIZE, "CARPETA:%s\n", entrada->d_name);
                send(socket_cliente, buffer, strlen(buffer), 0);
            }
        }
    }
    
    // Enviar fin de carpeta
    send(socket_cliente, "FIN_CARPETA\n", 12, 0);
    closedir(dir);
    printf("[SERVIDOR] Carpeta enviada: %s\n", nombre_carpeta);
}

// Función para recibir un archivo
void recibir_archivo(int socket_cliente, const char* nombre_archivo) {
    FILE* archivo = fopen(nombre_archivo, "wb");
    if (!archivo) {
        printf("[SERVIDOR] No se pudo crear el archivo: %s\n", nombre_archivo);
        return;
    }
    
    char buffer[BUF_SIZE];
    int bytes_recibidos;
    while ((bytes_recibidos = recv(socket_cliente, buffer, BUF_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_recibidos, archivo);
    }
    
    fclose(archivo);
    printf("[SERVIDOR] Archivo recibido y actualizado: %s\n", nombre_archivo);
}

// Función para manejar clientes del daemon
void* manejar_cliente_daemon(void* arg) {
    DatosHilo* datos = (DatosHilo*)arg;
    int socket_cliente = datos->socket_cliente;
    char buffer[BUF_SIZE];
    
    int n = recv(socket_cliente, buffer, BUF_SIZE, 0);
    if (n <= 0) {
        close(socket_cliente);
        free(datos);
        return NULL;
    }
    
    buffer[n] = '\0';
    
    char comando[16], nombre[MAX_FILENAME];
    nombre[0] = '\0';
    sscanf(buffer, "%s %s", comando, nombre);
    printf("[DAEMON] Comando recibido: '%s' para elemento: '%s'\n", comando, nombre);
    
    if (strcmp(comando, "OBTENER_ELEMENTO") == 0 && strlen(nombre) > 0) {
        if (esta_registrado(nombre)) {
            struct stat info;
            if (stat(nombre, &info) == 0) {
                if (S_ISREG(info.st_mode)) {
                    // Es un archivo
                    printf("[DAEMON] Sirviendo archivo: %s\n", nombre);
                    enviar_archivo(socket_cliente, nombre);
                } else if (S_ISDIR(info.st_mode)) {
                    // Es una carpeta
                    printf("[DAEMON] Sirviendo carpeta: %s\n", nombre);
                    enviar_carpeta(socket_cliente, nombre);
                }
            } else {
                printf("[DAEMON] Elemento no encontrado: %s\n", nombre);
            }
        } else {
            printf("[DAEMON] Elemento no registrado: %s\n", nombre);
        }
    } else if (strcmp(comando, "ACTUALIZAR_ARCHIVO") == 0 && strlen(nombre) > 0) {
        if (esta_registrado(nombre)) {
            struct stat info;
            if (stat(nombre, &info) == 0 && S_ISREG(info.st_mode)) {
                printf("[DAEMON] Recibiendo actualización de archivo: %s\n", nombre);
                recibir_archivo(socket_cliente, nombre);
                desbloquear_elemento_dns(nombre);
            }
        } else {
            printf("[DAEMON] Archivo no registrado para actualización: %s\n", nombre);
        }
    } else {
        printf("[DAEMON] Comando desconocido o mal formado: '%s'\n", buffer);
    }
    
    close(socket_cliente);
    free(datos);
    return NULL;
}

// Función del daemon (proceso en segundo plano)
void ejecutar_daemon() {
    int socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor < 0) {
        perror("Error creando socket del daemon");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in direccion = {0};
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto_servidor);
    inet_pton(AF_INET, ip_servidor, &direccion.sin_addr);
    
    if (bind(socket_servidor, (struct sockaddr*)&direccion, sizeof(direccion)) < 0) {
        perror("Error en bind del daemon");
        exit(1);
    }
    
    listen(socket_servidor, 10);
    printf("[DAEMON] Servidor de archivos ejecutándose en %s:%d\n", ip_servidor, puerto_servidor);
    
    while (1) {
        struct sockaddr_in direccion_cliente;
        socklen_t tam_direccion = sizeof(direccion_cliente);
        int socket_cliente = accept(socket_servidor, (struct sockaddr*)&direccion_cliente, &tam_direccion);
        
        if (socket_cliente < 0) {
            perror("Error en accept del daemon");
            continue;
        }
        
        DatosHilo* datos = malloc(sizeof(DatosHilo));
        datos->socket_cliente = socket_cliente;
        
        pthread_t hilo;
        pthread_create(&hilo, NULL, manejar_cliente_daemon, datos);
        pthread_detach(hilo);
    }
    
    close(socket_servidor);
}

// Función para registrar todos los archivos .txt
void registrar_todos_archivos_txt() {
    DIR* dir = opendir(".");
    if (!dir) return;
    
    struct dirent* entrada;
    while ((entrada = readdir(dir)) != NULL) {
        if (strstr(entrada->d_name, ".txt") && strlen(entrada->d_name) > 4) {
            registrar_elemento_dns(entrada->d_name, ip_servidor, puerto_servidor, TIPO_ARCHIVO);
        }
    }
    closedir(dir);
}

// Función para registrar todas las carpetas
void registrar_todas_carpetas() {
    DIR* dir = opendir(".");
    if (!dir) return;
    
    struct dirent* entrada;
    struct stat info;
    while ((entrada = readdir(dir)) != NULL) {
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) {
            continue;
        }
        
        if (stat(entrada->d_name, &info) == 0 && S_ISDIR(info.st_mode)) {
            // Omitir carpetas especiales
            if (strcmp(entrada->d_name, FOLDER_DOWNLOADS) != 0) {
                registrar_elemento_dns(entrada->d_name, ip_servidor, puerto_servidor, TIPO_CARPETA);
            }
        }
    }
    closedir(dir);
}

// Función para mostrar el menú de comandos
void mostrar_menu() {
    printf("\n=== SERVIDOR DE ARCHIVOS DISTRIBUIDO ===\n");
    printf("1. habilitar - Registrar servidor en DNS e iniciar daemon\n");
    printf("2. subir_archivo <nombre> - Subir archivo específico\n");
    printf("3. subir_carpeta <nombre> - Subir carpeta específica\n");
    printf("4. actualizar_todo - Registrar todos los archivos y carpetas\n");
    printf("5. salir - Salir del programa\n");
    printf("==========================================\n");
    printf("Comando: ");
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printf("Uso: %s <ip_servidor> <puerto_servidor> <ip_dns> <puerto_dns>\n", argv[0]);
        printf("Ejemplo: %s 127.0.0.1 9001 127.0.0.1 9000\n", argv[0]);
        return 1;
    }
    
    strcpy(ip_servidor, argv[1]);
    puerto_servidor = atoi(argv[2]);
    strcpy(ip_dns, argv[3]);
    puerto_dns = atoi(argv[4]);
    
    printf("[SERVIDOR] Configurado - Servidor: %s:%d, DNS: %s:%d\n", 
           ip_servidor, puerto_servidor, ip_dns, puerto_dns);
    
    char comando[64];
    char parametro[MAX_FILENAME];
    
    while (1) {
        mostrar_menu();
        
        if (!fgets(comando, sizeof(comando), stdin)) {
            break;
        }
        
        // Remover salto de línea
        comando[strcspn(comando, "\n")] = 0;
        
        if (strcmp(comando, "habilitar") == 0) {
            printf("[SERVIDOR] Habilitando servidor...\n");
            registrar_todos_archivos_txt();
            registrar_todas_carpetas();
            
            // Crear proceso daemon
            pid_t pid = fork();
            if (pid == 0) {
                // Proceso hijo - daemon
                ejecutar_daemon();
            } else if (pid > 0) {
                // Proceso padre
                printf("[SERVIDOR] Servidor habilitado y daemon iniciado (PID: %d)\n", pid);
                printf("[SERVIDOR] El servidor ahora está registrado en el DNS.\n");
                printf("[SERVIDOR] Presione Enter para continuar con comandos o 'salir' para terminar.\n");
            } else {
                perror("Error creando proceso daemon");
            }
            
        } else if (sscanf(comando, "subir_archivo %s", parametro) == 1) {
            struct stat info;
            if (stat(parametro, &info) == 0 && S_ISREG(info.st_mode)) {
                if (registrar_elemento_dns(parametro, ip_servidor, puerto_servidor, TIPO_ARCHIVO)) {
                    printf("[SERVIDOR] Archivo subido exitosamente: %s\n", parametro);
                } else {
                    printf("[SERVIDOR] Error subiendo archivo: %s\n", parametro);
                }
            } else {
                printf("[SERVIDOR] Archivo no encontrado: %s\n", parametro);
            }
            
        } else if (sscanf(comando, "subir_carpeta %s", parametro) == 1) {
            struct stat info;
            if (stat(parametro, &info) == 0 && S_ISDIR(info.st_mode)) {
                if (registrar_elemento_dns(parametro, ip_servidor, puerto_servidor, TIPO_CARPETA)) {
                    printf("[SERVIDOR] Carpeta subida exitosamente: %s\n", parametro);
                } else {
                    printf("[SERVIDOR] Error subiendo carpeta: %s\n", parametro);
                }
            } else {
                printf("[SERVIDOR] Carpeta no encontrada: %s\n", parametro);
            }
            
        } else if (strcmp(comando, "actualizar_todo") == 0) {
            printf("[SERVIDOR] Actualizando todos los elementos...\n");
            registrar_todos_archivos_txt();
            registrar_todas_carpetas();
            printf("[SERVIDOR] Actualización completa.\n");
            
        } else if (strcmp(comando, "salir") == 0) {
            printf("[SERVIDOR] Saliendo del programa...\n");
            break;
            
        } else if (strlen(comando) > 0) {
            printf("[SERVIDOR] Comando no reconocido: %s\n", comando);
        }
    }
    
    return 0;
} 