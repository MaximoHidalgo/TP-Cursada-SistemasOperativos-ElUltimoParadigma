#ifndef KERNEL_MEMORY_INIT_H
#define KERNEL_MEMORY_INIT_H

#include "atender_cliente_kernel_memory.h"
#include "gestion_programas.h"

t_memory_context* iniciar_contexto(int argc, char* argv[]);
void inicializar_entorno(t_memory_context* ctx, int argc, char* argv[]);
int iniciar_servidor_memoria(t_memory_context* ctx);
void iniciar_escucha_servidor(t_memory_context* ctx);
void limpiar_contexto(t_memory_context* ctx);

#endif // KERNEL_MEMORY_INIT_H
