#ifndef ATENDER_CLIENTE_SCHEDULER_H
#define ATENDER_CLIENTE_SCHEDULER_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include "planificador.h"
#include "estructuras_scheduler.h"

//cabezera temporal:
#include "atender_cliente_Y_scheduler_server.h"
//========
/* typedef struct {
    int cliente_fd;
    t_scheduler_context* ctx;
} t_args_hilo_scheduler; */


void limpiar_contexto(t_scheduler_context* ctx);
void despachar_proceso_a_cpu(t_scheduler_context* ctx, int socket_cpu);
void procesar_mensaje_handshake(int cliente_fd, t_scheduler_context* ctx);
void procesar_syscall_exit(int cliente_fd, t_scheduler_context* ctx);
void procesar_fin_quantum(int cliente_fd, t_scheduler_context* ctx);
void procesar_syscall_io(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall);
void procesar_syscall_mutex(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall);
void procesar_syscall_memoria(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall);
void procesar_syscall_init_proc(int cliente_fd, t_scheduler_context* ctx);

int atenderRespuesta(int cliente_fd, t_scheduler_context* ctx, int mensaje_cliente);
void atender_cliente_scheduler(void* arg);



#endif //ATENDER_CLIENTE_SCHEDULER_H