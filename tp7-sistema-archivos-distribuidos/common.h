// common.h
#pragma once

#define MAX_FILENAME 256
#define MAX_IP 16
#define DNS_PORT 9000
#define SERVER_PORT 9001
#define BUF_SIZE 1024

typedef struct {
    char fileName[MAX_FILENAME];
    char ip[MAX_IP];
    int port;
    int lock;
} FileEntry;