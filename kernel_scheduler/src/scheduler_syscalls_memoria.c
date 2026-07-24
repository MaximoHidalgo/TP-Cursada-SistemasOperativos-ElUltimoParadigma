#include "scheduler_syscalls_memoria.h"

void procesar_syscall_memoria(int cliente_fd, t_scheduler_context *ctx,
                              int tipo_syscall) {
  t_list *paquete = recibir_paquete(cliente_fd);
  uint32_t *pid_ptr = list_get(paquete, 0);
  uint32_t pid = *pid_ptr;

  // Log obligatorio del TP
  log_info(ctx->logger, "## (%d) - Solicitó syscall: %s", pid,
           obtener_nombre_syscall(tipo_syscall));

  // Guard Clause: Validación del proceso actual
  if (ctx->estratega->proceso_exec == NULL ||
      ctx->estratega->proceso_exec->pid != pid) {
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
    return;
  }

  if (tipo_syscall == SYSCALL_MEM_ALLOC) {
    uint32_t *id_seg_ptr = list_get(paquete, 1);
    uint32_t *tamanio_ptr = list_get(paquete, 2);

    t_paquete *paq_mem = crear_paquete();
    paq_mem->codigo_operacion = CREAR_SEGMENTO;
    agregar_a_paquete(paq_mem, &pid, sizeof(uint32_t));
    agregar_a_paquete(paq_mem, id_seg_ptr, sizeof(uint32_t));
    agregar_a_paquete(paq_mem, tamanio_ptr, sizeof(uint32_t));

    pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
    enviar_paquete(paq_mem, ctx->socket_memoria);
    eliminar_paquete(paq_mem);

    int respuesta =
        esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);

    if (respuesta == SOLICITUD_COMPACTACION) {
      log_info(ctx->logger,
               "## Inicio de compactación"); // Log obligatorio del TP

      // Como la CPU ya desalojó el proceso y está inactiva, autorizamos
      // inmediatamente a Memoria
      int op_resp = RESPUESTA_COMPACTACION;
      send(ctx->socket_memoria, &op_resp, sizeof(int), 0);

      // Esperamos a que termine de compactar y nos devuelva el resultado real
      // (OK/ERROR)
      respuesta =
          esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
    }
    if (respuesta == MENSAJE) {
      int size;
      char *msj = recibir_buffer(&size, ctx->socket_memoria);
      if (string_equals_ignore_case(msj, "OK")) {
        pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
        devolver_ejecucion_a_cpu(ctx, cliente_fd);
      } else {
        log_info(ctx->logger,
                 "## Out of Memory para proceso %d. Reencolando en NEW para "
                 "reintento.",
                 pid);

        t_paquete *paq_pc = crear_paquete();
        paq_pc->codigo_operacion = RETROCEDER_PC_OUT_OF_MEMORY;
        agregar_a_paquete(paq_pc, &pid, sizeof(uint32_t));
        enviar_paquete(paq_pc, ctx->socket_memoria);
        eliminar_paquete(paq_pc);

        int rta =
            esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
        if (rta == MENSAJE) {
          int s;
          char *m = recibir_buffer(&s, ctx->socket_memoria);
          free(m);
        }

        // No matamos al proceso: lo devolvemos a cola_new para que el
        // planificador de mediano plazo lo re-admita cuando haya espacio
        // (tras la suspensión y swapeo de otros procesos).
        t_proceso_planificable *proceso_oom = ctx->estratega->proceso_exec;
        ctx->estratega->proceso_exec = NULL;
        proceso_oom->estado = ESTADO_PROCESO_NEW;

        pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);

        pthread_mutex_lock(&ctx->estratega->mutex_colas);
        queue_push(ctx->estratega->cola_new, proceso_oom);
        pthread_mutex_unlock(&ctx->estratega->mutex_colas);

        despachar_proceso_a_cpu(ctx, cliente_fd);
      }
      free(msj);
    } else {
      pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
    }
  } else if (tipo_syscall == SYSCALL_MEM_FREE) {
    uint32_t *id_seg_ptr = list_get(paquete, 1);

    t_paquete *paq_mem = crear_paquete();
    paq_mem->codigo_operacion = ELIMINAR_SEGMENTO;
    agregar_a_paquete(paq_mem, &pid, sizeof(uint32_t));
    agregar_a_paquete(paq_mem, id_seg_ptr, sizeof(uint32_t));

    pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
    enviar_paquete(paq_mem, ctx->socket_memoria);
    eliminar_paquete(paq_mem);

    int respuesta =
        esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
    if (respuesta == MENSAJE) {
      int size;
      char *msj = recibir_buffer(&size, ctx->socket_memoria);
      if (string_equals_ignore_case(msj, "OK")) {
        pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
        devolver_ejecucion_a_cpu(ctx, cliente_fd);
      } else {
        log_error(
            ctx->logger,
            "Error de memoria al intentar liberar segmento %d para PID %d: %s",
            *id_seg_ptr, pid, msj);
        pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
        devolver_ejecucion_a_cpu(ctx, cliente_fd);
      }
      free(msj);
    } else {
      pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
      log_error(
          ctx->logger,
          "Error desconocido al intentar liberar el segmento %d del PID %d.",
          *id_seg_ptr, pid);
      devolver_ejecucion_a_cpu(ctx, cliente_fd);
    }
  }

  list_destroy_and_destroy_elements(paquete, free);
}