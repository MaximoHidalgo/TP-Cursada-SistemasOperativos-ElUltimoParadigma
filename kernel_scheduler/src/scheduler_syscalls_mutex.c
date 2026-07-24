#include "scheduler_syscalls_mutex.h"

void procesar_syscall_mutex(int cliente_fd, t_scheduler_context *ctx,
                            int tipo_syscall) {
  t_list *paquete = recibir_paquete(cliente_fd);
  uint32_t *pid_ptr = list_get(paquete, 0);
  uint32_t pid = *pid_ptr;
  char *recurso = list_get(paquete, 1);

  // Log obligatorio del TP
  log_info(ctx->logger, "## (%d) - Solicitó syscall: %s", pid,
           obtener_nombre_syscall(tipo_syscall));

  // Guard Clause
  if (ctx->estratega->proceso_exec == NULL ||
      ctx->estratega->proceso_exec->pid != pid) {
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
    return;
  }

  switch (tipo_syscall) {
  case SYSCALL_MUTEX_CREATE:
    planificador_mutex_create(ctx->estratega, ctx->estratega->proceso_exec,
                              recurso);
    devolver_ejecucion_a_cpu(ctx, cliente_fd);
    break;

  case SYSCALL_MUTEX_LOCK: {
    int se_bloqueo = planificador_mutex_lock(
        ctx->estratega, ctx->estratega->proceso_exec, recurso);
    if (se_bloqueo == 0) {
      devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC
    } else {
      despachar_proceso_a_cpu(ctx, cliente_fd); // Se bloqueó
    }
    break;
  }

  case SYSCALL_MUTEX_UNLOCK: {
    int debe_desalojar = planificador_mutex_unlock(
        ctx->estratega, ctx->estratega->proceso_exec, recurso);
    if (debe_desalojar) {
      encolar_en_cola_ready(ctx->estratega->colas,
                            ctx->estratega->proceso_exec);
      ctx->estratega->proceso_exec = NULL;
      despachar_proceso_a_cpu(ctx, cliente_fd);
    } else {
      devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC
    }
    break;
  }
  }

  list_destroy_and_destroy_elements(paquete, free);
}