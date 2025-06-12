// common_nuevo.h - Definiciones comunes para el sistema de archivos distribuido
#pragma once

#include <pthread.h>

#define MAX_FILENAME 256
#define MAX_IP 16
#define DNS_PORT 9000
#define SERVER_PORT 9001
#define BUF_SIZE 1024
#define MAX_FILES 100
#define FOLDER_DOWNLOADS "descargas_distribuidas"

// Tipos de elementos en el sistema
typedef enum {
    TIPO_ARCHIVO,
    TIPO_CARPETA
} TipoElemento;

// Estructura para representar archivos y carpetas
typedef struct {
    char nombre[MAX_FILENAME];
    char ip[MAX_IP];
    int puerto;
    int bloqueado;
    TipoElemento tipo;
} EntradaElemento;

// Estructura para pasar datos a los hilos
typedef struct {
    int socket_cliente;
    char datos[BUF_SIZE];
} DatosHilo;

// Comandos del servidor de archivos
typedef enum {
    CMD_HABILITAR,
    CMD_SUBIR_ARCHIVO,
    CMD_SUBIR_CARPETA,
    CMD_ACTUALIZAR_TODO,
    CMD_SALIR
} ComandoServidor; 