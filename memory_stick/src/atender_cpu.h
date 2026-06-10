#ifndef ATENDER_CPU_H_
#define ATENDER_CPU_H_

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>

typedef struct {
    void* espacio_memoria;
    uint32_t tamanio_total;
    t_log* logger;
    pthread_mutex_t mutex_memoria;
} t_memory_stick_context;

typedef struct {
    int socket_cliente;
    t_memory_stick_context* ctx;
} t_args_hilo_cpu;

void* atender_cliente_cpu(void* arg);
void procesar_mensaje_handshake(int socket_cpu, t_log* logger);
void procesar_lectura(int socket_cpu, t_memory_stick_context* ctx);
void procesar_escritura(int socket_cpu, t_memory_stick_context* ctx);

#endif // ATENDER_CPU_H_
