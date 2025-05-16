#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h> // For open()
#include <string.h> // For strlen()

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

//#define IP "192.168.1.68" //Ubicacion del servidor
#define IP "172.20.10.2"
#define PORT 9007

int main() 
{
	//crear socket
	int network_socket;
	network_socket = socket(AF_INET, SOCK_STREAM, 0);
	
	//especificar direccion para el socket
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET; //que tipo de address vamos a trabajar
	server_address.sin_port = htons(PORT); //fn de conversion para q entienda el nro
    server_address.sin_addr.s_addr = inet_addr(IP);

	int connection_status = connect(network_socket, (struct sockaddr *) &server_address, sizeof(server_address));
	
	//error handling
	if (connection_status == -1) {
		printf("Error al establecer la conexion\n");
	}
	
	//dsp de establecer conexion tengo que recibir o enviar datos del servidor
	//char server_response[256];
	//recv(network_socket, &server_response, sizeof(server_response), 0);
	
	//imprimir el mensaje que recibi del servidor de max 256 bytes
	//printf("Data del server: %s\n", server_response);

	// abro el archivo a enviar
	int file_descriptor = open("codigo.c", O_RDONLY);
	if (file_descriptor < 0) {
		perror("Error al abrir el archivo codigo.c");
		return 1;
	}

	// tamaño maximo archivo
	char file_buffer[1024];
	ssize_t bytes_read = read(file_descriptor, file_buffer, sizeof(file_buffer) - 1);
	if (bytes_read < 0) {
		perror("Error al leer el archivo codigo.c");
		close(file_descriptor);
		return 1;
	}

	// enviar archivo
	if (send(network_socket, file_buffer, bytes_read, 0) < 0) {
		perror("Error al enviar el archivo al servidor");
		close(file_descriptor);
		return 1;
	}

	printf("Codigo enviado al servidor con éxito.\n");

	// respuesta del servidor
	char server_output[1024];
	int bytes_received;

	printf("Respuesta del servidor:\n");
	while ((bytes_received = recv(network_socket, server_output, sizeof(server_output) - 1, 0)) > 0) {
		server_output[bytes_received] = '\0';
		printf("%s", server_output);
	}

	if (bytes_received < 0) {
		perror("Error al recibir la respuesta del servidor");
	}

	close(file_descriptor);

	//cerrar conexion
	close(network_socket);

	return 0;
}
