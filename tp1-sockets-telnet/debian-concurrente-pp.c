/*
Mini servidor concurrente con procesos pesados de comandos Debian
*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#define IP "192.168.1.68" //Ubicacion del servidor
#define PUERTO 6666

void UbicacionDelCliente(struct sockaddr_in);
void RecibeEnviaComandos(int);

main(int argc, char *argv[])
{
    struct sockaddr_in s_sock,c_sock;
    int idsocks,idsockc;
    int lensock = sizeof(struct sockaddr_in);
    idsocks = socket(AF_INET, SOCK_STREAM, 0);

    printf("idsocks %d\n",idsocks);

    s_sock.sin_family = AF_INET;
    s_sock.sin_port = htons(PUERTO);
    s_sock.sin_addr.s_addr = inet_addr(IP);
    memset(s_sock.sin_zero,0,8); //arregla formato

    printf("bind %d\n", bind(idsocks,(struct sockaddr *) &s_sock,lensock));
    printf("listen %d\n",listen(idsocks,5));

    while(1)
    {
        printf("esperando conexion\n");
        idsockc = accept(idsocks,(struct sockaddr *)&c_sock,&lensock);
        if(idsockc != -1)
        {
            if (!fork())
            {
                /* Ubicacion del Cliente */
                printf("conexion aceptada desde el cliente\n");
                UbicacionDelCliente(c_sock);
                /*--------------------------------------------------*/
                RecibeEnviaComandos(idsockc);
                close(idsockc);
                exit(0); //Se convierte en zombie
            }
        }
        else
        {
            printf("conexion rechazada %d \n",idsockc);
        }
    }
}

void RecibeEnviaComandos(int idsockc)
{
    char buf[30];
    int nb;
    int defout ;
    nb = read(idsockc,buf,30);
    while(strcmp(buf,"exit"))
    {
        buf[nb] = '\0';
        printf(".......recibido del cliente %d : %s\n",idsockc,buf);
        defout = dup(1); //copia el file descriptor para las salidas a la terminal y lo almacena en defout
        dup2(idsockc,1); //redirecciona salida a el idsockc, para que todo lo escrito en stdout sea enviado al cliente
        system(buf);
        dup2(defout,1); //vuelve todo a la normalidad para que se vea la salida de stdout en el server
        close(defout);
        nb = read(idsockc,buf,30); // lee el nuevo comando enviado por el cliente 
    }
}

void UbicacionDelCliente(struct sockaddr_in c_sock)
{
    printf("............c_sock.sin_family %d\n",c_sock.sin_family);
    printf("............c_sock.sin_port %d\n",c_sock.sin_port);
    printf("............c_sock.sin_addr.s_addr %s\n\n", inet_ntoa(c_sock.sin_addr));
}
