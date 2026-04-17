#ifndef SRC_MEMORY_STICK_H_
#define SRC_MEMORY_STICK_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include<commons/config.h>

extern t_log* logger;
extern t_config* config;
extern char* puerto_escucha;

int iniciar_servidor(char* puerto);
int esperar_cliente(int socket_servidor);
void iniciar_config_memory_stick();

#endif