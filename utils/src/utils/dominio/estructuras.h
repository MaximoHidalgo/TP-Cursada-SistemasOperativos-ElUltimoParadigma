#ifndef DOMINIO_ESTRUCTURAS_H_
#define DOMINIO_ESTRUCTURAS_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include "../protocolo/protocolo.h"
#include "../sockets/sockets.h"

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

typedef struct {
    uint32_t pid;
    t_registros registros;
    // Más adelante acá irán las estructuras de la tabla de segmentos
} t_contexto_ejecucion;

typedef struct {
    int scheduler;
    int memoria;
    int sticks;
} t_conexiones_cpu;

typedef struct {
    uint32_t pid;
    char* path;
} t_peticion_crear_proceso;

typedef struct {
    uint32_t pid;
    uint32_t pc;
} t_peticion_fetch_instruccion;

typedef struct {
    uint32_t direccion_fisica;
    uint32_t tamanio;
} t_peticion_lectura;

typedef struct {
    uint32_t direccion_fisica;
    uint32_t tamanio;
    void* contenido;
} t_peticion_escritura;

// Funciones usadas por CPU / utilería compartida
void validar_argumentos(int argc);
void limpieza_programa(t_conexiones_cpu* conexiones, t_log* logger, t_config* config);

// Contexto
void serializar_contexto(t_paquete* paquete, t_contexto_ejecucion* contexto);
void deserializar_contexto(void* stream, t_contexto_ejecucion* contexto);
void enviar_contexto(t_contexto_ejecucion* contexto, int socket_cliente);

// Crear Proceso
void enviar_peticion_crear_proceso(uint32_t pid, char* path, int socket);
t_peticion_crear_proceso* deserializar_peticion_crear_proceso(void* buffer);
void destruir_peticion_crear_proceso(t_peticion_crear_proceso* peticion);

// Fetch
void enviar_peticion_fetch_instruccion(uint32_t pid, uint32_t pc, int socket);
t_peticion_fetch_instruccion* deserializar_peticion_fetch_instruccion(void* buffer);
void destruir_peticion_fetch_instruccion(t_peticion_fetch_instruccion* peticion);

// Lectura Memoria
void enviar_peticion_lectura(uint32_t direccion_fisica, uint32_t tamanio, int socket);
t_peticion_lectura* deserializar_peticion_lectura(void* buffer);
void destruir_peticion_lectura(t_peticion_lectura* peticion);

// Escritura Memoria
void enviar_peticion_escritura(uint32_t direccion_fisica, uint32_t tamanio, void* contenido, int socket);
t_peticion_escritura* deserializar_peticion_escritura(void* buffer);
void destruir_peticion_escritura(t_peticion_escritura* peticion);

#endif // DOMINIO_ESTRUCTURAS_H_
