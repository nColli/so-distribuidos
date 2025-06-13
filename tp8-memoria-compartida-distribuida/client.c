#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "communication.h"

#define MAX_PAGE_SIZE 4096

void handle_read_operation(int client_socket, int page_num);
void handle_write_operation(int client_socket, int page_num);
//void handle_read_lock_operation(int client_socket, int page_num);
int parse_server_response(const char* response, int* command, char** content);
int create_connection(const char* server_ip, int port);

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <servidor_ip> <puerto> <R|W> <numero_pagina>\n", argv[0]);
        printf("  R = Read (leer página)\n");
        printf("  W = Write (escribir página)\n");
        exit(1);
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char operation = argv[3][0];
    int page_num = atoi(argv[4]);

    // Validar operación
    if (operation != 'R' && operation != 'W' && operation != 'r' && operation != 'w') {
        printf("Error: Operación debe ser R (read) o W (write)\n");
        exit(1);
    }

    // Crear conexión al servidor
    int client_socket = create_connection(server_ip, port);
    if (client_socket < 0) {
        exit(1);
    }

    // Procesar operación solicitada
    if (operation == 'R' || operation == 'r') {
        // Operación de lectura simple - comando "0 nro_pag"
        handle_read_operation(client_socket, page_num);
    } else {
        // Operación de escritura - requiere primero read lock y luego write
        handle_write_operation(client_socket, page_num);
    }
    
    close(client_socket);
    return 0;
} 

void handle_read_operation(int client_socket, int page_num) {
    printf("\n--- Lectura de página %d ---\n", page_num);
    
    // Enviar comando "0 nro_pag" para lectura simple
    char page_str[16];
    snprintf(page_str, sizeof(page_str), "%d", page_num);
    
    if (send_command(client_socket, 0, page_str) != 0) {
        printf("Error al enviar comando de lectura\n");
        return;
    }
    
    // Recibir respuesta del servidor
    char* response = recv_command(client_socket);
    if (response == NULL) {
        printf("Error al recibir respuesta del servidor\n");
        return;
    }
    
    printf("Contenido de la página %d:\n", page_num);
    printf("%s\n", response);
    
    free(response);
    printf("--- Fin de lectura ---\n");
}

int parse_server_response(const char* response, int* command, char** content) {
    if (!response || !command || !content) return -1;
    
    *content = NULL;
    
    // Buscar el primer espacio
    char* space = strchr(response, ' ');
    if (space == NULL) {
        // Solo hay comando
        *command = atoi(response);
        return 0;
    }
    
    // Extraer comando
    *command = atoi(response);
    
    // Extraer contenido (después del espacio)
    char* content_start = space + 1;
    if (strlen(content_start) > 0) {
        *content = malloc(strlen(content_start) + 1);
        if (*content) {
            strcpy(*content, content_start);
        }
    }
    
    return 0;
}

