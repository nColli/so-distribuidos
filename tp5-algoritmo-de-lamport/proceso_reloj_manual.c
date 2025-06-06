#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>

#define MAX_MSG 128

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int max(int a, int b) {
    return a > b ? a : b;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <puerto> <reloj_inicial>\n", argv[0]);
        exit(1);
    }
    int my_port = atoi(argv[1]);
    int logical_clock = atoi(argv[2]);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[MAX_MSG];

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        error("socket failed");
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        error("setsockopt");
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(my_port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        error("bind failed");
    if (listen(server_fd, 3) < 0)
        error("listen");

    // Set stdin to non-blocking
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);

    printf("Proceso iniciado en el puerto %d con logical clock %d\n", my_port, logical_clock);
    printf("Ingresa un numero de puerto y presiona enter para enviar el mensaje.\n");

    fd_set readfds;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(0, &readfds); // stdin
        int maxfd = server_fd > 0 ? server_fd : 0;
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0) continue;

        // Check for incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
                error("accept");
            memset(buffer, 0, MAX_MSG);
            int valread = read(new_socket, buffer, MAX_MSG);
            if (valread > 0) {
                int recv_clock;
                sscanf(buffer, "%d", &recv_clock);
                int old_clock = logical_clock;
                logical_clock = max(logical_clock, recv_clock) + 1;
                printf("[RECV] Clock recibido=%d | Mi clock: %d -> %d\n", recv_clock, old_clock, logical_clock);
            }
            close(new_socket);
        }

        // Check for user input
        if (FD_ISSET(0, &readfds)) {
            char input[32];
            if (fgets(input, sizeof(input), stdin)) {
                int dest_port = atoi(input);
                if (dest_port > 0) {
                    int sock = 0;
                    struct sockaddr_in serv_addr;
                    char msg[MAX_MSG];
                    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                        printf("Socket creation error\n");
                        continue;
                    }
                    serv_addr.sin_family = AF_INET;
                    serv_addr.sin_port = htons(dest_port);
                    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                        printf("Connection to port %d failed\n", dest_port);
                        close(sock);
                        continue;
                    }
                    logical_clock++;
                    snprintf(msg, MAX_MSG, "%d", logical_clock);
                    printf("[SEND] A puerto %d: clock=%d\n", dest_port, logical_clock);
                    send(sock, msg, strlen(msg), 0);
                    close(sock);
                }
            }
        }
    }
    close(server_fd);
    return 0;
}
