#ifndef UTILS_SERVER_H_
#define UTILS_SERVER_H_

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/collections/list.h>
#include<assert.h>

extern t_log* logger;

int iniciar_servidor(char* puerto);
int esperar_cliente(int);
int recibir_operacion(int);
void* recibir_buffer(int*, int);
void recibir_mensaje(int);
char* recibir_handshake(int socket_cliente);
t_list* recibir_paquete(int);
int recibir_int(int socket); //Con kernel memory, Ver si queda en utils
char* recibir_string(int socket); //Con kernel memory, Ver si queda en utils

#endif //UTILS_SERVER_H_