void handle_write_operation(int client_socket, int page_num) {
    printf("\n--- Operación de escritura en página %d ---\n", page_num);
    
    // PASO 1: Hacer read lock (comando "1 nro_pag")
    printf("Paso 1: Solicitando read lock para página %d\n", page_num);
    
    char page_str[16];
    snprintf(page_str, sizeof(page_str), "%d", page_num);
    
    if (send_command(client_socket, 1, page_str) != 0) {
        printf("Error al enviar comando de read lock\n");
        return;
    }
    
    // Recibir respuesta del read lock
    char* response = recv_command(client_socket);
    if (response == NULL) {
        printf("Error al recibir respuesta del servidor\n");
        return;
    }
    
    int server_command;
    char* current_content = NULL;
    
    if (parse_server_response(response, &server_command, &current_content) != 0) {
        printf("Error al parsear respuesta del servidor\n");
        free(response);
        return;
    }
    
    if (server_command == 1) {
        // Página bloqueada - esperar notificación "0 contenido"
        printf("Página %d está bloqueada, esperando notificación...\n", page_num);
        free(response);
        
        char* wait_response;
        do {
            wait_response = recv_command(client_socket);
            if (wait_response == NULL) {
                printf("Error al recibir notificación del servidor o conexión cerrada\n");
                if (current_content) free(current_content);
                return;
            }
            
            // Verificar si es la respuesta esperada
            if (parse_server_response(wait_response, &server_command, &current_content) != 0) {
                printf("Error al parsear notificación del servidor\n");
                free(wait_response);
                if (current_content) free(current_content);
                return;
            }
            
            if (server_command == 0) {
                printf("Página %d ahora disponible para escritura\n", page_num);
                free(wait_response);
                break;
            } else {
                printf("Respuesta inesperada: comando %d, esperando comando 0...\n", server_command);
                free(wait_response);
                if (current_content) {
                    free(current_content);
                    current_content = NULL;
                }
            }
        } while (1);
        
    } else if (server_command == 0) {
        // Página disponible inmediatamente
        printf("Página %d disponible inmediatamente para escritura\n", page_num);
        free(response);
        
    } else {
        printf("Respuesta inesperada del servidor: comando %d\n", server_command);
        free(response);
        if (current_content) free(current_content);
        return;
    }
    
    // Mostrar contenido actual
    printf("\nContenido actual de la página %d:\n", page_num);
    printf("%s\n", current_content ? current_content : "(página vacía)");
    
    // PASO 2: Solicitar nuevo contenido al usuario
    printf("\nIngrese el nuevo contenido para la página (máximo %d caracteres):\n", MAX_PAGE_SIZE - 1);
    printf("> ");
    fflush(stdout);
    
    char new_content[MAX_PAGE_SIZE];
    if (fgets(new_content, sizeof(new_content), stdin) != NULL) {
        // Remover salto de línea si existe
        size_t len = strlen(new_content);
        if (len > 0 && new_content[len-1] == '\n') {
            new_content[len-1] = '\0';
        }
        
        // PASO 3: Enviar comando "2 nro_pag contenido" para escribir
        printf("Paso 2: Enviando nuevo contenido a página %d\n", page_num);
        
        // Crear mensaje "nro_pag contenido"
        char write_msg[MAX_PAGE_SIZE + 32];
        snprintf(write_msg, sizeof(write_msg), "%d %s", page_num, new_content);
        
        if (send_command(client_socket, 2, write_msg) != 0) {
            printf("Error al enviar comando de escritura\n");
            if (current_content) free(current_content);
            return;
        }
        
        // Recibir confirmación del servidor (comando "2")
        char* write_response = recv_command(client_socket);
        if (write_response == NULL) {
            printf("Error al recibir confirmación del servidor\n");
            if (current_content) free(current_content);
            return;
        }
        
        int confirm_command;
        char* confirm_content = NULL;
        
        if (parse_server_response(write_response, &confirm_command, &confirm_content) != 0) {
            printf("Error al parsear confirmación del servidor\n");
        } else if (confirm_command == 2) {
            printf("✓ Página %d actualizada exitosamente\n", page_num);
        } else {
            printf("Error: se esperaba confirmación (comando 2), recibido %d\n", confirm_command);
        }
        
        free(write_response);
        if (confirm_content) free(confirm_content);
        
    } else {
        printf("Error al leer el nuevo contenido\n");
    }
    
    if (current_content) free(current_content);
    printf("--- Fin de operación de escritura ---\n");
}

int create_connection(const char* server_ip, int port) {
    // Crear socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error al crear socket");
        return -1;
    }

    // Configurar dirección del servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Dirección IP inválida");
        close(client_socket);
        return -1;
    }

    // Conectar al servidor
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectar con el servidor");
        close(client_socket);
        return -1;
    }

    printf("Conectado al servidor %s:%d\n", server_ip, port);

    return client_socket;
}