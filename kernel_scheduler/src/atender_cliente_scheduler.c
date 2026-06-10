#include "atender_cliente_scheduler.h"

void despachar_proceso_a_cpu(t_estratega_planificacion* estratega, int socket_cpu) {
    t_proceso_planificable* proceso_a_ejecutar = planificador_despachar_siguiente(estratega);
    
    if (proceso_a_ejecutar != NULL) {
        t_paquete* paquete_pid = crear_paquete();
        paquete_pid->codigo_operacion = PID_A_EJECUTAR;
        
        agregar_a_paquete(paquete_pid, &(proceso_a_ejecutar->pid), sizeof(int));
        enviar_paquete(paquete_pid, socket_cpu);
        eliminar_paquete(paquete_pid);
        
        log_info(estratega->logger, "Se envió el PID %d a la CPU para ejecutar", proceso_a_ejecutar->pid);
    } else {
        log_info(estratega->logger, "No hay procesos en la cola READY para despachar a la CPU.");
    }
}

void procesar_mensaje_handshake(int cliente_fd, t_estratega_planificacion* estratega) {
    char* mensaje = recibir_handshake(cliente_fd);
    if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        log_info(estratega->logger, "## CPU %s Conectada", mensaje);
        estratega->socket_cpu = cliente_fd;
        despachar_proceso_a_cpu(estratega, cliente_fd);
    } else if (string_equals_ignore_case(mensaje, "OK")) {
        planificador_io_terminada(estratega, cliente_fd);
        if (estratega->proceso_exec == NULL && estratega->socket_cpu != -1) {
            despachar_proceso_a_cpu(estratega, estratega->socket_cpu);
        }
    } else {
        planificador_registrar_io(estratega, mensaje, cliente_fd);
    }
    free(mensaje);
}

