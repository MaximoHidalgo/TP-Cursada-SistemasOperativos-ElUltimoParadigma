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

#include <stdint.h>
typedef enum{
	MENSAJE,
	PAQUETE,
	LECTURA,
	ESCRITURA,
	FETCH,
	CONTEXTO,
	INTERRUPCION,
	IO_SLEEP,
	IO_STDIN,
	IO_STDOUT,
	MUTEX_LOCK,
	MUTEX_UNLOCK,
	FIN_QUANTUM,
	PROCESO_EXIT
}op_code;

typedef struct{
	int size;
	void* stream;
} t_buffer;

typedef struct{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

typedef struct {
    uint32_t PC;
    uint8_t AX;
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI;
    uint32_t DI;
} t_registros;

typedef struct {
    uint32_t pid;
    t_registros registros;
} t_contexto_ejecucion;

typedef struct {
    uint32_t pid;
    uint32_t pc;
} t_fetch;

typedef struct {
    uint32_t pid;
} t_interrupcion;

typedef struct {
    uint32_t pid;
    op_code syscall;
} t_syscall;

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
void serializar_contexto(t_paquete* paquete, t_contexto_ejecucion* contexto);
void deserializar_contexto(void* stream, t_contexto_ejecucion* contexto);
void enviar_contexto(t_contexto_ejecucion* contexto, int socket_cliente);
void* recibir_buffer(int* size, int socket_cliente);

#endif // UTILS_CLIENTE_H_
