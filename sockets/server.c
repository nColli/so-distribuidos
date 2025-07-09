#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main (int argc, char *argv[]) {
    struct sockaddr_in socket_server, client_socket;
    int id_socket_server, id_socket_client, len_socket;

    if (argc < 2) {
        printf("Use: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    printf("Port: %d\n", port);
    
    id_socket_server = socket(AF_INET, SOCK_STREAM, 0);
    if (id_socket_server < 0) {
        perror("Error al crear socket\n");
        exit(1);
    }

    socket_server.sin_family = AF_INET;
    socket_server.sin_port = htons(port);
    socket_server.sin_addr.s_addr = INADDR_ANY; //bind the server socket to all available nw interfaces

    len_socket = sizeof(struct sockaddr_in);

    if (bind(id_socket_server, (struct sockaddr *)&socket_server, len_socket) < 0) {
        perror("Error bind\n");
        close(id_socket_server);
        exit(1);
    }

    if (listen(id_socket_server, 5) < 0) {
        perror("Error listen\n");
        close(id_socket_server);
        exit(1);
    }

    while (1) {
        printf("Esperando conexion\n");
        id_socket_client = accept(id_socket_server, (struct sockaddr *)&client_socket, &len_socket);
        if (id_socket_client < 0) {
            perror("Error aceptar conexion\n");
            continue;
        }

        char buffer[30];
        int nbytes;
        printf("Conexion aceptada\n");
        nbytes = read(id_socket_client, buffer, sizeof(buffer));
        buffer[nbytes] = '\0';

        printf("Recibido del cliente %d: %s\n", id_socket_client, buffer);
        write(id_socket_client, "recibido\n", 10);
        close(id_socket_client);
    }

    exit(0);
}