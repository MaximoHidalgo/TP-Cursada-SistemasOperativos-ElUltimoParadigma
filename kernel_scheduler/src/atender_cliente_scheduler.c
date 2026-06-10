#include "atender_cliente_scheduler.h"

void despachar_proceso_a_cpu(t_scheduler_context* ctx, int socket_cpu) {
    t_proceso_planificable* proceso_a_ejecutar = planificador_despachar_siguiente(ctx->estratega);
    
    if (proceso_a_ejecutar != NULL) {
        t_paquete* paquete_pid = crear_paquete();
        paquete_pid->codigo_operacion = PID_A_EJECUTAR;
        
        agregar_a_paquete(paquete_pid, &(proceso_a_ejecutar->pid), sizeof(int));
        enviar_paquete(paquete_pid, socket_cpu);
        eliminar_paquete(paquete_pid);
        
        log_info(ctx->logger, "Se envió el PID %d a la CPU para ejecutar", proceso_a_ejecutar->pid);
    } else {
        log_info(ctx->logger, "No hay procesos en la cola READY para despachar a la CPU.");
    }
}

static void devolver_ejecucion_a_cpu(t_scheduler_context* ctx, int cliente_fd) {
    t_paquete* paquete_pid = crear_paquete();
    paquete_pid->codigo_operacion = PID_A_EJECUTAR;
    agregar_a_paquete(paquete_pid, &(ctx->estratega->proceso_exec->pid), sizeof(int));
    enviar_paquete(paquete_pid, cliente_fd);
    eliminar_paquete(paquete_pid);
}

void procesar_mensaje_handshake(int cliente_fd, t_scheduler_context* ctx) {
    char* mensaje = recibir_handshake(cliente_fd);
    if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        log_info(ctx->logger, "## CPU %s Conectada", mensaje);
        ctx->estratega->socket_cpu = cliente_fd;
        despachar_proceso_a_cpu(ctx, cliente_fd);
    } else if (string_equals_ignore_case(mensaje, "OK")) {
        planificador_io_terminada(ctx->estratega, cliente_fd);
        if (ctx->estratega->proceso_exec == NULL && ctx->estratega->socket_cpu != -1) {
            despachar_proceso_a_cpu(ctx, ctx->estratega->socket_cpu);
        }
    } else {
        planificador_registrar_io(ctx->estratega, mensaje, cliente_fd);
    }
    free(mensaje);
}

static void manejar_io_sleep(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
    uint32_t* tiempo_ptr = list_get(paquete, 1);
    t_interfaz_io* interfaz = buscar_interfaz_por_nombre(ctx->estratega, "SLEEP");
    if (interfaz != NULL) {
        int op = PETICION_IO;
        send(interfaz->socket_fd, &op, sizeof(int), 0);
        send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
        send(interfaz->socket_fd, tiempo_ptr, sizeof(uint32_t), 0);
    }
    planificador_atender_peticion_io(ctx->estratega, ctx->estratega->proceso_exec, "SLEEP");
}

static void manejar_io_stdout(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
    char* dummy = list_get(paquete, 1);
    t_interfaz_io* interfaz = buscar_interfaz_por_nombre(ctx->estratega, "STDOUT");
    if (interfaz != NULL) {
        int op = PETICION_IO;
        send(interfaz->socket_fd, &op, sizeof(int), 0);
        send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
        
        int len = strlen(dummy) + 1;
        send(interfaz->socket_fd, &len, sizeof(int), 0);
        send(interfaz->socket_fd, dummy, len, 0);
    }
    planificador_atender_peticion_io(ctx->estratega, ctx->estratega->proceso_exec, "STDOUT");
}

static void manejar_io_stdin(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
    uint32_t* size_ptr = list_get(paquete, 1);
    t_interfaz_io* interfaz = buscar_interfaz_por_nombre(ctx->estratega, "STDIN");
    if (interfaz != NULL) {
        int op = PETICION_IO;
        send(interfaz->socket_fd, &op, sizeof(int), 0);
        send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
        send(interfaz->socket_fd, size_ptr, sizeof(uint32_t), 0);
    }
    planificador_atender_peticion_io(ctx->estratega, ctx->estratega->proceso_exec, "STDIN");
}

