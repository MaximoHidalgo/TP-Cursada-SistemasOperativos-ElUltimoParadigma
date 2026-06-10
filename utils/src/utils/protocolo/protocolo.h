#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include "../dominio/opcodes.h"

// Macros para Magic Strings y Constantes
#define HANDSHAKE_CPU "CPU"
#define HANDSHAKE_SCHEDULER "SCHEDULER"
#define HANDSHAKE_STICK "STICK"
#define MSG_OK "OK"

typedef struct {
	int size;
	void* stream;
} t_buffer;

typedef struct {
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

// Funciones para manejar paquetes
t_paquete* crear_paquete(void);
void eliminar_paquete(t_paquete* paquete);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

// Funciones genéricas de utilería
void enviar_mensaje(char* mensaje, int socket_cliente);
void enviar_int(int socket, int numero);
void enviar_string(int socket, char* texto);

// Abstracción de Stream (Clean Code: Evitar aritmética de punteros en los callers)
void buffer_write(t_buffer* buffer, void* data, int size);
void buffer_write_uint32(t_buffer* buffer, uint32_t data);
void buffer_write_int(t_buffer* buffer, int data);
void buffer_write_string(t_buffer* buffer, char* data);

// Estructura de ayuda para lectura secuencial
typedef struct {
    void* stream;
    int size;
    int offset;
} t_buffer_reader;

t_buffer_reader* buffer_reader_create(void* stream, int size);
void buffer_reader_destroy(t_buffer_reader* reader);

void buffer_read(t_buffer_reader* reader, void* dest, int size);
uint32_t buffer_read_uint32(t_buffer_reader* reader);
int buffer_read_int(t_buffer_reader* reader);
char* buffer_read_string(t_buffer_reader* reader);

#endif // PROTOCOLO_H_
