#ifndef SCHEDULER_SYSCALLS_MUTEX_H
#define SCHEDULER_SYSCALLS_MUTEX_H

#include "estructuras_scheduler.h"
#include <utils/sockets/sockets.h>
#include "planificador_recursos.h"
#include "scheduler_operaciones.h"
#include "utils_scheduler.h"

void procesar_syscall_mutex(int cliente_fd, t_scheduler_context *ctx, int tipo_syscall);
#endif