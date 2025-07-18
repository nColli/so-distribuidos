#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#define MAX_MSG 1024

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <ip_servidor> <puerto_servidor>\n", argv[0]);
        exit(1);
    }

    char* server_ip = argv[1];
    int server_port = atoi(argv[2]);

    int fd_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in dir_server;
    memset(&dir_server, 0, sizeof(struct sockaddr_in));
    dir_server.sin_family = AF_INET;
    dir_server.sin_port = htons(server_port);
    dir_server.sin_addr.s_addr = inet_addr(server_ip);

    int len_dir = sizeof(struct sockaddr_in);

    int fd_socket_server = connect(fd_socket, (struct sockaddr *)&dir_server, len_dir);
    if (fd_socket_server != -1) {
        int nbytes;
        char buf[MAX_MSG] = "Hola servidor\0";
        printf("Conexion aceptada\n");
        nbytes = write(fd_socket, buf, strlen(buf));
        printf("%d bytes enviados al servidor\n", nbytes);

        nbytes = read(fd_socket, buf, MAX_MSG);
        buf[nbytes] = '\0';
        printf("%d bytes recibidos del servidor\n", nbytes);
        printf("Mensaje del servidor: %s", buf);
    }

    close(fd_socket_server);
    close(fd_socket);
    exit(0);
}