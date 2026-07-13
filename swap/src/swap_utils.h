#ifndef SWAP_UTILS_H_
#define SWAP_UTILS_H_

#include <commons/string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <stdlib.h>
#include <stdio.h>
#include <utils/sockets/sockets.h>
#include "estructuras.h"
#include "peticiones_swap.h"
#include <utils/protocolo/protocolo.h>

t_swap_context* iniciar_contexto(int argc, char* argv[]);
void crear_archivo_swap(t_swap_context* ctx);
void conectar_kernel_memory(t_swap_context* ctx);
void atender_kernel_memory(t_swap_context* ctx);
void limpiar_contexto(t_swap_context* ctx);

#endif // SWAP_UTILS_H_
