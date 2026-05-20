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

void* recibir_buffer(int*, int);
int iniciar_servidor(char* puerto);
int esperar_cliente(int);
t_list* recibir_paquete(int);
void recibir_mensaje(int);
char* recibir_handshake(int socket_cliente);
int recibir_operacion(int);
int recibir_int(int socket); //Con kernel memory, Ver si queda en utils
char* recibir_string(int socket); //Con kernel memory, Ver si queda en utils

#endif //UTILS_SERVER_H_
