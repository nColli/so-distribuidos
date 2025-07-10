#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"

void readMessage(char *buffer) {
    printf("Mensaje: ");
    if (fgets(buffer, MAX_MSG, stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
    } else {
        buffer[0] = '\0';  // Clear buffer on error
    }
}

int readSendMessage(int fd_socket, char* buffer) {
    readMessage(buffer);

    return sendMessage(fd_socket, buffer);
}

int sendMessage(int fd_socket, char* mensaje) {
    long int nb = write(fd_socket, mensaje, strlen(mensaje));

    printf("%ld bytes enviados\n", nb);

    return nb;
}

int recvMessage(int fd_socket, char* mensaje) {
    long int nb = read(fd_socket, mensaje, MAX_MSG);
    mensaje[nb] = '\0';

    printf("%ld bytes recibidos de socket %d\n", nb, fd_socket);

    return nb;
}
