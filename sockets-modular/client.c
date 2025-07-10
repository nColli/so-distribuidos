#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#define MAX_MSG 1024

typedef struct client_state {
    int fd_socket;
    int client_port;
    char* server_ip;
    int server_port;
    struct sockaddr_in server_addr;
} client_state;

client_state client = {0}; //variable global

void start_client();
void connect_to_server();
void chat();
void readMessage(char *buffer);

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Use: %s <port_client> port_server> <ip_server>\n", argv[0]);
        exit(1);
    }

    client.client_port = atoi(argv[1]);
    client.server_ip = argv[2];
    client.server_port = atoi(argv[3]);

    start_client();
    
    connect_to_server();

    chat();

    printf("Cerrando conexion\n");
    close(client.fd_socket);
    exit(0);
}

void start_client() {
    struct sockaddr_in client_addr = {0};

    printf("Conectándose a %s:%d desde %d\n", client.server_ip, client.server_port, client.client_port);
    client.fd_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client.fd_socket == -1) {
        perror("Error crear socket");
        close(client.fd_socket);
        exit(1);
    }

    client.server_addr.sin_family = AF_INET;
    client.server_addr.sin_addr.s_addr = inet_addr(client.server_ip);
    client.server_addr.sin_port = htons(client.server_port);
    
    //Para que la conexion se haga desde el puerto cliente elegido:
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(client.client_port);

    if (bind(client.fd_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) == -1) {
        perror("Error bind client_addr");
        exit(1);
    }
}

void connect_to_server() {
    //crear conexión persistente con servidor
    int connection = connect(client.fd_socket, (struct sockaddr *)&client.server_addr, sizeof(client.server_addr));

    if (connection == -1) {
        perror("Connect failed");
        close(client.fd_socket);
        exit(1);
    }

    printf("Conectado al servidor\n");
}

void chat() {
    long int nb;

    char buffer[MAX_MSG];
    readMessage(buffer);

    while (buffer[0] != '\0') {
        nb = write(client.fd_socket, buffer, strlen(buffer));

        printf("%ld bytes enviados al servidor\n", nb);

        nb = read(client.fd_socket, buffer, MAX_MSG);
        buffer[nb] = '\0';

        printf("Servidor: %s", buffer);
        readMessage(buffer);
    }
}

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