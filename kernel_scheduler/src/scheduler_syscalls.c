#include "scheduler_syscalls.h"
#include "planificador_io.h"
#include "planificador_recursos.h"
#include "planificador_memoria.h"

void despachar_proceso_a_cpu(t_scheduler_context* ctx, int socket_cpu) {
    t_proceso_planificable* proceso_a_ejecutar = planificador_despachar_siguiente(ctx->estratega);
    
    if (proceso_a_ejecutar != NULL) {
        t_paquete* paquete_pid = crear_paquete();
        paquete_pid->codigo_operacion = PID_A_EJECUTAR;
        
        agregar_a_paquete(paquete_pid, &(proceso_a_ejecutar->pid), sizeof(int));
        enviar_paquete(paquete_pid, socket_cpu);
        eliminar_paquete(paquete_pid);
        
        log_info(ctx->logger, "Se envió el PID %d a la CPU para ejecutar", proceso_a_ejecutar->pid);
        planificador_iniciar_quantum_si_corresponde(ctx->estratega, proceso_a_ejecutar, socket_cpu);
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
    int size;
    char* mensaje = recibir_buffer(&size, cliente_fd);

    if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        log_info(ctx->logger, "## CPU %s Conectada", mensaje);
        //ctx->estratega->socket_cpu = cliente_fd;
        //despachar_proceso_a_cpu(ctx, cliente_fd);

        // 1. Agregamos el socket a la lista de CPUs
        int* nuevo_socket_cpu = malloc(sizeof(int));
        *nuevo_socket_cpu = cliente_fd;
        list_add(ctx->estratega->sockets_cpu, nuevo_socket_cpu);
        
        // Despachamos a esta CPU que se acaba de conectar
        despachar_proceso_a_cpu(ctx, cliente_fd);
    } else {
        t_interfaz_io* interfaz = buscar_interfaz_por_socket(ctx->estratega, cliente_fd);
        if (interfaz != NULL) {
            // Mensaje de una IO ya registrada
            if (string_equals_ignore_case(mensaje, "OK")) {
                planificador_io_terminada(ctx->estratega, cliente_fd);

                // 2 y 3. Verificamos la lista e intentamos despachar
                if (ctx->estratega->proceso_exec == NULL && !list_is_empty(ctx->estratega->sockets_cpu)) {
                    for(int i = 0; i < list_size(ctx->estratega->sockets_cpu); i++) {
                        int* fd_cpu = list_get(ctx->estratega->sockets_cpu, i);
                        if (ctx->estratega->proceso_exec == NULL) {
                            despachar_proceso_a_cpu(ctx, *fd_cpu);
                        }
                    }
                }
            } else {
                // Es el texto leido por STDIN
                t_proceso_planificable* proceso = NULL;
                if (interfaz->solicitud_actual != NULL) {
                    proceso = interfaz->solicitud_actual->proceso;
                }
                
                if (proceso != NULL) {
                    t_paquete* paq_write = crear_paquete();
                    paq_write->codigo_operacion = ESCRIBIR_MEMORIA;
                    agregar_a_paquete(paq_write, &(proceso->pid), sizeof(uint32_t));
                    agregar_a_paquete(paq_write, &(proceso->io_dir_fisica), sizeof(uint32_t));
                    agregar_a_paquete(paq_write, &(proceso->io_tamanio), sizeof(uint32_t));
                    agregar_a_paquete(paq_write, mensaje, proceso->io_tamanio);
                    
                    pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
                    enviar_paquete(paq_write, ctx->socket_memoria);
                    eliminar_paquete(paq_write);
                    
                    int r_mem = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
                    if (r_mem == MENSAJE) {
                        int s_m; char* msj_m = recibir_buffer(&s_m, ctx->socket_memoria); free(msj_m);
                    }
                    pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
                }
                
                planificador_io_terminada(ctx->estratega, cliente_fd);

                if (ctx->estratega->proceso_exec == NULL && !list_is_empty(ctx->estratega->sockets_cpu)) {
                    for(int i = 0; i < list_size(ctx->estratega->sockets_cpu); i++) {
                        int* fd_cpu = list_get(ctx->estratega->sockets_cpu, i);
                        if (ctx->estratega->proceso_exec == NULL) {
                            despachar_proceso_a_cpu(ctx, *fd_cpu);
                        }
                    }
                }
            }
        } else {
            // Es el nombre de una nueva interfaz registrandose
            planificador_registrar_io(ctx->estratega, mensaje, cliente_fd);
        }
    }
    free(mensaje);
}


