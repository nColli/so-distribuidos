// cliente_nuevo.c - Cliente para el sistema de archivos distribuido
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common_nuevo.h"

// Variables para configuración
char ip_dns[MAX_IP];
int puerto_dns;

// Función para crear la carpeta de descargas si no existe
void crear_carpeta_descargas() {
    struct stat info;
    if (stat(FOLDER_DOWNLOADS, &info) != 0) {
        if (mkdir(FOLDER_DOWNLOADS, 0755) == 0) {
            printf("[CLIENTE] Carpeta de descargas creada: %s\n", FOLDER_DOWNLOADS);
        } else {
            perror("Error creando carpeta de descargas");
        }
    }
}

// Función para recibir un archivo
void recibir_archivo(int socket_servidor, const char* nombre_archivo) {
    char ruta_completa[MAX_FILENAME];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", FOLDER_DOWNLOADS, nombre_archivo);
    
    FILE* archivo = fopen(ruta_completa, "wb");
    if (!archivo) {
        printf("[CLIENTE] No se pudo crear el archivo: %s\n", ruta_completa);
        return;
    }
    
    char buffer[BUF_SIZE];
    int bytes_recibidos;
    while ((bytes_recibidos = recv(socket_servidor, buffer, BUF_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_recibidos, archivo);
    }
    
    fclose(archivo);
    printf("[CLIENTE] Archivo recibido: %s\n", ruta_completa);
}

// Función para recibir una carpeta completa
void recibir_carpeta(int socket_servidor, const char* nombre_carpeta) {
    char ruta_carpeta[MAX_FILENAME];
    snprintf(ruta_carpeta, sizeof(ruta_carpeta), "%s/%s", FOLDER_DOWNLOADS, nombre_carpeta);
    
    // Crear la carpeta de destino
    if (mkdir(ruta_carpeta, 0755) != 0) {
        perror("Error creando carpeta destino");
        return;
    }
    
    char buffer[BUF_SIZE];
    int n;
    
    while ((n = recv(socket_servidor, buffer, BUF_SIZE - 1, 0)) > 0) {
        buffer[n] = '\0';
        
        if (strncmp(buffer, "FIN_CARPETA", 11) == 0) {
            break;
        }
        
        if (strncmp(buffer, "ARCHIVO:", 8) == 0) {
            // Formato: ARCHIVO:nombre:tamaño
            char nombre_archivo[MAX_FILENAME];
            long tamaño_archivo;
            char* linea = strtok(buffer, "\n");
            sscanf(linea, "ARCHIVO:%[^:]:%ld", nombre_archivo, &tamaño_archivo);
            
            // Crear archivo en la carpeta
            char ruta_archivo[MAX_FILENAME];
            snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/%s", ruta_carpeta, nombre_archivo);
            
            FILE* archivo = fopen(ruta_archivo, "wb");
            if (archivo) {
                long bytes_restantes = tamaño_archivo;
                while (bytes_restantes > 0) {
                    int bytes_a_leer = (bytes_restantes > BUF_SIZE) ? BUF_SIZE : bytes_restantes;
                    int bytes_leidos = recv(socket_servidor, buffer, bytes_a_leer, 0);
                    if (bytes_leidos <= 0) break;
                    
                    fwrite(buffer, 1, bytes_leidos, archivo);
                    bytes_restantes -= bytes_leidos;
                }
                fclose(archivo);
                printf("[CLIENTE] Archivo en carpeta recibido: %s\n", ruta_archivo);
            }
        } else if (strncmp(buffer, "CARPETA:", 8) == 0) {
            // Formato: CARPETA:nombre
            char nombre_subcarpeta[MAX_FILENAME];
            sscanf(buffer, "CARPETA:%s", nombre_subcarpeta);
            
            char ruta_subcarpeta[MAX_FILENAME];
            snprintf(ruta_subcarpeta, sizeof(ruta_subcarpeta), "%s/%s", ruta_carpeta, nombre_subcarpeta);
            mkdir(ruta_subcarpeta, 0755);
            printf("[CLIENTE] Subcarpeta creada: %s\n", ruta_subcarpeta);
        }
    }
    
    printf("[CLIENTE] Carpeta completa recibida: %s\n", ruta_carpeta);
}

// Función para enviar un archivo
void enviar_archivo(int socket_servidor, const char* nombre_archivo) {
    char ruta_completa[MAX_FILENAME];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", FOLDER_DOWNLOADS, nombre_archivo);
    
    FILE* archivo = fopen(ruta_completa, "rb");
    if (!archivo) {
        printf("[CLIENTE] No se pudo abrir el archivo: %s\n", ruta_completa);
        return;
    }
    
    char buffer[BUF_SIZE];
    size_t bytes_leidos;
    while ((bytes_leidos = fread(buffer, 1, BUF_SIZE, archivo)) > 0) {
        send(socket_servidor, buffer, bytes_leidos, 0);
    }
    
    fclose(archivo);
    printf("[CLIENTE] Archivo enviado: %s\n", ruta_completa);
}

// Función principal para solicitar un elemento
void solicitar_elemento(const char* nombre, const char* comando) {
    // Conectar al DNS
    int socket_dns = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_dns < 0) {
        perror("Error creando socket DNS");
        return;
    }
    
    struct sockaddr_in dns_addr = {0};
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(puerto_dns);
    inet_pton(AF_INET, ip_dns, &dns_addr.sin_addr);

    if (connect(socket_dns, (struct sockaddr*)&dns_addr, sizeof(dns_addr)) < 0) {
        perror("Error conectando al DNS");
        close(socket_dns);
        return;
    }
    
    // Enviar solicitud al DNS
    char buffer[BUF_SIZE];
    snprintf(buffer, BUF_SIZE, "SOLICITAR_ELEMENTO %s %s", nombre, comando);
    send(socket_dns, buffer, strlen(buffer), 0);
    
    // Recibir respuesta del DNS
    int n = recv(socket_dns, buffer, BUF_SIZE - 1, 0);
    buffer[n] = '\0';
    close(socket_dns);
    
    if (strncmp(buffer, "OK", 2) == 0) {
        char ip_servidor[MAX_IP], tipo_str[16];
        int puerto_servidor;
        sscanf(buffer, "OK %s %d %s", ip_servidor, &puerto_servidor, tipo_str);
        
        printf("[CLIENTE] Elemento encontrado en servidor %s:%d (Tipo: %s)\n", 
               ip_servidor, puerto_servidor, tipo_str);
        
        // Conectar al servidor de archivos
        int socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in servidor_addr = {0};
        servidor_addr.sin_family = AF_INET;
        servidor_addr.sin_port = htons(puerto_servidor);
        inet_pton(AF_INET, ip_servidor, &servidor_addr.sin_addr);

        if (connect(socket_servidor, (struct sockaddr*)&servidor_addr, sizeof(servidor_addr)) < 0) {
            perror("Error conectando al servidor de archivos");
            close(socket_servidor);
            return;
        }
        
        if (strcmp(comando, "LEER") == 0) {
            // Solicitar elemento para lectura
            snprintf(buffer, BUF_SIZE, "OBTENER_ELEMENTO %s", nombre);
            send(socket_servidor, buffer, strlen(buffer), 0);
            
            crear_carpeta_descargas();
            
            if (strcmp(tipo_str, "ARCHIVO") == 0) {
                recibir_archivo(socket_servidor, nombre);
            } else if (strcmp(tipo_str, "CARPETA") == 0) {
                recibir_carpeta(socket_servidor, nombre);
            }
            
        } else if (strcmp(comando, "ESCRIBIR") == 0) {
            if (strcmp(tipo_str, "ARCHIVO") == 0) {
                // Primero obtener el archivo
                snprintf(buffer, BUF_SIZE, "OBTENER_ELEMENTO %s", nombre);
                send(socket_servidor, buffer, strlen(buffer), 0);
                
                crear_carpeta_descargas();
                recibir_archivo(socket_servidor, nombre);
                close(socket_servidor);
                
                printf("[CLIENTE] Archivo descargado para edición: %s/%s\n", FOLDER_DOWNLOADS, nombre);
                printf("[CLIENTE] Edite el archivo y presione Enter para enviarlo de vuelta.\n");
                getchar();
                
                // Conectar nuevamente para enviar el archivo modificado
                socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
                connect(socket_servidor, (struct sockaddr*)&servidor_addr, sizeof(servidor_addr));
                
                snprintf(buffer, BUF_SIZE, "ACTUALIZAR_ARCHIVO %s", nombre);
                send(socket_servidor, buffer, strlen(buffer), 0);
                
                enviar_archivo(socket_servidor, nombre);
                printf("[CLIENTE] Archivo enviado de vuelta al servidor.\n");
            } else {
                printf("[CLIENTE] No se puede escribir en carpetas, solo en archivos.\n");
            }
        }
        
        close(socket_servidor);
        
    } else if (strncmp(buffer, "BLOQUEADO", 9) == 0) {
        printf("[CLIENTE] El elemento está bloqueado (en uso por otro cliente).\n");
    } else if (strncmp(buffer, "NO_ENCONTRADO", 13) == 0) {
        printf("[CLIENTE] El elemento '%s' no fue encontrado en el sistema.\n", nombre);
    } else {
        printf("[CLIENTE] Respuesta desconocida del DNS: %s\n", buffer);
    }
}

