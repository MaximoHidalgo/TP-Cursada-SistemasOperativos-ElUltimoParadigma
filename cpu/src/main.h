#ifndef MAIN_H_
#define MAIN_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

typedef enum
{
	MENSAJE,
	PAQUETE
}op_code;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

void validar_argumentos(int argc);
int crear_conexion(char *ip, char* puerto);
void limpieza_programa(int conexion1, int conexion2, int conexion3, t_log* logger, t_config* config);
void liberar_conexion(int socket_cliente);
void enviar_mensaje(char* mensaje, int socket_cliente);
void* serializar_paquete(t_paquete* paquete, int bytes);
void eliminar_paquete(t_paquete* paquete);


#endif /* MAIN_H_ */
