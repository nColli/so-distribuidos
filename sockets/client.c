#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>


int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Use: %s <port_client> port_server> <ip_server>\n", argv[0]);
        exit(1);
    }

    int client_port = atoi(argv[1]);
    char* server_ip = argv[2];
    int server_port = atoi(argv[3]);
    
    printf("Conectandose a %s:%d desde %d\n", server_ip, server_port, client_port);

    //crear socket cliente
    int id_socket_client = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in socket_client;
    socket_client.sin_family = AF_INET;
    socket_client.sin_port = htons(server_port);
    socket_client.sin_addr.s_addr = inet_addr(server_ip);

    int len_socket = sizeof(socket_client);

    if (connect(id_socket_client, (struct sockaddr *)&socket_client, len_socket) >= 0) {
        char buf[1024] = "hola servidor\0";
        printf("Conexión aceptada\n");
        long int nb = write(id_socket_client, buf, sizeof(buf));
        printf("Bytes escritos en servidor %d\n", nb);
        //sleep(1);
        nb = read(id_socket_client, buf, 1024);
        buf[nb] = '\0';
        printf("Recibido del cliente %d: %s\n", id_socket_client, buf);
    } else {
        printf("Error en conexión\n");
    }

    close(id_socket_client);

}