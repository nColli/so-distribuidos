// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "common.h"

void request_file(const char* fname, const char* cmd) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dns_addr = {0};
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(DNS_PORT);
    inet_pton(AF_INET, "127.0.0.1", &dns_addr.sin_addr);

    connect(sock, (struct sockaddr*)&dns_addr, sizeof(dns_addr));
    char buf[BUF_SIZE];
    snprintf(buf, BUF_SIZE, "REQUEST_FILE %s %s", fname, cmd);
    send(sock, buf, strlen(buf), 0);
    recv(sock, buf, BUF_SIZE, 0);
    close(sock);

    if (strncmp(buf, "OK", 2) == 0) {
        char ip[MAX_IP];
        int port;
        sscanf(buf, "OK %s %d", ip, &port);

        int fsock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in fs_addr = {0};
        fs_addr.sin_family = AF_INET;
        fs_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &fs_addr.sin_addr);

        connect(fsock, (struct sockaddr*)&fs_addr, sizeof(fs_addr));
        if (strcmp(cmd, "READ") == 0) {
            snprintf(buf, BUF_SIZE, "GET_FILE %s", fname);
            send(fsock, buf, strlen(buf), 0);
            FILE* f = fopen("client_copy.txt", "w");
            int n;
            while ((n = recv(fsock, buf, BUF_SIZE, 0)) > 0) {
                fwrite(buf, 1, n, f);
            }
            fclose(f);
            printf("File received as client_copy.txt\n");
        } else if (strcmp(cmd, "WRITE") == 0) {
            snprintf(buf, BUF_SIZE, "GET_FILE %s", fname);
            send(fsock, buf, strlen(buf), 0);
            FILE* f = fopen("client_edit.txt", "w");
            int n;
            while ((n = recv(fsock, buf, BUF_SIZE, 0)) > 0) {
                fwrite(buf, 1, n, f);
            }
            fclose(f);
            printf("File received as client_edit.txt. Edit and press Enter to send back.\n");
            getchar();

            // Send back modified file
            int fsock2 = socket(AF_INET, SOCK_STREAM, 0);
            connect(fsock2, (struct sockaddr*)&fs_addr, sizeof(fs_addr));
            snprintf(buf, BUF_SIZE, "PUT_FILE %s", fname);
            send(fsock2, buf, strlen(buf), 0);
            FILE* f2 = fopen("client_edit.txt", "r");
            while (fgets(buf, BUF_SIZE, f2)) {
                send(fsock2, buf, strlen(buf), 0);
            }
            fclose(f2);
            close(fsock2);

            // Unlock file
            int dsock = socket(AF_INET, SOCK_STREAM, 0);
            connect(dsock, (struct sockaddr*)&dns_addr, sizeof(dns_addr));
            snprintf(buf, BUF_SIZE, "UNLOCK_FILE %s", fname);
            send(dsock, buf, strlen(buf), 0);
            recv(dsock, buf, BUF_SIZE, 0);
            close(dsock);
            printf("File sent back and unlocked.\n");
        }
        close(fsock);
    } else {
        printf("File not available or locked.\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <filename> <READ|WRITE>\n", argv[0]);
        return 1;
    }
    request_file(argv[1], argv[2]);
    return 0;
}