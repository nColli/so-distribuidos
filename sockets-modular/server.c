#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"

#define MAX_LISTEN 5

typedef struct server_state {
    int fd_socket;
    int server_port;
    struct sockaddr_in server_addr;
} server_state;

server_state server = {0}; //global para fn e hilos

void start_server();
void handle_connections();

int main (int argc, char *argv[]) {
    if (argc < 2) {
        printf("Use: %s <port>\n", argv[0]);
        exit(1);
    }

    server.server_port = atoi(argv[1]);

    start_server();

    handle_connections();
}

void start_server() {
    printf("Iniciando servidor en puerto %d\n", server.server_port);

    server.fd_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server.fd_socket == -1) {
        perror("Error crear socket");
        close(server.fd_socket);
        exit(1);
    }

    server.server_addr.sin_family = AF_INET;
    server.server_addr.sin_port = htons(server.server_port);
    server.server_addr.sin_addr.s_addr = INADDR_ANY; //escucha todas las interfaces de red (ips)

    if (bind(server.fd_socket, (struct sockaddr *)&server.server_addr, sizeof(server.server_addr)) == -1) {
        perror("Error bind\n");
        close(server.fd_socket);
        exit(1);
    }

    if (listen(server.fd_socket, MAX_LISTEN) == -1) {
        perror("Error listen");
        close(server.fd_socket);
        exit(1);
    }
}

void handle_connections() {
    int fd_socket_client;
    struct sockaddr_in client_addr = {0};
    char buffer[MAX_MSG];
    long int nb;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        fd_socket_client = accept(server.fd_socket, (struct sockaddr *)&client_addr, &client_len);

        if (fd_socket_client == -1) {
            perror("Error accept connection");
            continue;
        }

        printf("Conexion aceptada\n");
        printf("Client IP: %s\n", inet_ntoa(client_addr.sin_addr));
        printf("Client port: %d\n", ntohs(client_addr.sin_port));

        

        nb = recvMessage(fd_socket_client, buffer);

        while (nb > 0) { //hasta que cliente no envie \0 se reciben mensajes
            sendMessage(fd_socket_client, "Recibido\n");

            nb = recvMessage(fd_socket_client, buffer);
        }

        printf("Cerrando conexion con cliente %d\n", fd_socket_client);
    }

}

