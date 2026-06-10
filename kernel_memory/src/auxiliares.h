#ifndef AUXILIARES_H
#define AUXILIARES_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include <pthread.h>
#include <string.h>

#include <commons/string.h>
#include <commons/log.h>
#include "estructuras_memory.h"

typedef struct {
    int pid;
    char** instrucciones;
    int cantidad_instrucciones;
    t_contexto_ejecucion* contexto; 
} t_programa;

t_programa* buscar_programa(t_memory_context* ctx, int pid);
char** leer_lineas(FILE* f, int* cantidad);
void crear_proceso(t_memory_context* ctx, int pid, char* path);
char* obtener_instruccion(t_memory_context* ctx, int pid, int pc);

#endif // AUXILIARES_H