void procesar_syscall_exit(int cliente_fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    if (ctx->estratega->proceso_exec != NULL) {
        // Log obligatorio del TP
        log_info(ctx->logger, "## (%d) - Solicitó syscall: EXIT", ctx->estratega->proceso_exec->pid);
        
        log_info(ctx->logger, "## (%d) finalizó su ejecución con motivo de SUCCESS", ctx->estratega->proceso_exec->pid);
        ctx->estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
        
        t_paquete* paq_liberar = crear_paquete();
        paq_liberar->codigo_operacion = LIBERAR_PROCESO;
        agregar_a_paquete(paq_liberar, &(ctx->estratega->proceso_exec->pid), sizeof(uint32_t));
        
        pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
        enviar_paquete(paq_liberar, ctx->socket_memoria);
        pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
        
        eliminar_paquete(paq_liberar);
        
        planificador_liberar_exec(ctx->estratega, ctx->estratega->proceso_exec->pid);
        planificador_admitir_a_ready(ctx->estratega);
    }
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_mutex(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    char* recurso = list_get(paquete, 1);

    // Log obligatorio del TP
    log_info(ctx->logger, "## (%d) - Solicitó syscall: %s", pid, obtener_nombre_syscall(tipo_syscall));

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

        case SYSCALL_MUTEX_UNLOCK: {
            int debe_desalojar = planificador_mutex_unlock(ctx->estratega, ctx->estratega->proceso_exec, recurso);
            if (debe_desalojar) {
                encolar_en_cola_ready(ctx->estratega->colas, ctx->estratega->proceso_exec);
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

void procesar_syscall_memoria(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    
    // Log obligatorio del TP
    log_info(ctx->logger, "## (%d) - Solicitó syscall: %s", pid, obtener_nombre_syscall(tipo_syscall));

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
        
        pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
        enviar_paquete(paq_mem, ctx->socket_memoria);
        eliminar_paquete(paq_mem);

        int respuesta = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
    
        if (respuesta == SOLICITUD_COMPACTACION) {
            log_info(ctx->logger, "## Inicio de compactación"); // Log obligatorio del TP
            
            // Como la CPU ya desalojó el proceso y está inactiva, autorizamos inmediatamente a Memoria
            int op_resp = RESPUESTA_COMPACTACION;
            send(ctx->socket_memoria, &op_resp, sizeof(int), 0);

            
            // Esperamos a que termine de compactar y nos devuelva el resultado real (OK/ERROR)
            respuesta = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
        }
        if (respuesta == MENSAJE) {
            int size;
            char* msj = recibir_buffer(&size, ctx->socket_memoria);
            if (string_equals_ignore_case(msj, "OK")) {
                pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
                devolver_ejecucion_a_cpu(ctx, cliente_fd);
            } else {
                log_info(ctx->logger, "## Out of Memory para proceso %d. Reencolando en NEW para reintento.", pid);

                t_paquete* paq_pc = crear_paquete();
                paq_pc->codigo_operacion = RETROCEDER_PC_OUT_OF_MEMORY;
                agregar_a_paquete(paq_pc, &pid, sizeof(uint32_t));
                enviar_paquete(paq_pc, ctx->socket_memoria);
                eliminar_paquete(paq_pc);
                
                int rta = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
                if (rta == MENSAJE) {
                    int s; char* m = recibir_buffer(&s, ctx->socket_memoria); free(m);
                }
                
                // No matamos al proceso: lo devolvemos a cola_new para que el
                // planificador de mediano plazo lo re-admita cuando haya espacio
                // (tras la suspensión y swapeo de otros procesos).
                t_proceso_planificable* proceso_oom = ctx->estratega->proceso_exec;
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
        uint32_t* id_seg_ptr = list_get(paquete, 1);

        t_paquete* paq_mem = crear_paquete();
        paq_mem->codigo_operacion = ELIMINAR_SEGMENTO;
        agregar_a_paquete(paq_mem, &pid, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, id_seg_ptr, sizeof(uint32_t));
        
        pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
        enviar_paquete(paq_mem, ctx->socket_memoria);
        eliminar_paquete(paq_mem);

        int respuesta = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
        if (respuesta == MENSAJE) {
            int size;
            char* msj = recibir_buffer(&size, ctx->socket_memoria);
            if (string_equals_ignore_case(msj, "OK")) {
                pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
                devolver_ejecucion_a_cpu(ctx, cliente_fd);
            } else {
                log_error(ctx->logger, "Error de memoria al intentar liberar segmento %d para PID %d: %s", *id_seg_ptr, pid, msj);
                pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
                devolver_ejecucion_a_cpu(ctx, cliente_fd);
            }
            free(msj);
        }else{
            pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
            log_error(ctx->logger,"Error desconocido al intentar liberar el segmento %d del PID %d.",*id_seg_ptr,pid);
            devolver_ejecucion_a_cpu(ctx, cliente_fd);
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

void procesar_syscall_init_proc(int cliente_fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    char* script_path = list_get(paquete, 1);
    uint32_t* prioridad_ptr = list_get(paquete, 2);
    uint32_t prioridad = *prioridad_ptr;

    // Log obligatorio del TP
    log_info(ctx->logger, "## (%d) - Solicitó syscall: INIT_PROC", pid);

    // Guard Clause
    if (ctx->estratega->proceso_exec == NULL || ctx->estratega->proceso_exec->pid != pid) {
        despachar_proceso_a_cpu(ctx, cliente_fd);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    static int contador_pid = 1;
    int nuevo_pid = contador_pid++;

    log_info(ctx->logger, "## Proceso %d solicita crear nuevo proceso con script %s", pid, script_path);

    pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
    enviar_peticion_crear_proceso(nuevo_pid, script_path, ctx->socket_memoria);

    int resp_mem = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
    if (resp_mem == MENSAJE) {
        int s; char* msj = recibir_buffer(&s, ctx->socket_memoria); free(msj);
    }
    pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);

    t_proceso_planificable* pcb_nuevo = pcb_crear(nuevo_pid, prioridad);

    planificador_encolar_new(ctx->estratega, pcb_nuevo);
    planificador_admitir_a_ready(ctx->estratega);

    devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC

    list_destroy_and_destroy_elements(paquete, free);
}