#ifndef SCHEDULER_SYSCALLS_H
#define SCHEDULER_SYSCALLS_H

#include "estructuras_scheduler.h"
#include "planificador.h"
#include "scheduler_syscalls_io.h"
#include "scheduler_syscalls_memoria.h"
#include "scheduler_syscalls_mutex.h"
#include "scheduler_syscalls_proceso.h"

//void despachar_proceso_a_cpu(t_scheduler_context* ctx, int socket_cpu);
//void procesar_mensaje_handshake(int cliente_fd, t_scheduler_context* ctx);
//void procesar_syscall_exit(int cliente_fd, t_scheduler_context* ctx);
//void procesar_fin_quantum(int cliente_fd, t_scheduler_context* ctx);

//void procesar_syscall_mutex(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall);
//void procesar_syscall_memoria(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall);
//void procesar_syscall_init_proc(int cliente_fd, t_scheduler_context* ctx);

#endif // SCHEDULER_SYSCALLS_H