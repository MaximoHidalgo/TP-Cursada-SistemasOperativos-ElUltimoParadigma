#ifndef CONEXIONES_H
#define CONEXIONES_H
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>

//int PUERTO_SCHEDULER=8000;


//int iniciar_servidor(char* puerto);
//int esperar_cliente(int server_fd);
//int crear_conexion(char* ip, char* puerto);
//void enviar_mensaje(char* mensaje, int socket);
char* guardar_mensaje(int socket);

#endif