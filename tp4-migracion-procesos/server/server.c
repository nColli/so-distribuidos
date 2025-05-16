#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h> // For open()

#define IP "192.168.1.68" //Ubicacion del servidor
#define PUERTO 9003

void UbicacionDelCliente(struct sockaddr_in);
void RecibeEnviaComandos(int);


main(int argc, char *argv[])
{
  struct sockaddr_in s_sock,c_sock;
  int idsocks,idsockc;
  int lensock = sizeof(struct sockaddr_in);
  idsocks = socket(AF_INET, SOCK_STREAM, 0);
  printf("idsocks %d\n",idsocks);
  s_sock.sin_family      = AF_INET;
  s_sock.sin_port        = htons(PUERTO);
  s_sock.sin_addr.s_addr = inet_addr(IP);
  memset(s_sock.sin_zero,0,8);


  printf("bind %d\n", bind(idsocks,(struct sockaddr *) &s_sock,lensock));
  printf("listen %d\n",listen(idsocks,5));
  while(1)
    {
      printf("esperando conexion\n");    
      idsockc = accept(idsocks,(struct sockaddr *)&c_sock,&lensock);
      if(idsockc != -1)
         {
           /* Ubicacion del Cliente */
            printf("conexion aceptada desde el cliente\n");
            UbicacionDelCliente(c_sock);
           /*--------------------------------------------------*/

           RecibeEnviaComandos(idsockc);
           
           close(idsockc);
         }
      else 
         {
            printf("conexion rechazada %d \n",idsockc);
         }
    }
}

void RecibeEnviaComandos(int idsockc)
{
    char buf[1024]; // Buffer to receive file content
    int nb;

    // Receive the file content from the client
    nb = read(idsockc, buf, sizeof(buf) - 1);
    if (nb <= 0) {
        perror("Error al recibir el archivo del cliente");
        return;
    }

    buf[nb] = '\0'; // Null-terminate the received content

    // Save the received content to a file named "codigo.c"
    int file_descriptor = open("codigo.c", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_descriptor < 0) {
        perror("Error al crear el archivo codigo.c");
        return;
    }

    if (write(file_descriptor, buf, nb) < 0) {
        perror("Error al escribir en el archivo codigo.c");
        close(file_descriptor);
        return;
    }

    close(file_descriptor);

    // Compile the received file
    if (system("gcc codigo.c -o codigo.out") != 0) {
        char *error_message = "Error al compilar el archivo\n";
        send(idsockc, error_message, strlen(error_message), 0);
        close(idsockc); // Ensure the socket is properly closed
        return;
    }

    // Execute the compiled file and capture its output
    FILE *output = popen("./codigo.out", "r");
    if (!output) {
        char *error_message = "Error al ejecutar el archivo\n";
        send(idsockc, error_message, strlen(error_message), 0);
        return;
    }

    char output_buffer[1024];
    size_t output_size;

    // Read the output and send it back to the client
    while ((output_size = fread(output_buffer, 1, sizeof(output_buffer) - 1, output)) > 0) {
        output_buffer[output_size] = '\0';
        send(idsockc, output_buffer, output_size, 0);
    }

    pclose(output);
}

void UbicacionDelCliente(struct sockaddr_in c_sock)
{
  printf("............c_sock.sin_family %d\n",c_sock.sin_family);
  printf("............c_sock.sin_port %d\n",c_sock.sin_port);
  printf("............c_sock.sin_addr.s_addr %s\n\n", inet_ntoa(c_sock.sin_addr));
}
