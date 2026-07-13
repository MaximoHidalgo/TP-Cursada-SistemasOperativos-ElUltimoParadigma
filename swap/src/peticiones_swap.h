#ifndef PETICIONES_SWAP_H
#define PETICIONES_SWAP_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include "estructuras.h"

void procesar_peticion_lectura(t_swap_context* ctx);
void procesar_peticion_escritura(t_swap_context* ctx);
void escribir_bloque_swap(FILE* archivo,int numero_bloque,int block_size,void* buffer);
void leer_bloque_swap(FILE* archivo, int numero_bloque, int block_size, void* destino);

#endif