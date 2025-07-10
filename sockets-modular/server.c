#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define MAX_MSG 1024
#define MAX_LISTEN 5

typedef struct server_state {
    int fd_socket;
    int server_port;
    struct sockaddr_in server_addr;
} server_state;

server_state server = {0}; //global para fn e hilos

void start_server();

int main (int argc, char *argv[]) {
    int id_socket_client;

    if (argc < 2) {
        printf("Use: %s <port>\n", argv[0]);
        exit(1);
    }

    server.server_port = atoi(argv[1]);

    start_server();

    struct sockaddr_in socket_client;

    while (1) {
        printf("Esperando nueva conexion\n");
        socklen_t client_len = sizeof(struct sockaddr_in);
        id_socket_client = accept(server.fd_socket, (struct sockaddr *)&socket_client, &client_len);
        if (id_socket_client < 0) {
            perror("Error aceptar conexion\n");
            continue;
        }

        char buffer[30];
        int nbytes;
        printf("Conexion aceptada\n");
        printf("Client IP: %s\n", inet_ntoa(socket_client.sin_addr));
        printf("Client port: %d\n", ntohs(socket_client.sin_port));

        nbytes = read(id_socket_client, buffer, sizeof(buffer));
        buffer[nbytes] = '\0';

        while (nbytes > 0) {
            printf("Recibido del cliente %d: %s\n", id_socket_client, buffer);
            write(id_socket_client, "recibido\n", 10);
            nbytes = read(id_socket_client, buffer, sizeof(buffer));
            buffer[nbytes] = '\0';
        }

        printf("Cerrando conexion con cliente %d\n", id_socket_client);
        close(id_socket_client);
    }
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

