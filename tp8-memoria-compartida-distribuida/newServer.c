#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include "communication.h"

#define MAX_MSG 512
#define SHM_KEY 0x1234

typedef struct process_node {
    int client_port;
    int client_socket;
    struct process_node *next;
} process_node_t;

typedef struct {
    int lock;
    pthread_mutex_t page_mutex;  // mutex para cada página - para que op de lockear o ver si esta lock sean atomicas
    process_node_t *queue; //puntero a cola de espera
} page_entry_t; //fila de tabla de paginas

// Variables globales para acceso desde hilos
int port;
page_entry_t *page_table;
int page_size;
int num_pages;
void *shared_memory;

// Estructura para pasar datos a hilos
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_data_t;

// Declaraciones de funciones
void recv_command_page_num(int client_socket, int *command, int *page_num);
void read_page(int client_socket, int page_num);
void read_lock_page(int client_socket, int page_num);
void write_page(int client_socket, int page_num, const char* content);
void *handle_client(void *arg);
void print_page_table_state(const char *operation_context);
// Funciones auxiliares para manejo de cola
void add_to_queue(process_node_t **queue, int client_socket);
int remove_from_queue(process_node_t **queue);
void recv_command_page_num_content(int client_socket, int *command, int *page_num, char **content);

int main (int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <puerto> <tamano_pagina> (bytes) <num_paginas>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[1]);
    page_size = atoi(argv[2]);
    num_pages = atoi(argv[3]);
    int shm_size = page_size * num_pages;

    int shmid = shmget(SHM_KEY, shm_size, 0666);
    if (shmid != -1) {
        printf("Shared memory ya existe\n");
    } else {
        printf("Creando memoria compartida\n");
        shmid = shmget(SHM_KEY, shm_size, IPC_CREAT | 0666);
        if (shmid == -1) {
            perror("shmget");
            exit(1);
        }
    }

    // tabla de paginas
    page_table = calloc(num_pages, sizeof(page_entry_t)); //calloc aloja mem y pone en 0
    if (!page_table) {
        perror("calloc");
        exit(1);
    }
    for (int i = 0; i < num_pages; i++) {
        page_table[i].queue = NULL;
        page_table[i].lock = 0;
        if (pthread_mutex_init(&page_table[i].page_mutex, NULL) != 0) {
            perror("Error inicializando mutex de página");
            exit(1);
        }
    }

    // Atach shared memory
    shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    //configurar socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error al crear el socket\n");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt fallo");
        close(server_socket);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error bind socket");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, 5) < 0) {
        perror("Error listen socket");
        close(server_socket);
        return 1;
    }

    printf("Servidor iniciado en puerto %d\n", port);

    while (1) {
        socklen_t client_len = sizeof(struct sockaddr_in);
        struct sockaddr_in client_addr;
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            perror("Error al aceptar la conexion");
            continue;
        }

        // Crear estructura para pasar datos al thread
        client_data_t *client_data = malloc(sizeof(client_data_t));
        if (!client_data) {
            perror("Error al asignar memoria para client_data");
            close(client_socket);
            continue;
        }
        
        client_data->client_socket = client_socket;
        client_data->client_addr = client_addr;

        // handle_client - logica de manejar cliente
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_data) != 0) {
            perror("Error al crear thread");
            free(client_data);
            close(client_socket);
            continue;
        }

        // Detach thread para que se libere automáticamente
        pthread_detach(thread_id);
    } 

    close(server_socket);
    return 0;
}

