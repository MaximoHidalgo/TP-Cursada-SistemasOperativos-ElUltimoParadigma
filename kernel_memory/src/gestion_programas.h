#ifndef GESTION_PROGRAMAS_H
#define GESTION_PROGRAMAS_H

#include "estructuras_memory.h"
#include <utils/dominio/estructuras.h>

t_programa* buscar_programa(t_memory_context* ctx, int pid);
char** leer_lineas(FILE* f, int* cantidad);
void crear_proceso(t_memory_context* ctx, int pid, char* path);
void destruir_programa(t_memory_context* ctx, t_programa* prog);
char* obtener_instruccion(t_memory_context* ctx, int pid, int pc);

#endif // GESTION_PROGRAMAS_H
