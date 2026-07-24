#ifndef SCHEDULER_SYSCALLS_PROCESO_H
#define SCHEDULER_SYSCALLS_PROCESO_H



#include "estructuras_scheduler.h"
#include <utils/sockets/sockets.h>
#include "scheduler_operaciones.h"
#include "planificador_memoria.h"

void procesar_syscall_exit(int cliente_fd, t_scheduler_context *ctx);
void procesar_syscall_init_proc(int cliente_fd, t_scheduler_context *ctx);
#endif