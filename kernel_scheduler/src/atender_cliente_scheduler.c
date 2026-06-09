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
        despachar_proceso_a_cpu(estratega, cliente_fd);
    } else {
        log_info(estratega->logger, "Se conectó módulo: %s", mensaje);
    }
    free(mensaje);
}

void procesar_syscall_exit(int cliente_fd, t_estratega_planificacion* estratega) {
    if (estratega->proceso_exec != NULL) {
        log_info(estratega->logger, "## Finaliza el proceso %d - Motivo: SUCCESS", estratega->proceso_exec->pid);
        estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
        
        planificador_liberar_exec(estratega, estratega->proceso_exec->pid);
    }
    despachar_proceso_a_cpu(estratega, cliente_fd);
}

void procesar_fin_quantum(int cliente_fd, t_estratega_planificacion* estratega) {
    if (estratega->proceso_exec != NULL) {
        log_info(estratega->logger, "## Fin de Quantum para proceso %d - Desalojado", estratega->proceso_exec->pid);
        
        planificador_reencolar_por_quantum(estratega, estratega->proceso_exec);
        planificador_liberar_exec(estratega, estratega->proceso_exec->pid);
    }
    despachar_proceso_a_cpu(estratega, cliente_fd);
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