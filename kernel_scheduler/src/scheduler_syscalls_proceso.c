#include "scheduler_syscalls_proceso.h"

void procesar_syscall_exit(int cliente_fd, t_scheduler_context *ctx) {
  t_list *paquete = recibir_paquete(cliente_fd);
  if (ctx->estratega->proceso_exec != NULL) {
    // Log obligatorio del TP
    log_info(ctx->logger, "## (%d) - Solicitó syscall: EXIT",
             ctx->estratega->proceso_exec->pid);

    log_info(ctx->logger, "## (%d) finalizó su ejecución con motivo de SUCCESS",
             ctx->estratega->proceso_exec->pid);
    ctx->estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;

    t_paquete *paq_liberar = crear_paquete();
    paq_liberar->codigo_operacion = LIBERAR_PROCESO;
    agregar_a_paquete(paq_liberar, &(ctx->estratega->proceso_exec->pid),
                      sizeof(uint32_t));

    pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
    enviar_paquete(paq_liberar, ctx->socket_memoria);
    pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);

    eliminar_paquete(paq_liberar);

    planificador_liberar_exec(ctx->estratega,
                              ctx->estratega->proceso_exec->pid);
    planificador_admitir_a_ready(ctx->estratega);
  }
  despachar_proceso_a_cpu(ctx, cliente_fd);
  list_destroy_and_destroy_elements(paquete, free);
}
 
void procesar_syscall_init_proc(int cliente_fd, t_scheduler_context *ctx) {
  t_list *paquete = recibir_paquete(cliente_fd);
  uint32_t *pid_ptr = list_get(paquete, 0);
  uint32_t pid = *pid_ptr;
  char *script_path = list_get(paquete, 1);
  uint32_t *prioridad_ptr = list_get(paquete, 2);
  uint32_t prioridad = *prioridad_ptr;

  // Log obligatorio del TP
  log_info(ctx->logger, "## (%d) - Solicitó syscall: INIT_PROC", pid);

  // Guard Clause
  if (ctx->estratega->proceso_exec == NULL ||
      ctx->estratega->proceso_exec->pid != pid) {
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
    return;
  }

  static int contador_pid = 1;
  int nuevo_pid = contador_pid++;

  log_info(ctx->logger,
           "## Proceso %d solicita crear nuevo proceso con script %s", pid,
           script_path);

  pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
  enviar_peticion_crear_proceso(nuevo_pid, script_path, ctx->socket_memoria);

  int resp_mem = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
  if (resp_mem == MENSAJE) {
    int s;
    char *msj = recibir_buffer(&s, ctx->socket_memoria);
    free(msj);
  }
  pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);

  t_proceso_planificable *pcb_nuevo = pcb_crear(nuevo_pid, prioridad);

  planificador_encolar_new(ctx->estratega, pcb_nuevo);
  planificador_admitir_a_ready(ctx->estratega);

  devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC

  list_destroy_and_destroy_elements(paquete, free);
}