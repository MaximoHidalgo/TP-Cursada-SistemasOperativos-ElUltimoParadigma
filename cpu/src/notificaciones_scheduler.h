#ifndef NOTIFICACIONES_SCHEDULER_H_
#define NOTIFICACIONES_SCHEDULER_H_

#include "cpu_memoria.h"

void notificar_desalojo_a_scheduler(uint32_t pid, int motivo_desalojo, t_cpu_context* ctx);
void notificar_syscall_sleep_a_scheduler(uint32_t pid, uint32_t tiempo_sleep, t_cpu_context* ctx);
void notificar_syscall_recurso_a_scheduler(op_code codigo, uint32_t pid, char* recurso, t_cpu_context* ctx);
void notificar_syscall_memoria_a_scheduler(op_code codigo, uint32_t pid, uint32_t param1, uint32_t param2, t_cpu_context* ctx);
void notificar_syscall_init_proc_a_scheduler(uint32_t pid, char* path, uint32_t prioridad, t_cpu_context* ctx);

#endif // NOTIFICACIONES_SCHEDULER_H_