void procesar_syscall_io(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    
    // Guard Clause: Validación del proceso actual
    if (ctx->estratega->proceso_exec == NULL || ctx->estratega->proceso_exec->pid != pid) {
        despachar_proceso_a_cpu(ctx, cliente_fd);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    switch (tipo_syscall) {
        case SYSCALL_SLEEP:
            manejar_io_sleep(ctx, paquete, pid);
            break;
        case SYSCALL_STDOUT:
            manejar_io_stdout(ctx, paquete, pid);
            break;
        case SYSCALL_STDIN:
            manejar_io_stdin(ctx, paquete, pid);
            break;
    }
    
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_exit(int cliente_fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    if (ctx->estratega->proceso_exec != NULL) {
        log_info(ctx->logger, "## Finaliza el proceso %d - Motivo: SUCCESS", ctx->estratega->proceso_exec->pid);
        ctx->estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
        
        t_paquete* paq_liberar = crear_paquete();
        paq_liberar->codigo_operacion = LIBERAR_PROCESO;
        agregar_a_paquete(paq_liberar, &(ctx->estratega->proceso_exec->pid), sizeof(uint32_t));
        enviar_paquete(paq_liberar, ctx->socket_memoria);
        eliminar_paquete(paq_liberar);
        
        planificador_liberar_exec(ctx->estratega, ctx->estratega->proceso_exec->pid);
    }
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_mutex(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    char* recurso = list_get(paquete, 1);

    // Guard Clause
    if (ctx->estratega->proceso_exec == NULL || ctx->estratega->proceso_exec->pid != pid) {
        despachar_proceso_a_cpu(ctx, cliente_fd);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    switch (tipo_syscall) {
        case SYSCALL_MUTEX_CREATE:
            planificador_mutex_create(ctx->estratega, ctx->estratega->proceso_exec, recurso);
            devolver_ejecucion_a_cpu(ctx, cliente_fd);
            break;

        case SYSCALL_MUTEX_LOCK: {
            int se_bloqueo = planificador_mutex_lock(ctx->estratega, ctx->estratega->proceso_exec, recurso);
            if (se_bloqueo == 0) {
                devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC
            } else {
                despachar_proceso_a_cpu(ctx, cliente_fd); // Se bloqueó
            }
            break;
        }

        case SYSCALL_MUTEX_UNLOCK:
            planificador_mutex_unlock(ctx->estratega, ctx->estratega->proceso_exec, recurso);
            devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC
            break;
    }
    
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_memoria(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;

    // Guard Clause: Validación del proceso actual
    if (ctx->estratega->proceso_exec == NULL || ctx->estratega->proceso_exec->pid != pid) {
        despachar_proceso_a_cpu(ctx, cliente_fd);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    if (tipo_syscall == SYSCALL_MEM_ALLOC) {
        uint32_t* id_seg_ptr = list_get(paquete, 1);
        uint32_t* tamanio_ptr = list_get(paquete, 2);
        
        t_paquete* paq_mem = crear_paquete();
        paq_mem->codigo_operacion = CREAR_SEGMENTO;
        agregar_a_paquete(paq_mem, &pid, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, id_seg_ptr, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, tamanio_ptr, sizeof(uint32_t));
        enviar_paquete(paq_mem, ctx->socket_memoria);
        eliminar_paquete(paq_mem);

        int respuesta = recibir_operacion(ctx->socket_memoria);
        
        if (respuesta == SOLICITUD_COMPACTACION) {
            log_info(ctx->logger, "Compactación requerida. Avisando a Memoria que puede proceder...");
            t_paquete* paq_resp = crear_paquete();
            paq_resp->codigo_operacion = RESPUESTA_COMPACTACION;
            enviar_paquete(paq_resp, ctx->socket_memoria);
            eliminar_paquete(paq_resp);
            
            // Esperamos la respuesta final de la creación del segmento post-compactación
            respuesta = recibir_operacion(ctx->socket_memoria);
        }

        if (respuesta == MENSAJE) {
            int size;
            char* msj = recibir_buffer(&size, ctx->socket_memoria);
            if (string_equals_ignore_case(msj, "OK")) {
                devolver_ejecucion_a_cpu(ctx, cliente_fd);
            } else {
                log_error(ctx->logger, "Out of memory. Finalizando proceso %d", pid);
                ctx->estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
                
                t_paquete* paq_liberar = crear_paquete();
                paq_liberar->codigo_operacion = LIBERAR_PROCESO;
                agregar_a_paquete(paq_liberar, &pid, sizeof(uint32_t));
                enviar_paquete(paq_liberar, ctx->socket_memoria);
                eliminar_paquete(paq_liberar);

                planificador_liberar_exec(ctx->estratega, pid);
                despachar_proceso_a_cpu(ctx, cliente_fd);
            }
            free(msj);
        }
    } else if (tipo_syscall == SYSCALL_MEM_FREE) {
        uint32_t* id_seg_ptr = list_get(paquete, 1);

        t_paquete* paq_mem = crear_paquete();
        paq_mem->codigo_operacion = ELIMINAR_SEGMENTO;
        agregar_a_paquete(paq_mem, &pid, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, id_seg_ptr, sizeof(uint32_t));
        enviar_paquete(paq_mem, ctx->socket_memoria);
        eliminar_paquete(paq_mem);

        int respuesta = recibir_operacion(ctx->socket_memoria);
        if (respuesta == MENSAJE) {
            int size;
            char* msj = recibir_buffer(&size, ctx->socket_memoria);
            if (string_equals_ignore_case(msj, "OK")) {
                devolver_ejecucion_a_cpu(ctx, cliente_fd);
            }
            free(msj);
        }
    }
    
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_fin_quantum(int cliente_fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    if (ctx->estratega->proceso_exec != NULL) {
        log_info(ctx->logger, "## Fin de Quantum para proceso %d - Desalojado", ctx->estratega->proceso_exec->pid);
        
        planificador_reencolar_por_quantum(ctx->estratega, ctx->estratega->proceso_exec);
        planificador_liberar_exec(ctx->estratega, ctx->estratega->proceso_exec->pid);
    }
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void atender_cliente_scheduler(void* arg) {
    t_args_hilo_scheduler* args = (t_args_hilo_scheduler*)arg;
    int cliente_fd = args->cliente_fd;
    t_scheduler_context* ctx = args->ctx;
    free(args);

    while (1) {
        int mensaje_cliente = recibir_operacion(cliente_fd);
        
        switch(mensaje_cliente) {
            case MENSAJE:
                procesar_mensaje_handshake(cliente_fd, ctx);
                break;
            case SYSCALL_EXIT:
                procesar_syscall_exit(cliente_fd, ctx);
                break;
            case SYSCALL_SLEEP:
            case SYSCALL_STDOUT:
            case SYSCALL_STDIN:
                procesar_syscall_io(cliente_fd, ctx, mensaje_cliente);
                break;
            case SYSCALL_MUTEX_CREATE:
            case SYSCALL_MUTEX_LOCK:
            case SYSCALL_MUTEX_UNLOCK:
                procesar_syscall_mutex(cliente_fd, ctx, mensaje_cliente);
                break;
            case SYSCALL_SEG_FAULT:
            {
                t_list* paquete = recibir_paquete(cliente_fd);
                if (ctx->estratega->proceso_exec != NULL) {
                    log_info(ctx->logger, "## Finaliza el proceso %d - Motivo: SEG_FAULT", ctx->estratega->proceso_exec->pid);
                    ctx->estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
                    
                    t_paquete* paq_liberar = crear_paquete();
                    paq_liberar->codigo_operacion = LIBERAR_PROCESO;
                    agregar_a_paquete(paq_liberar, &(ctx->estratega->proceso_exec->pid), sizeof(uint32_t));
                    enviar_paquete(paq_liberar, ctx->socket_memoria);
                    eliminar_paquete(paq_liberar);
                    
                    planificador_liberar_exec(ctx->estratega, ctx->estratega->proceso_exec->pid);
                }
                despachar_proceso_a_cpu(ctx, cliente_fd);
                list_destroy_and_destroy_elements(paquete, free);
                break;
            }
            case SYSCALL_MEM_ALLOC:
            case SYSCALL_MEM_FREE:
                procesar_syscall_memoria(cliente_fd, ctx, mensaje_cliente);
                break;
            case FIN_DE_QUANTUM:
                procesar_fin_quantum(cliente_fd, ctx);
                break;
            case -1:
                log_warning(ctx->logger, "El cliente se desconectó.");
                close(cliente_fd);
                return;
            default:
                log_warning(ctx->logger, "No entiendo el mensaje o código de operación: %d", mensaje_cliente);
                close(cliente_fd);
                return;
        }
    }
}