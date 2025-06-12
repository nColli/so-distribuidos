// file_server.c - Servidor de archivos compatible con dns.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_FILES 1000
#define MAX_FILENAME 256
#define MAX_IP 16
#define BUF_SIZE 1024

char elementos_registrados[MAX_FILES][MAX_FILENAME];
int contador_registrados = 0;
pthread_mutex_t mutex_registrados = PTHREAD_MUTEX_INITIALIZER;
char ip_dns[MAX_IP];
int puerto_dns;
char ip_servidor[MAX_IP];
int puerto_servidor;
int indice_servidor_dns = -1;

// Registrar el servidor en el DNS
int registrar_servidor_dns(const char* ip, int puerto) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in dns_addr = {0};
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(puerto_dns);
    inet_pton(AF_INET, ip_dns, &dns_addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&dns_addr, sizeof(dns_addr)) < 0) {
        close(sock);
        return -1;
    }
    char buffer[BUF_SIZE];
    snprintf(buffer, BUF_SIZE, "REGISTRAR_SERVIDOR %s %d", ip, puerto);
    send(sock, buffer, strlen(buffer), 0);
    int n = recv(sock, buffer, BUF_SIZE, 0);
    buffer[n] = '\0';
    close(sock);
    int indice = -1;
    if (sscanf(buffer, "OK %d", &indice) == 1) {
        printf("[SERVIDOR] Registrado en DNS con índice %d\n", indice);
        return indice;
    } else {
        printf("[SERVIDOR] Error registrando servidor en DNS: %s\n", buffer);
        return -1;
    }
}

// Registrar un archivo en el DNS
int registrar_archivo_dns(const char* nombre) {
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
    snprintf(buffer, BUF_SIZE, "REGISTRAR_ARCHIVO_DIRECTORIO %s", nombre);
    send(sock, buffer, strlen(buffer), 0);
    int n = recv(sock, buffer, BUF_SIZE, 0);
    buffer[n] = '\0';
    close(sock);
    if (strncmp(buffer, "OK", 2) == 0) {
        pthread_mutex_lock(&mutex_registrados);
        strcpy(elementos_registrados[contador_registrados], nombre);
        contador_registrados++;
        pthread_mutex_unlock(&mutex_registrados);
        printf("[SERVIDOR] Archivo registrado: %s\n", nombre);
        return 1;
    } else {
        printf("[SERVIDOR] Error registrando archivo '%s' en DNS: %s\n", nombre, buffer);
        return 0;
    }
}

// Registrar una carpeta en el DNS (solo el nombre)
int registrar_directorio_dns(const char* nombre) {
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
    snprintf(buffer, BUF_SIZE, "REGISTRAR_DIRECTORIO %s", nombre);
    send(sock, buffer, strlen(buffer), 0);
    int n = recv(sock, buffer, BUF_SIZE, 0);
    buffer[n] = '\0';
    close(sock);
    if (strncmp(buffer, "OK", 2) == 0) {
        printf("[SERVIDOR] Carpeta registrada: %s\n", nombre);
        return 1;
    } else {
        printf("[SERVIDOR] Error registrando carpeta '%s' en DNS: %s\n", nombre, buffer);
        return 0;
    }
}

// Registrar todos los archivos en el directorio actual
void registrar_todos_los_archivos() {
    DIR* dir = opendir(".");
    if (!dir) return;
    struct dirent* entrada;
    while ((entrada = readdir(dir)) != NULL) {
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) continue;
        struct stat info;
        if (stat(entrada->d_name, &info) == 0 && S_ISREG(info.st_mode)) {
            registrar_archivo_dns(entrada->d_name);
        }
    }
    closedir(dir);
}

void mostrar_menu() {
    printf("\n=== SERVIDOR DE ARCHIVOS ===\n");
    printf("1. habilitar - Registrar servidor en DNS\n");
    printf("2. subir_archivo <nombre> - Registrar archivo específico\n");
    printf("3. subir_carpeta <nombre> - Registrar carpeta específica\n");
    printf("4. actualizar_todo - Registrar todos los archivos\n");
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
    printf("[SERVIDOR] Configurado - Servidor: %s:%d, DNS: %s:%d\n", ip_servidor, puerto_servidor, ip_dns, puerto_dns);
    char comando[64];
    char parametro[MAX_FILENAME];
    while (1) {
        mostrar_menu();
        if (!fgets(comando, sizeof(comando), stdin)) {
            break;
        }
        comando[strcspn(comando, "\n")] = 0;
        if (strcmp(comando, "habilitar") == 0) {
            printf("[SERVIDOR] Habilitando servidor...\n");
            indice_servidor_dns = registrar_servidor_dns(ip_servidor, puerto_servidor);
            if (indice_servidor_dns >= 0) {
                printf("[SERVIDOR] Servidor habilitado en DNS.\n");
            } else {
                printf("[SERVIDOR] No se pudo habilitar el servidor en DNS.\n");
            }
        } else if (sscanf(comando, "subir_archivo %s", parametro) == 1) {
            struct stat info;
            if (stat(parametro, &info) == 0 && S_ISREG(info.st_mode)) {
                registrar_archivo_dns(parametro);
            } else {
                printf("[SERVIDOR] Archivo no encontrado: %s\n", parametro);
            }
        } else if (sscanf(comando, "subir_carpeta %s", parametro) == 1) {
            struct stat info;
            if (stat(parametro, &info) == 0 && S_ISDIR(info.st_mode)) {
                registrar_directorio_dns(parametro);
            } else {
                printf("[SERVIDOR] Carpeta no encontrada: %s\n", parametro);
            }
        } else if (strcmp(comando, "actualizar_todo") == 0) {
            printf("[SERVIDOR] Registrando todos los archivos...\n");
            registrar_todos_los_archivos();
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
