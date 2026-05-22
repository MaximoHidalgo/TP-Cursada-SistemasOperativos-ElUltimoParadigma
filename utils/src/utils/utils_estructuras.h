#ifndef UTILS_ESTRUCTURAS_H_
#define UTILS_ESTRUCTURAS_H_

#include <stdint.h>
#include <errno.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include "utils_client.h"
#include "utils_server.h"


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
    char* identificador;
    char* parametro1;//Ej: "AX", "MUTEX_1", "25000"
    char* parametro2;//Ej: "6", "BX" 
    char* parametro3;//Por las dudas
} t_instruccion;

t_contexto_ejecucion* pedir_contexto_a_memoria(int pid_recibido, int conexion_memory, t_log* logger);
void enviar_contexto_a_memoria(t_contexto_ejecucion* contexto, int conexion_memory);
char* fetch(t_contexto_ejecucion* contexto, int conexion_memory, t_log* logger);
t_instruccion* decode(char* instruccion_texto);
int execute(t_instruccion* instruccion, t_contexto_ejecucion* contexto, int socket_scheduler, int socket_memoria, t_log* logger);
void asignar_valor_a_registro(t_contexto_ejecucion* contexto, char* nombre_registro, uint32_t valor);
int chequear_interrupciones(int socket_scheduler, t_log* logger);



#endif //UTILS_ESTRUCTURAS_H_