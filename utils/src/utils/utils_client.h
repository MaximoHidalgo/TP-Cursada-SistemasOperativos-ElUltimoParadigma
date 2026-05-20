#ifndef UTILS_CLIENTE_H_
#define UTILS_CLIENTE_H_

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
#include<readline/readline.h>

typedef enum{
	MENSAJE,
	CREAR_PROCESO,
	OBTENER_INSTRUCCION,
	PAQUETE,
	LEER_MEMORIA,
	ESCRIBIR_MEMORIA,
	LECTURA,
	ESCRITURA
}op_code;


typedef struct{
	int size;
	void* stream;
} t_buffer;

typedef struct{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

void* serializar_paquete(t_paquete* paquete, int bytes);
void crear_buffer(t_paquete* paquete);
void enviar_mensaje(char* mensaje, int socket_cliente);
int crear_conexion(char* ip, char* puerto);
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void validar_argumentos(int argc);
void limpieza_programa(int conexion1, int conexion2, int conexion3, t_log* logger, t_config* config);
void enviar_int(int socket, int numero); //Con kernel memory, Ver si queda en utils
void enviar_string(int socket, char* texto); //Con kernel memory, Ver si queda en utils

#endif // UTILS_CLIENTE_H_
