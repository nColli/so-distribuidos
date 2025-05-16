#include "sna.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


const char* get_field(const char* line, int num) {
	char* line_copy = strdup(line);
	if (!line_copy) {
		return NULL; 
	}
	char* token;
	char* saveptr = NULL;

	for (token = strtok_r(line_copy, ",", &saveptr);
		 token;
		 token = strtok_r(NULL, ",", &saveptr)) {
		if (--num == 0) {
			char* result = strdup(token); 
			free(line_copy); 
			return result;
		}
	}
	free(line_copy); 
	return NULL;
}

void bloquear(char *buff, FILE *fp) {
	if (strcmp(get_field(buff, 3), "U") == 0) { 
		char *lastField = strstr(buff, "U");  
		if (lastField) {
			*lastField = 'L';

			fseek(fp, -strlen(buff), SEEK_CUR);

			fprintf(fp, "%s", buff);
			fflush(fp);
		}
	}
}

//devuelve la direccion y lo bloquea
void getDireccion(char* nombre, char* direccion) {
	int flag = 1;
	strcpy(direccion, "0");

    FILE *fp = fopen("tabla.txt", "r+");
	printf("tabla encontrada\n");
    if (fp == NULL) {
        printf("error reading file\n");
        //return "0";
		flag = 0;
    }

    char buff[256];
    while ( flag && (fgets(buff, sizeof(buff), fp) != NULL)) {
		const char* nombreFila = get_field(buff, 1);
		printf("leyendo fila %s\n", nombreFila);
		//printf("archivo: %s", get_field(strdup(buff), 1));
		
		if (strcmp(nombreFila, nombre) == 0) {
			printf("fila encontrada \n");
			//strcpy(direccion, get_field(buff, 2));
			strcpy(direccion, get_field(buff, 2));

			if (strcmp(get_field(buff, 3), "L") == 0) { //si es igual a L ya esta bloqueado, no puedo tener acceso a archivo
				//return "0";
				bloquear(buff, fp);	
				flag = 0;
			} else {
				//bloquear el archivo cambiando U (unlock) por L (lock)
				flag = 1;

				//return direccion;
			}
		}
    }

	//return "0";
}


struct retornoDireccion *
sna_solicitar_1_svc(struct envia *argp, struct svc_req *rqstp)
{
	static struct retornoDireccion  result;
	char direccion[15];


	printf("\niniciando busqueda de archivo\n");

	//memset(direccion, 0, 15);
	getDireccion(argp->nombre, direccion);
	
	printf("direccion %s\n", direccion);

	if (strcmp(direccion, "0") == 0) {
		printf("DIRECCION NO ENCONTRADA o ARCHIVO NO DISPONIBLE\n");
		result.disponible = 0;
		strcpy(result.direccion, "0");
	} else {
		printf("Direccion encontrada y disponible\n");
		result.disponible = 1;
		strcpy(result.direccion, direccion);
		printf("direccion result %s\n", result.direccion);
	}
	
	printf("test del server\n");

	printf("result final dir: %s, disp: %d\n", result.direccion, result.disponible);

	

	return &result;
}
