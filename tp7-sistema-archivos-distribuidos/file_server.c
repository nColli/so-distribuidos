// file_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "common.h"
#include <dirent.h>

#define MAX_FILES 100

char registered_files[MAX_FILES][MAX_FILENAME];
int registered_count = 0;

int register_file(const char* fname, const char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dns_addr = {0};
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(DNS_PORT);
    inet_pton(AF_INET, "127.0.0.1", &dns_addr.sin_addr);

    connect(sock, (struct sockaddr*)&dns_addr, sizeof(dns_addr));
    char buf[BUF_SIZE];
    snprintf(buf, BUF_SIZE, "REGISTER_FILE %s %s %d", fname, ip, port);
    send(sock, buf, strlen(buf), 0);
    int n = recv(sock, buf, BUF_SIZE, 0);
    buf[n] = '\0';
    close(sock);
    if (strncmp(buf, "OK", 2) == 0) {
        strcpy(registered_files[registered_count], fname);
        registered_count++;
        printf("[SERVER] Registered file: %s\n", fname);
        return 1;
    } else if (strncmp(buf, "DUPLICATE", 9) == 0) {
        printf("[SERVER] File %s already registered elsewhere. Skipping.\n", fname);
        return 0;
    }
    return 0;
}

void register_all_txt_files(const char* ip, int port) {
    DIR* d = opendir(".");
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        if (strstr(dir->d_name, ".txt") && strlen(dir->d_name) > 4) {
            register_file(dir->d_name, ip, port);
        }
    }
    closedir(d);
}

void handle_client(int client_sock) {
    char buf[BUF_SIZE];
    int n = recv(client_sock, buf, BUF_SIZE, 0);
    buf[n] = '\0';
    char cmd[16], fname[MAX_FILENAME];
    fname[0] = '\0';
    sscanf(buf, "%s %s", cmd, fname);
    printf("[SERVER] Received command: '%s' for file: '%s'\n", cmd, fname);
    if (strcmp(cmd, "GET_FILE") == 0 && strlen(fname) > 0) {
        // Only serve if file is registered
        int found = 0;
        for (int i = 0; i < registered_count; i++) {
            if (strcmp(registered_files[i], fname) == 0) {
                found = 1;
                break;
            }
        }
        if (found) {
            printf("[SERVER] Serving file: %s\n", fname);
            FILE* f = fopen(fname, "r");
            if (f) {
                while (fgets(buf, BUF_SIZE, f)) {
                    send(client_sock, buf, strlen(buf), 0);
                }
                fclose(f);
            } else {
                printf("[SERVER] Could not open file: %s\n", fname);
            }
        } else {
            printf("[SERVER] File not registered: %s\n", fname);
        }
    } else if (strcmp(cmd, "PUT_FILE") == 0 && strlen(fname) > 0) {
        int found = 0;
        for (int i = 0; i < registered_count; i++) {
            if (strcmp(registered_files[i], fname) == 0) {
                found = 1;
                break;
            }
        }
        if (found) {
            printf("[SERVER] Receiving and updating file: %s\n", fname);
            FILE* f = fopen(fname, "w");
            while ((n = recv(client_sock, buf, BUF_SIZE, 0)) > 0) {
                fwrite(buf, 1, n, f);
            }
            fclose(f);
        } else {
            printf("[SERVER] File not registered for update: %s\n", fname);
        }
    } else {
        printf("[SERVER] Unknown or malformed command: '%s'\n", buf);
    }
    close(client_sock);
}

int main() {
    const char* ip = "127.0.0.1";
    int port = SERVER_PORT;

    register_all_txt_files(ip, port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 5);
    printf("File server running on port %d\n", port);

    while (1) {
        int client_sock = accept(sock, NULL, NULL);
        handle_client(client_sock);
    }
}