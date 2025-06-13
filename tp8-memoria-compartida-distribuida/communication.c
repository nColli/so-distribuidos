#include "communication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int send_command(int socket, int command, const char* content) {
    if (command < 0 || command > 2) {
        fprintf(stderr, "Error: command debe ser entre 0 y 2\n");
        return -1;
    }

    if (content == NULL) {
        fprintf(stderr, "Error: content no puede ser NULL\n");
        return -1;
    }
    
    // crear mensaje
    char message[MAX_MSG];
    int bytes_written = snprintf(message, sizeof(message), "%d %s", command, content);
    
    if (bytes_written >= sizeof(message)) {
        fprintf(stderr, "Error: mensaje muy largo\n");
        return -1;
    }
    
    // Send message
    int bytes_sent = send(socket, message, strlen(message), 0);
    if (bytes_sent < 0) {
        perror("Error enviando command");
        return -1;
    }
    
    printf("Comando enviado: %d con contenido: %s (%d bytes)\n", command, content, bytes_sent);
    return 0;
}

char* recv_command(int socket) {
    char* buffer = malloc(MAX_MSG);
    if (buffer == NULL) {
        perror("Error alojando memoria");
        return NULL;
    }
    
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags & ~O_NONBLOCK);
    
    int bytes_received = recv(socket, buffer, MAX_MSG - 1, 0);
    
    fcntl(socket, F_SETFL, flags);
    
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("Conexion cerrada\n");
        } else {
            perror("Error receiving command");
        }
        free(buffer);
        return NULL;
    }
    buffer[bytes_received] = '\0';
    
    printf("Comando recibido: %s (%d bytes)\n", buffer, bytes_received);
    return buffer;
} 