void procesar_syscall_io(int cliente_fd, t_estratega_planificacion* estratega, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    
    if (estratega->proceso_exec != NULL && estratega->proceso_exec->pid == pid) {
        if (tipo_syscall == SYSCALL_SLEEP) {
            uint32_t* tiempo_ptr = list_get(paquete, 1);
            
            t_interfaz_io* interfaz = buscar_interfaz_por_nombre(estratega, "SLEEP");
            if (interfaz != NULL) {
                int op = PETICION_IO;
                send(interfaz->socket_fd, &op, sizeof(int), 0);
                send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
                send(interfaz->socket_fd, tiempo_ptr, sizeof(uint32_t), 0);
            }
            
            planificador_atender_peticion_io(estratega, estratega->proceso_exec, "SLEEP");
        } 
        else if (tipo_syscall == SYSCALL_STDOUT) {
            char* dummy = list_get(paquete, 1);
            
            t_interfaz_io* interfaz = buscar_interfaz_por_nombre(estratega, "STDOUT");
            if (interfaz != NULL) {
                int op = PETICION_IO;
                send(interfaz->socket_fd, &op, sizeof(int), 0);
                send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
                
                int len = strlen(dummy) + 1;
                send(interfaz->socket_fd, &len, sizeof(int), 0);
                send(interfaz->socket_fd, dummy, len, 0);
            }
            
            planificador_atender_peticion_io(estratega, estratega->proceso_exec, "STDOUT");
        }
        else if (tipo_syscall == SYSCALL_STDIN) {
            uint32_t* size_ptr = list_get(paquete, 1);
            
            t_interfaz_io* interfaz = buscar_interfaz_por_nombre(estratega, "STDIN");
            if (interfaz != NULL) {
                int op = PETICION_IO;
                send(interfaz->socket_fd, &op, sizeof(int), 0);
                send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
                send(interfaz->socket_fd, size_ptr, sizeof(uint32_t), 0);
            }
            
            planificador_atender_peticion_io(estratega, estratega->proceso_exec, "STDIN");
        }
        
        despachar_proceso_a_cpu(estratega, cliente_fd);
    } else {
        despachar_proceso_a_cpu(estratega, cliente_fd);
    }
    
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_exit(int cliente_fd, t_estratega_planificacion* estratega) {
    t_list* paquete = recibir_paquete(cliente_fd);
    if (estratega->proceso_exec != NULL) {
        log_info(estratega->logger, "## Finaliza el proceso %d - Motivo: SUCCESS", estratega->proceso_exec->pid);
        estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
        
        planificador_liberar_exec(estratega, estratega->proceso_exec->pid);
    }
    despachar_proceso_a_cpu(estratega, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_mutex(int cliente_fd, t_estratega_planificacion* estratega, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    
    char* recurso = list_get(paquete, 1);

    if (estratega->proceso_exec != NULL && estratega->proceso_exec->pid == pid) {
        if (tipo_syscall == SYSCALL_MUTEX_CREATE) {
            planificador_mutex_create(estratega, estratega->proceso_exec, recurso);
            
            // Re-enviar el PID a la CPU para que continúe
            t_paquete* paquete_pid = crear_paquete();
            paquete_pid->codigo_operacion = PID_A_EJECUTAR;
            agregar_a_paquete(paquete_pid, &(estratega->proceso_exec->pid), sizeof(int));
            enviar_paquete(paquete_pid, cliente_fd);
            eliminar_paquete(paquete_pid);
            
        } else if (tipo_syscall == SYSCALL_MUTEX_LOCK) {
            int se_bloqueo = planificador_mutex_lock(estratega, estratega->proceso_exec, recurso);
            if (se_bloqueo == 0) {
                // Sigue en EXEC, le volvemos a dar el PID para que retome
                t_paquete* paquete_pid = crear_paquete();
                paquete_pid->codigo_operacion = PID_A_EJECUTAR;
                agregar_a_paquete(paquete_pid, &(estratega->proceso_exec->pid), sizeof(int));
                enviar_paquete(paquete_pid, cliente_fd);
                eliminar_paquete(paquete_pid);
            } else {
                // Se bloqueó, despachamos el siguiente
                despachar_proceso_a_cpu(estratega, cliente_fd);
            }
        } else if (tipo_syscall == SYSCALL_MUTEX_UNLOCK) {
            planificador_mutex_unlock(estratega, estratega->proceso_exec, recurso);
            
            // Sigue en EXEC
            t_paquete* paquete_pid = crear_paquete();
            paquete_pid->codigo_operacion = PID_A_EJECUTAR;
            agregar_a_paquete(paquete_pid, &(estratega->proceso_exec->pid), sizeof(int));
            enviar_paquete(paquete_pid, cliente_fd);
            eliminar_paquete(paquete_pid);
        }
    } else {
        despachar_proceso_a_cpu(estratega, cliente_fd);
    }
    
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_fin_quantum(int cliente_fd, t_estratega_planificacion* estratega) {
    t_list* paquete = recibir_paquete(cliente_fd);
    if (estratega->proceso_exec != NULL) {
        log_info(estratega->logger, "## Fin de Quantum para proceso %d - Desalojado", estratega->proceso_exec->pid);
        
        planificador_reencolar_por_quantum(estratega, estratega->proceso_exec);
        planificador_liberar_exec(estratega, estratega->proceso_exec->pid);
    }
    despachar_proceso_a_cpu(estratega, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void atender_cliente_scheduler(void* arg) {
    t_args_hilo_scheduler* args = (t_args_hilo_scheduler*)arg;
    int cliente_fd = args->cliente_fd;
    t_estratega_planificacion* estratega = args->estratega;
    free(args);

    while (1) {
        int mensaje_cliente = recibir_operacion(cliente_fd);
        
        switch(mensaje_cliente) {
            case MENSAJE:
                procesar_mensaje_handshake(cliente_fd, estratega);
                break;
            case SYSCALL_EXIT:
                procesar_syscall_exit(cliente_fd, estratega);
                break;
            case SYSCALL_SLEEP:
            case SYSCALL_STDOUT:
            case SYSCALL_STDIN:
                procesar_syscall_io(cliente_fd, estratega, mensaje_cliente);
                break;
            case SYSCALL_MUTEX_CREATE:
            case SYSCALL_MUTEX_LOCK:
            case SYSCALL_MUTEX_UNLOCK:
                procesar_syscall_mutex(cliente_fd, estratega, mensaje_cliente);
                break;
            case FIN_DE_QUANTUM:
                procesar_fin_quantum(cliente_fd, estratega);
                break;
            case -1:
                log_warning(estratega->logger, "El cliente se desconectó.");
                close(cliente_fd);
                return;
            default:
                log_warning(estratega->logger, "No entiendo el mensaje o código de operación: %d", mensaje_cliente);
                close(cliente_fd);
                return;
        }
    }
}