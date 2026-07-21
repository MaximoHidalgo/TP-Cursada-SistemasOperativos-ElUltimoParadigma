#ifndef SCHEDULER_SYSCALLS_IO_H
#define SCHEDULER_SYSCALLS_IO_H

#include <commons/collections/list.h>
#include "atender_cliente_scheduler.h"
#include "planificador.h"
#include "utils_scheduler.h"
#include "utils/dominio/opcodes.h"
#include "estructuras_scheduler.h"
#include "utils/protocolo/protocolo.h"
#include <utils/sockets/sockets.h>

void manejar_io_sleep(t_scheduler_context* ctx, t_list* paquete, uint32_t pid);
void manejar_io_stdout(t_scheduler_context* ctx, t_list* paquete, uint32_t pid);
void manejar_io_stdin(t_scheduler_context* ctx, t_list* paquete, uint32_t pid);
void procesar_syscall_io(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall);

#endif