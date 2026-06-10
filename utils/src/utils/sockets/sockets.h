#ifndef SOCKETS_H_
#define SOCKETS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/collections/list.h>

// --- CLIENTE ---
int crear_conexion(char *ip, char* puerto);
void liberar_conexion(int socket_cliente);

// --- SERVIDOR ---
int iniciar_servidor(char* puerto, t_log* logger);
int esperar_cliente(int socket_servidor, t_log* logger);

// --- RECEPCION GENÉRICA ---
int recibir_operacion(int socket_cliente);
void* recibir_buffer(int* size, int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log* logger);
char* recibir_handshake(int socket_cliente);
t_list* recibir_paquete(int socket_cliente);
int recibir_int(int socket);
char* recibir_string(int socket);

#endif // SOCKETS_H_
