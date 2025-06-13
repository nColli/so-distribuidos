#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <sys/socket.h>

#define MAX_MSG 512
#define MAX_CONTENT_SIZE 4096

/**
 * Sends a command with content to the specified socket
 * @param socket Socket to send data to
 * @param command Command as integer (0-9), will be converted to char
 * @param content Content to send (page number or page content)
 * @return 0 on success, -1 on error
 */
int send_command(int socket, int command, const char* content);

/**
 * Receives a message from the specified socket
 * @param socket Socket to receive data from
 * @return Pointer to received message (caller must free), NULL on error
 */
char* recv_command(int socket);

#endif // COMMUNICATION_H 