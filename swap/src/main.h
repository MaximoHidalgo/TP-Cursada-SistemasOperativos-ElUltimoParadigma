#ifndef SWAP_H
#define SWAP_H

#include <commons/string.h>
#include "estructuras.h"
#include "peticiones_swap.h"

// Inicialización de módulos
t_swap_context* iniciar_contexto(int argc, char* argv[]);
void crear_archivo_swap(t_swap_context* ctx);
void conectar_kernel_memory(t_swap_context* ctx);
void procesar_peticiones_memoria(t_swap_context* ctx);
void limpiar_contexto(t_swap_context* ctx);

#endif //SWAP_H