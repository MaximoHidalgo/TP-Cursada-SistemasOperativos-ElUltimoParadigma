#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include "procesador_mensajes.h"

t_memory_context* iniciar_contexto(int argc, char* argv[]);
int iniciar_servidor_memoria(t_memory_context* ctx);
void limpiar_contexto(t_memory_context* ctx);

#endif //KERNEL_MEMORY_H