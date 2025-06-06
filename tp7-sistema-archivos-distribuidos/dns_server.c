// dns_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "common.h"

FileEntry fileTable[10];
int fileCount = 0;

void print_file_table() {
    printf("\nCurrent File Table:\n");
    printf("%-20s %-16s %-6s %-4s\n", "FileName", "IP", "Port", "Lock");
    for (int i = 0; i < fileCount; i++) {
        printf("%-20s %-16s %-6d %-4d\n", fileTable[i].fileName, fileTable[i].ip, fileTable[i].port, fileTable[i].lock);
    }
    printf("-----------------------------\n");
}

void handle_client(int client_sock) {
    char buf[BUF_SIZE];
    int n = recv(client_sock, buf, BUF_SIZE, 0);
    if (n <= 0) {
        close(client_sock);
        return;
    }
    buf[n] = '\0'; // Ensure null-terminated
    printf("[DNS] Received request: %s\n", buf);

    if (strncmp(buf, "REGISTER_FILE", 13) == 0) {
        // Format: REGISTER_FILE filename ip port
        char fname[MAX_FILENAME], ip[MAX_IP];
        int port;
        sscanf(buf, "REGISTER_FILE %s %s %d", fname, ip, &port);
        // Check for duplicate file name
        for (int i = 0; i < fileCount; i++) {
            if (strcmp(fileTable[i].fileName, fname) == 0) {
                printf("[DNS] Duplicate file registration attempt: %s\n", fname);
                send(client_sock, "DUPLICATE\n", 10, 0);
                close(client_sock);
                return;
            }
        }
        strcpy(fileTable[fileCount].fileName, fname);
        strcpy(fileTable[fileCount].ip, ip);
        fileTable[fileCount].port = port;
        fileTable[fileCount].lock = 0;
        fileCount++;
        printf("[DNS] Registered file: %s at %s:%d\n", fname, ip, port);
        send(client_sock, "OK\n", 3, 0);
        print_file_table();
    } else if (strncmp(buf, "REQUEST_FILE", 12) == 0) {
        // Format: REQUEST_FILE filename command
        char fname[MAX_FILENAME], cmd[8];
        sscanf(buf, "REQUEST_FILE %s %s", fname, cmd);
        printf("[DNS] Client requests file '%s' with command '%s'\n", fname, cmd);
        for (int i = 0; i < fileCount; i++) {
            if (strcmp(fileTable[i].fileName, fname) == 0) {
                char reply[BUF_SIZE];
                if (strcmp(cmd, "WRITE") == 0 && fileTable[i].lock == 0) {
                    fileTable[i].lock = 1;
                    snprintf(reply, BUF_SIZE, "OK %s %d\n", fileTable[i].ip, fileTable[i].port);
                } else if (strcmp(cmd, "READ") == 0) {
                    snprintf(reply, BUF_SIZE, "OK %s %d\n", fileTable[i].ip, fileTable[i].port);
                } else {
                    snprintf(reply, BUF_SIZE, "LOCKED\n");
                }
                send(client_sock, reply, strlen(reply), 0);
                printf("[DNS] Sent reply: %s", reply);
                print_file_table();
                close(client_sock);
                return;
            }
        }
        send(client_sock, "NOT_FOUND\n", 10, 0);
    } else if (strncmp(buf, "UNLOCK_FILE", 11) == 0) {
        // Format: UNLOCK_FILE filename
        char fname[MAX_FILENAME];
        sscanf(buf, "UNLOCK_FILE %s", fname);
        for (int i = 0; i < fileCount; i++) {
            if (strcmp(fileTable[i].fileName, fname) == 0) {
                fileTable[i].lock = 0;
                printf("[DNS] Unlocked file: %s\n", fname);
                send(client_sock, "UNLOCKED\n", 9, 0);
                print_file_table();
                close(client_sock);
                return;
            }
        }
        send(client_sock, "NOT_FOUND\n", 10, 0);
    }
    close(client_sock);
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DNS_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 5);
    printf("DNS server running on port %d\n", DNS_PORT);

    while (1) {
        int client_sock = accept(sock, NULL, NULL);
        handle_client(client_sock);
    }
}