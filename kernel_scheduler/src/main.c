#include <utils/utils_server.h>
#include <utils/utils_client.h>
#include "planificador.h"

typedef struct {
    int cliente_fd;
    t_estratega_planificacion* estratega;
} t_args_hilo_scheduler;

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

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: ./bin/kernel_scheduler [Archivo Config] [Path Proceso Inicial]\n");
        return EXIT_FAILURE;
    }
    char* path_config = argv[1];
    char* path_script_inicial = argv[2];

    logger = log_create("Scheduler.log", "Scheduler", true, LOG_LEVEL_DEBUG);
    t_config* config = config_create(path_config);
    if (config == NULL) {
        log_error(logger, "No se pudo leer la configuración del scheduler");
        return EXIT_FAILURE;
    }

    char* ip_memory = config_get_string_value(config, "IP_MEMORY");
    char* puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");
    char* puerto_escucha = config_get_string_value(config, "PUERTO_SCHEDULER");
    t_estratega_planificacion* estratega = crear_estratega_planificacion(config, logger);

    int socket_memoria = crear_conexion(ip_memory, puerto_memory);
    if(socket_memoria != -1) {
        log_info(logger, "## Conectado a Kernel Memory");
        enviar_mensaje(HANDSHAKE_SCHEDULER, socket_memoria);
    } else {
        log_error(logger, "Fallo conexion a memoria");
        return EXIT_FAILURE;
    }

    int server_fd = iniciar_servidor(puerto_escucha);
    if (server_fd == -1) return EXIT_FAILURE;

    log_info(logger, "Kernel Scheduler listo para recibir clientes en el puerto %s", puerto_escucha);

    log_info(logger, "Se debe iniciar el proceso con script: %s", path_script_inicial);
    
    t_proceso_planificable* pcb_inicial = malloc(sizeof(t_proceso_planificable));
    pcb_inicial->pid = 0; 
    pcb_inicial->cola_prioridad = 0;
    
    enviar_peticion_crear_proceso(pcb_inicial->pid, path_script_inicial, socket_memoria);
    
    planificador_encolar_new(estratega, pcb_inicial);
    planificador_admitir_a_ready(estratega);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd == -1) continue;
        
        t_args_hilo_scheduler* args = malloc(sizeof(t_args_hilo_scheduler));
        args->cliente_fd = cliente_fd;
        args->estratega = estratega;

        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_scheduler, args);
        pthread_detach(hilo_cliente);
    }

    config_destroy(config);
    destruir_estratega_planificacion(estratega);
    return 0;
}