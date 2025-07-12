#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_MSG 1024

int readSendMessage(int fd_socket, char* buffer);
int sendMessage(int fd_socket, char* mensaje);
int recvMessage(int fd_socket, char* mensaje);


#endif //PROTOCOL_H