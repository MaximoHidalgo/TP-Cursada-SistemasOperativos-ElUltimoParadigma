#ifndef PETICIONES_SWAP_H
#define PETICIONES_SWAP_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>

void procesar_peticion_lectura(int socket_kernel_memory, t_log* logger);
void procesar_peticion_escritura(int socket_kernel_memory, t_log* logger);

#endif