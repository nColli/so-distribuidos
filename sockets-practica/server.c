#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#define MAX_MSG 1024
#define MAX_CON 5
#define MSG_RCV "recibido\n"


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <puerto>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    printf("Puerto elegido %d\n", port);

    int fd_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in dir_socket;
    memset(&dir_socket, 0, sizeof(dir_socket));
    dir_socket.sin_family = AF_INET;
    dir_socket.sin_port = htons(port);
    dir_socket.sin_addr.s_addr = INADDR_ANY;

    int len_dir_socket = sizeof(struct sockaddr_in);

    bind(fd_socket, (struct sockaddr *)&dir_socket, len_dir_socket);

    listen(fd_socket, MAX_CON);

    int fd_socket_client;
    struct sockaddr_in dir_client;

    while (1) {
        printf("Servidor corriendo esperando conexion\n");
        fd_socket_client = accept(fd_socket, (struct sockaddr *)&dir_client, &len_dir_socket);
        if (fd_socket_client != -1) {
            char buf[MAX_MSG];
            int nbytes;
            printf("Conexion aceptada\n");
            nbytes = read(fd_socket_client, buf, MAX_MSG);
            buf[nbytes] = '\0';

            printf("%d bytes recibidos del cliente\n", nbytes);

            printf("Mensaje del cliente %d: %s\n", fd_socket_client, buf);

            write(fd_socket_client, MSG_RCV, strlen(MSG_RCV));
            
            close(fd_socket_client);
        }
    }

    close(fd_socket);
    exit(0);
}

