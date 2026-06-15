#ifndef ESTRUCTURAS_H_
#define ESTRUCTURAS_H_

#include <stdint.h>
#include <errno.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>

#include "estructuras_cpu.h"

typedef struct {
    int id;
    int socket_fd;
    uint32_t tamanio;
    uint32_t base_address;
    char* ip;
    int puerto;
} t_memory_stick_info;

typedef struct {
    char* identificador;
    char* parametro1;//Ej: "AX", "MUTEX_1", "25000"
    char* parametro2;//Ej: "6", "BX" 
    char* parametro3;//Por las dudas
} t_instruccion;

t_contexto_ejecucion* pedir_contexto_a_memoria(int pid_recibido, t_cpu_context* ctx);
void enviar_contexto_a_memoria(t_contexto_ejecucion* contexto, int conexion_memory);
char* fetch(t_contexto_ejecucion* contexto, t_cpu_context* ctx);
t_instruccion* decode(char* instruccion_texto);
int execute(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx);
void asignar_valor_a_registro(t_contexto_ejecucion* contexto, char* nombre_registro, uint32_t valor);
int chequear_interrupciones(t_cpu_context* ctx);
int mmu_traducir(t_contexto_ejecucion* contexto, int dir_logica, int tam_bytes, int tam_max_segmento, uint32_t* dir_fisica);

void* leer_memoria_distribuida_cpu(t_cpu_context* ctx, uint32_t dir_fisica, uint32_t tamanio);
void escribir_memoria_distribuida_cpu(t_cpu_context* ctx, uint32_t dir_fisica, uint32_t tamanio, void* buffer);

uint32_t leer_registro(t_contexto_ejecucion* contexto, char* nombre_registro);
int obtener_tamanio_registro(char* nombre_registro);

#endif //ESTRUCTURAS_H_