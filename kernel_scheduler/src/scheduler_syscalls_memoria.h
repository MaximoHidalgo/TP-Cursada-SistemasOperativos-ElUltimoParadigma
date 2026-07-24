#ifndef SCHEDULER_SYSCALLS_MEMORIA_H
#define SCHEDULER_SYSCALLS_MEMORIA_H

#include "estructuras_scheduler.h"
#include <utils/sockets/sockets.h>
#include "scheduler_operaciones.h"
#include "utils_scheduler.h"
#include "planificador.h"

void procesar_syscall_memoria(int cliente_fd, t_scheduler_context *ctx,
                              int tipo_syscall);
#endif



