#ifndef SWAP_UTILS_H_
#define SWAP_UTILS_H_

#include "estructuras.h"
#include "peticiones_swap.h"

t_swap_context* iniciar_contexto(int argc, char* argv[]);
void crear_archivo_swap(t_swap_context* ctx);
void conectar_kernel_memory(t_swap_context* ctx);
void atender_kernel_memory(t_swap_context* ctx);
void limpiar_contexto(t_swap_context* ctx);

#endif // SWAP_UTILS_H_
