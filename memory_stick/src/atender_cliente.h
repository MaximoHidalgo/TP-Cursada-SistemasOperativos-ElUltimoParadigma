#ifndef ATENDER_CLIENTE_H_
#define ATENDER_CLIENTE_H_

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <commons/log.h>
#include <commons/config.h>
#include "estructuras.h"

typedef struct {
    int socket_cliente;
    t_memory_stick_context* ctx;
} t_args_hilo_cliente;

void* atender_cliente(void* arg);
void procesar_mensaje_handshake(int socket_cliente, t_log* logger);
void procesar_lectura(int socket_cliente, t_memory_stick_context* ctx);
void procesar_escritura(int socket_cliente, t_memory_stick_context* ctx);

#endif // ATENDER_CLIENTE_H_
