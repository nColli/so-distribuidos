#include "san.h"


void
san_programa_1(char *host, char *nombre)
{
	CLIENT *clnt;
	struct retorno  *result_1;
	struct envia  san_1_arg;

#ifndef	DEBUG
	clnt = clnt_create (host, SAN_PROGRAMA, VERSION_SAN_PROGRAMA, "udp");
	if (clnt == NULL) {
		clnt_pcreateerror (host);
		exit (1);
	}
#endif	/* DEBUG */


	strcpy( san_1_arg.nombre , nombre ); //agregado 


	result_1 = san_1(&san_1_arg, clnt);
	if (result_1 == (struct retorno *) NULL) {
		clnt_perror (clnt, "call failed");
	}


	printf("direccion recibida del servidor %s \n", result_1->direccion); //agregago


#ifndef	DEBUG
	clnt_destroy (clnt);
#endif	 /* DEBUG */
}


int
main (int argc, char *argv[])
{
	char *host, *direccion, *archivo;

	if (argc < 2) {
		printf ("usage: %s server_host\n", argv[0]);
		exit (1);
	}
	host = argv[1];
	direccion = san_programa_1 (host, argv[2]); //envio nombre del programa y me da la direccion
	
	if (direccion == '0') {
		printf("ARCHIVO OCUPADO");
		exit(0);
	}

	//obtengo direccion compuesta de ip, llamo a procedimiento remoto para esa direccion, para obtener el archivo
	*archivo = sad_get_programa_1 (direccion)

	//modifico el archivo
	*archivo = "prueba modificacion"

	//envio archivo modificado
	sad_post_programa_1(archivo) //envio archivo modificado

	//actualizo tabla de archivos - cambio nombre a archivo disponible
	san_liberar_programa_1 (host) //host es donde esta el servidor de nombres de archivos


	exit (0);
}
