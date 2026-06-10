#ifndef ATENDER_CLIENTE_SCHEDULER_H
#define ATENDER_CLIENTE_SCHEDULER_H

#include <utils/utils_server.h>
#include <utils/utils_client.h>
#include "planificador.h"

typedef struct {
    int cliente_fd;
    t_estratega_planificacion* estratega;
} t_args_hilo_scheduler;

void despachar_proceso_a_cpu(t_estratega_planificacion* estratega, int socket_cpu);
void procesar_mensaje_handshake(int cliente_fd, t_estratega_planificacion* estratega);
void procesar_syscall_exit(int cliente_fd, t_estratega_planificacion* estratega);
void procesar_fin_quantum(int cliente_fd, t_estratega_planificacion* estratega);
void atender_cliente_scheduler(void* arg);


#endif //ATENDER_CLIENTE_SCHEDULER_H