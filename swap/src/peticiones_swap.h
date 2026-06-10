#ifndef PETICIONES_SWAP_H
#define PETICIONES_SWAP_H

#include <utils/utils_client.h>
#include <utils/utils_server.h>

void procesar_peticion_lectura(int socket_kernel_memory, t_log* logger);
void procesar_peticion_escritura(int socket_kernel_memory, t_log* logger);

#endif