void *handle_client(void *arg) {
    client_data_t *data = (client_data_t *)arg;
    int client_socket = data->client_socket;
    struct sockaddr_in client_addr = data->client_addr;
    //arg - socket y direccion del cliente que comunico con servidor

    printf("\n\n================================================\n");
    printf("Thread creado para cliente ubicado en %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    //cliente comunica con comando y un numero de pagina requerido, buscar si pag esta ocupada
    int command, page_num;
    char* content;
    recv_command_page_num_content(client_socket, &command, &page_num, &content);

    //printf("Comando: %d, Pagina pedida por el cliente: %d\n", command, page_num);

    print_page_table_state("cliente conectado");

    /*COMANDOS
    invalido = -1
    leer = 0
    lecutura bloqueante = 1
    escribir = 2
    (capacidad de agregar)
    */
   if (command == 0) {
        read_page(client_socket, page_num);
   } else if (command == 1) {
        //lectura bloqueante
        read_lock_page(client_socket, page_num); //aca espero el comando write del cliente
   }
   
    
    // Cerrar socket y liberar memoria al final
    close(client_socket);
    if (content != NULL) {
        free(content);
    }
    free(data);
    printf("Thread terminado para cliente\n");
    return NULL;
}

void recv_command_page_num_content(int client_socket, int *command, int *page_num, char **content) {
    char buffer[MAX_MSG];
    
    // temporalmente se bloquea para comunicación sincronica
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags & ~O_NONBLOCK);

    // Recibir datos desde el cliente
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        perror("Error al recibir comando del cliente");
        fcntl(client_socket, F_SETFL, flags);
        *command = -1;
        *page_num = -1;
        *content = NULL;
        return;
    }

    printf("NUEVA CONEXION\n");
    
    // poner fin a string recibido
    buffer[bytes_received] = '\0';
    
    // Buscar el primer espacio y el segundo espacio para parsear: "command page_num content"
    char *first_space = strchr(buffer, ' ');
    if (first_space == NULL) {
        printf("Error: formato de mensaje invalido (sin espacios): %s\n", buffer);
        fcntl(client_socket, F_SETFL, flags);
        *command = -1;
        *page_num = -1;
        *content = NULL;
        return;
    }
    
    char *second_space = strchr(first_space + 1, ' ');
    if (second_space == NULL) {
        // Solo hay comando y página (formato: "command page_num")
        int parsed = sscanf(buffer, "%d %d", command, page_num);
        if (parsed != 2) {
            printf("Error: formato de mensaje invalido: %s\n", buffer);
            fcntl(client_socket, F_SETFL, flags);
            *command = -1;
            *page_num = -1;
            *content = NULL;
            return;
        }
        *content = NULL; // No hay contenido
    } else {
        // Hay comando, página y contenido (formato: "command page_num content")
        *first_space = '\0';  // Separar comando
        *second_space = '\0'; // Separar página
        
        *command = atoi(buffer);
        *page_num = atoi(first_space + 1);
        
        // Asignar memoria para el contenido y copiarlo
        char *content_start = second_space + 1;
        int content_length = strlen(content_start);
        *content = malloc(content_length + 1);
        if (*content == NULL) {
            perror("Error al asignar memoria para contenido");
            fcntl(client_socket, F_SETFL, flags);
            *command = -1;
            *page_num = -1;
            return;
        }
        strcpy(*content, content_start);
    }
    
    printf("Comando recibido: %d, Pagina: %d", *command, *page_num);
    if (*content != NULL) {
        printf(", Contenido: %.50s%s\n", *content, strlen(*content) > 50 ? "..." : "");
    } else {
        printf(", Sin contenido\n");
    }
    
    // restaurar flags de los sockets originales
    fcntl(client_socket, F_SETFL, flags);
}