// Función para mostrar ayuda
void mostrar_ayuda(const char* programa) {
    printf("Uso: %s <ip_dns> <puerto_dns> <nombre_elemento> <LEER|ESCRIBIR>\n", programa);
    printf("\nEjemplos:\n");
    printf("  %s 127.0.0.1 9000 archivo.txt LEER\n", programa);
    printf("  %s 127.0.0.1 9000 archivo.txt ESCRIBIR\n", programa);
    printf("  %s 127.0.0.1 9000 mi_carpeta LEER\n", programa);
    printf("\nComandos:\n");
    printf("  LEER     - Descargar archivo o carpeta para solo lectura\n");
    printf("  ESCRIBIR - Descargar archivo para edición (solo archivos)\n");
    printf("\nNota: Los archivos y carpetas se guardan en la carpeta '%s'\n", FOLDER_DOWNLOADS);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        mostrar_ayuda(argv[0]);
        return 1;
    }
    
    strcpy(ip_dns, argv[1]);
    puerto_dns = atoi(argv[2]);
    const char* nombre_elemento = argv[3];
    const char* comando = argv[4];
    
    // Validar comando
    if (strcmp(comando, "LEER") != 0 && strcmp(comando, "ESCRIBIR") != 0) {
        printf("[CLIENTE] Comando inválido: %s. Use LEER o ESCRIBIR.\n", comando);
        return 1;
    }
    
    printf("[CLIENTE] Conectando a DNS %s:%d\n", ip_dns, puerto_dns);
    printf("[CLIENTE] Solicitando '%s' con comando '%s'\n", nombre_elemento, comando);
    
    solicitar_elemento(nombre_elemento, comando);
    
    return 0;
} 