#ifndef ATENDER_CLIENTE_SCHEDULER_H
#define ATENDER_CLIENTE_SCHEDULER_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include "planificador.h"
#include "estructuras_scheduler.h"

typedef struct {
    int cliente_fd;
    t_scheduler_context* ctx;
} t_args_hilo_scheduler;

void despachar_proceso_a_cpu(t_scheduler_context* ctx, int socket_cpu);
void procesar_mensaje_handshake(int cliente_fd, t_scheduler_context* ctx);
void procesar_syscall_exit(int cliente_fd, t_scheduler_context* ctx);
void procesar_fin_quantum(int cliente_fd, t_scheduler_context* ctx);
void atender_cliente_scheduler(void* arg);


#endif //ATENDER_CLIENTE_SCHEDULER_H