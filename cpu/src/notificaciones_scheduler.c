#include "notificaciones_scheduler.h"

// -------------------------------------------------------------
// NOTIFICACIONES AL SCHEDULER
// -------------------------------------------------------------

void notificar_desalojo_a_scheduler(uint32_t pid, int motivo_desalojo, t_cpu_context* ctx) {
    enviar_operacion_con_pid(motivo_desalojo, pid, ctx->conexiones->scheduler);
    log_info(ctx->logger, "PID %d devuelto al Scheduler. Motivo (op_code): %d", pid, motivo_desalojo);
}

void notificar_syscall_sleep_a_scheduler(uint32_t pid, uint32_t tiempo_sleep, t_cpu_context* ctx) {
    enviar_operacion_con_pid_y_parametro(SYSCALL_SLEEP, pid, tiempo_sleep, ctx->conexiones->scheduler);
}

void notificar_syscall_recurso_a_scheduler(op_code codigo, uint32_t pid, char* recurso, t_cpu_context* ctx) {
    enviar_operacion_con_pid_y_string(codigo, pid, recurso, ctx->conexiones->scheduler);
}

void notificar_syscall_memoria_a_scheduler(op_code codigo, uint32_t pid, uint32_t param1, uint32_t param2, t_cpu_context* ctx) {
    enviar_operacion_con_pid_y_dos_parametros(codigo, pid, param1, param2, ctx->conexiones->scheduler);
}

void notificar_syscall_init_proc_a_scheduler(uint32_t pid, char* path, uint32_t prioridad, t_cpu_context* ctx) {
    enviar_operacion_con_pid_string_y_parametro(SYSCALL_INIT_PROC, pid, path, prioridad, ctx->conexiones->scheduler);
}