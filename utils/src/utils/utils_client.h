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
	ESCRITURA,
	PID_A_EJECUTAR,
	FETCH_INSTRUCCION,
	ENVIO_INSTRUCCION,
	SOLICITUD_CONTEXTO,
	ENVIO_CONTEXTO,
	ACTUALIZACION_CONTEXTO,
	INTERRUPCION_QUANTUM,
	FIN_DE_QUANTUM,
	SYSCALL_EXIT,
	SYSCALL_SLEEP,
	SYSCALL_STDOUT,
	SYSCALL_STDIN,
	ERROR_SEG_FAULT,
	CONSULTAR_ESPACIO_LIBRE,
	PETICION_IO
}op_code;

typedef struct { // Los uint8_t tienen 1 byte y los uint32_t 4 bytes
    uint32_t PC;  // Program Counter 
    uint8_t AX;
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    uint32_t SI;  // Dirección lógica de memoria de origen 
    uint32_t DI;  // Dirección lógica de memoria de destino 
} t_registros;

typedef struct{
	int size;
	void* stream;
} t_buffer;

typedef struct{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

typedef struct {
    uint32_t pid;
    t_registros registros;
    // Más adelante acá irán las estructuras de la tabla de segmentos
} t_contexto_ejecucion;


void* serializar_paquete(t_paquete* paquete, int bytes);
int crear_conexion(char* ip, char* puerto);
void crear_buffer(t_paquete* paquete);
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void liberar_conexion(int socket_cliente);
void enviar_mensaje(char* mensaje, int socket_cliente);

//FUNCIONES USADAS POR LA CPU, REVISAR PARA APROVECHAR EN LAS DEMAS
void validar_argumentos(int argc);
void limpieza_programa(int conexion1, int conexion2, int conexion3, t_log* logger, t_config* config);
void enviar_int(int socket, int numero); //Con kernel memory, Ver si queda en utils
void enviar_string(int socket, char* texto); //Con kernel memory, Ver si queda en utils
void serializar_contexto(t_paquete* paquete, t_contexto_ejecucion* contexto);
void deserializar_contexto(void* stream, t_contexto_ejecucion* contexto);
void enviar_contexto(t_contexto_ejecucion* contexto, int socket_cliente);


#endif // UTILS_CLIENTE_H_
