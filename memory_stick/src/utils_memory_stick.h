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


int iniciar_servidor(char* puerto,t_log* logger);
int esperar_cliente(int socket_servidor,t_log* logger);
//void iniciar_config_memory_stick(t_config* config,char* puerto_escucha,char* ip_kernel_memory,char* puerto_kernel_memory);
int crear_conexion(char* ip, char* puerto,t_log* logger);
int recibir_operacion(int socket_cliente);
void* recibir_buffer(int* size, int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log*logger);
void* serializar_paquete(t_paquete* paquete, int bytes);





#endif