void read_page(int client_socket, int page_num) {
    // Validar número de página
    if (page_num < 0 || page_num >= num_pages) {
        printf("Error: número de página inválido %d\n", page_num);
        char error_msg[] = "ERROR: Página inválida";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // Calcular offset en memoria compartida
    char *page_start = (char*)shared_memory + (page_num * page_size);
    
    printf("Leyendo página %d, enviando %d bytes al cliente\n", page_num, page_size);
    
    // Enviar contenido de la página al cliente
    int bytes_sent = send(client_socket, page_start, page_size, 0);
    if (bytes_sent < 0) {
        perror("Error al enviar página al cliente");
    } else {
        printf("Página %d enviada exitosamente (%d bytes)\n", page_num, bytes_sent);
    }
    
    print_page_table_state("después de operación de lectura");
}

void read_lock_page(int client_socket, int page_num) {
    /* Lectura bloqueante
    Buscar pagina si esta lock = 1, enviar comando "1" al cliente que esta esperando la pagina
    eso le indica que tiene que esperar a que se desbloquee, y agregar client_Socket a cola de espera
    
    Si lock = 0, enviar comando "0 content" al cliente que esta esperando la pagina
    y agregar a la cola como primer y unico elemento
    */
   // Validar número de página
    if (page_num < 0 || page_num >= num_pages) {
        printf("Error: número de página inválido %d\n", page_num);
        char error_msg[] = "ERROR: Página inválida";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    printf("Cliente socket %d solicita lectura bloqueante de página %d\n", client_socket, page_num);

    // Bloquear mutex de la página para acceso atomico
    pthread_mutex_lock(&page_table[page_num].page_mutex);
    
    if (page_table[page_num].lock == 1) {
        // PÁGINA BLOQUEADA - enviar comando "1" y agregar a cola de espera
        printf("Página %d bloqueada, agregando cliente socket %d a cola de espera\n", page_num, client_socket);
        
        // Enviar comando "1" al cliente indicando que debe esperar
        int send_result = send_command(client_socket, 1, "");
        if (send_result != 0) {
            printf("Error: no se pudo enviar comando de espera al cliente socket %d (posiblemente desconectado)\n", client_socket);
            pthread_mutex_unlock(&page_table[page_num].page_mutex);
            return;
        }
        
        // Agregar cliente a cola de espera
        add_to_queue(&page_table[page_num].queue, client_socket);
        
        pthread_mutex_unlock(&page_table[page_num].page_mutex);
        print_page_table_state("cliente agregado a cola para lectura bloqueante");

        // IMPORTANTE: El cliente debe esperar aquí hasta que sea su turno
        // En lugar de usar send_command para notificar, manejaremos esto directamente
        printf("Cliente socket %d esperando en cola para página %d\n", client_socket, page_num);
        
        // Esperar hasta que este cliente esté en el tope de la cola
        while (1) {
            pthread_mutex_lock(&page_table[page_num].page_mutex);
            if (page_table[page_num].queue != NULL && 
                page_table[page_num].queue->client_socket == client_socket) {
                // Es nuestro turno
                printf("Es el turno del cliente socket %d para página %d\n", client_socket, page_num);
                
                // Enviar contenido de la página
                char *page_start = (char*)shared_memory + (page_num * page_size);
                char *content = malloc(page_size + 1);
                if (content) {
                    memcpy(content, page_start, page_size);
                    content[page_size] = '\0';
                    
                    int send_result = send_command(client_socket, 0, content);
                    if (send_result != 0) {
                        printf("Error: no se pudo enviar contenido al cliente socket %d\n", client_socket);
                        free(content);
                        pthread_mutex_unlock(&page_table[page_num].page_mutex);
                        return;
                    }
                    
                    printf("Contenido enviado al cliente socket %d para página %d\n", client_socket, page_num);
                    free(content);
                } else {
                    printf("Error: no se pudo asignar memoria para contenido\n");
                    pthread_mutex_unlock(&page_table[page_num].page_mutex);
                    return;
                }
                
                pthread_mutex_unlock(&page_table[page_num].page_mutex);
                break;
            }
            pthread_mutex_unlock(&page_table[page_num].page_mutex);
            
            // Esperar un poco antes de verificar de nuevo
            usleep(100000); // 100ms
        }

        // Ahora esperar el comando de escritura
        int command;
        char* newContent;
        recv_command_page_num_content(client_socket, &command, &page_num, &newContent);

        if (command == 2) {
            printf("Solicitud de escritura recibida del cliente socket %d\n", client_socket);
            write_page(client_socket, page_num, newContent);
        } else {
            printf("Error: se esperaba comando de escritura (2), recibido %d\n", command);
        }

        if (newContent) {
            free(newContent);
        }

    } else {
        // PÁGINA DESBLOQUEADA - enviar comando "0" con contenido
        printf("Página %d disponible, enviando contenido al cliente socket %d\n", page_num, client_socket);
        
        // Calcular offset en memoria compartida
        char *page_start = (char*)shared_memory + (page_num * page_size);
        
        // Crear buffer temporal para el contenido (asegurar null termination)
        char *content = malloc(page_size + 1);
        if (!content) {
            printf("Error: no se pudo asignar memoria para contenido\n");
            pthread_mutex_unlock(&page_table[page_num].page_mutex);
            return;
        }
        
        memcpy(content, page_start, page_size);
        content[page_size] = '\0';  // Asegurar null termination
        
        // Enviar comando "0" con el contenido de la página
        int send_result = send_command(client_socket, 0, content);
        if (send_result != 0) {
            printf("Error: no se pudo enviar contenido al cliente socket %d (posiblemente desconectado)\n", client_socket);
            free(content);
            pthread_mutex_unlock(&page_table[page_num].page_mutex);
            return;
        }
        
        page_table[page_num].lock = 1;
        // Agregar cliente como primer y único elemento en la cola
        add_to_queue(&page_table[page_num].queue, client_socket);
        
        printf("Contenido de página %d enviado a cliente socket %d (%d bytes)\n", page_num, client_socket, page_size);
        
        free(content);
        pthread_mutex_unlock(&page_table[page_num].page_mutex);
        print_page_table_state("contenido enviado para lectura bloqueante");

        //espero nuevo contenido del socket para escribir
        int command, page_num;
        char* newContent;
        recv_command_page_num_content(client_socket, &command, &page_num, &newContent);

        if (command == 2) {
            printf("Solicitud de escritura recibida del cliente socket\n");
            write_page(client_socket, page_num, newContent);
        }

    }
}

void write_page(int client_socket, int page_num, const char* content) {
    // Validar número de página
    if (page_num < 0 || page_num >= num_pages) {
        printf("Error: número de página inválido %d\n", page_num);
        char error_msg[] = "ERROR: Página inválida";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // Validar contenido
    if (content == NULL) {
        printf("Error: contenido es NULL\n");
        char error_msg[] = "ERROR: Contenido inválido";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    printf("Cliente socket %d intenta escribir en página %d\n", client_socket, page_num);

    // Bloquear mutex de la página para acceso atomico
    pthread_mutex_lock(&page_table[page_num].page_mutex);
    
    // Verificar si el cliente está en el tope de la cola
    if (page_table[page_num].queue == NULL || page_table[page_num].queue->client_socket != client_socket) {
        printf("Error: Cliente socket %d no está en el tope de la cola para página %d\n", client_socket, page_num);
        pthread_mutex_unlock(&page_table[page_num].page_mutex);
        char error_msg[] = "ERROR: No autorizado para escribir";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    printf("Cliente socket %d autorizado para escribir en página %d\n", client_socket, page_num);

    // Escribir contenido en la página
    char *page_start = (char*)shared_memory + (page_num * page_size);
    memset(page_start, 0, page_size);  // Limpiar página
    
    int content_len = strlen(content);
    int bytes_to_write = content_len < page_size ? content_len : page_size;
    memcpy(page_start, content, bytes_to_write);
    
    printf("Página %d actualizada con %d bytes por cliente socket %d\n", page_num, bytes_to_write, client_socket);

    // Enviar comando "2" para confirmar que la escritura fue exitosa
    if (send_command(client_socket, 2, "") != 0) {
        printf("Error: no se pudo enviar confirmación al cliente socket %d\n", client_socket);
    } else {
        printf("Confirmación de escritura enviada al cliente socket %d\n", client_socket);
    }

    // Remover cliente actual de la cola
    int removed_client = remove_from_queue(&page_table[page_num].queue);
    if (removed_client != client_socket) {
        printf("WARNING: Cliente removido (%d) no coincide con cliente actual (%d)\n", removed_client, client_socket);
    }

    // Verificar si hay más clientes en la cola
    if (page_table[page_num].queue == NULL) {
        // Cola vacía - desbloquear página
        page_table[page_num].lock = 0;
        printf("Cola vacía para página %d, página desbloqueada\n", page_num);
        pthread_mutex_unlock(&page_table[page_num].page_mutex);
        print_page_table_state("página desbloqueada - cola vacía");
    } else {
        // Hay más clientes en cola - ellos mismos manejarán la notificación
        printf("Hay %s cliente(s) esperando en cola para página %d\n", 
               page_table[page_num].queue->next ? "más" : "otro", page_num);
        pthread_mutex_unlock(&page_table[page_num].page_mutex);
        print_page_table_state("escritura completada - otros clientes esperando");
    }
}

void print_page_table_state(const char *operation_context) {
    printf("\n=== ESTADO DE TABLA DE PÁGINAS (%s) ===\n", operation_context);
    printf("%-6s %-6s %-20s\n", "Página", "Lock", "Cola de espera");
    printf("--------------------------------------\n");
    
    for (int i = 0; i < num_pages; i++) {
        printf("%-6d %-6d ", i, page_table[i].lock);
        
        // Mostrar cola de espera
        process_node_t *current = page_table[i].queue;
        if (current == NULL) {
            printf("vacía");
        } else {
            printf("sockets: ");
            while (current != NULL) {
                printf("%d", current->client_socket);
                current = current->next;
                if (current != NULL) printf(", ");
            }
        }
        printf("\n");
    }
    printf("=======================================\n\n");
}




// Funciones auxiliares para manejo de cola
void add_to_queue(process_node_t **queue, int client_socket) {
    process_node_t *new_node = malloc(sizeof(process_node_t));
    if (!new_node) {
        perror("Error al asignar memoria para nodo de cola");
        return;
    }
    
    new_node->client_socket = client_socket;
    new_node->client_port = 0;  // No necesitamos el puerto para esta implementación
    new_node->next = NULL;
    
    // Agregar al final de la cola
    if (*queue == NULL) {
        *queue = new_node;
    } else {
        process_node_t *current = *queue;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
    
    printf("Cliente agregado a la cola de espera\n");
}

int remove_from_queue(process_node_t **queue) {
    if (*queue == NULL) {
        return -1;  // Cola vacía
    }
    
    process_node_t *first = *queue;
    int client_socket = first->client_socket;
    *queue = first->next;
    free(first);
    
    printf("Cliente removido de la cola de espera\n");
    return client_socket;
}