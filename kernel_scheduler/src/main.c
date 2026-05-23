#include <utils/utils_server.h>
#include <utils/utils_client.h>
#include "planificador.h"

t_estratega_planificacion* estratega;


// FUNCIÓN AUXILIAR: Saca de READY y manda a EXEC en la CPU
void despachar_proceso_a_cpu(int socket_cpu) {
    t_proceso_planificable* proceso_a_ejecutar = planificador_despachar_siguiente(estratega);
    
    if (proceso_a_ejecutar != NULL) {
        t_paquete* paquete_pid = crear_paquete();
        paquete_pid->codigo_operacion = PID_A_EJECUTAR;
        
        // Le mandamos el PID a la CPU
        agregar_a_paquete(paquete_pid, &(proceso_a_ejecutar->pid), sizeof(int));
        enviar_paquete(paquete_pid, socket_cpu);
        eliminar_paquete(paquete_pid);
        
        log_info(logger, "Se envió el PID %d a la CPU para ejecutar", proceso_a_ejecutar->pid);
    } else {
        log_info(logger, "No hay procesos en la cola READY para despachar a la CPU.");
    }
}

// FUNCIÓN PARA EL HILO QUE ATIENDE A CADA CLIENTE (CPU, IO)
void atender_cliente_scheduler(void* arg) {
    int cliente_fd = *(int*)arg;
    free(arg);

    while (1) {
        int mensaje_cliente = recibir_operacion(cliente_fd);
        
        if (mensaje_cliente == MENSAJE) {
            char* mensaje = recibir_handshake(cliente_fd);
            if (string_starts_with(mensaje, "CPU")) {
                log_info(logger, "## CPU %s Conectada", mensaje);
              
                despachar_proceso_a_cpu(cliente_fd);

            } else {
                log_info(logger, "Se conectó módulo: %s", mensaje);
            }
            free(mensaje);
        }
        else if (mensaje_cliente == SYSCALL_EXIT) {
            // La CPU nos avisa que el proceso actual ejecutó EXIT
            if (estratega->proceso_exec != NULL) {
                log_info(logger, "## Finaliza el proceso %d - Motivo: SUCCESS", estratega->proceso_exec->pid);
                estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
                
                // Liberamos el estado EXEC
                planificador_liberar_exec(estratega, estratega->proceso_exec->pid);
                
                // (Opcional acá: mandar msj a memoria para que libere sus estructuras)
            }
            
            // Mandamos el siguiente proceso a la CPU
            despachar_proceso_a_cpu(cliente_fd);
        }
        else if (mensaje_cliente == FIN_DE_QUANTUM) {
            // La CPU nos avisa que hubo interrupción por fin de Quantum (RR)
            if (estratega->proceso_exec != NULL) {
                log_info(logger, "## Fin de Quantum para proceso %d - Desalojado", estratega->proceso_exec->pid);
                
                // Lo volvemos a meter en la cola correspondiente
                planificador_reencolar_por_quantum(estratega, estratega->proceso_exec);
                planificador_liberar_exec(estratega, estratega->proceso_exec->pid);
            }
            
            // Mandamos el siguiente proceso a la CPU
            despachar_proceso_a_cpu(cliente_fd);
        }
        else if (mensaje_cliente == -1) {
            log_warning(logger, "El cliente se desconectó.");
            break; // Salimos del while para cerrar el hilo
        }
        else {
            log_warning(logger, "No entiendo el mensaje o código de operación: %d", mensaje_cliente);
            // Dependiendo de tu utils_server, acá capaz necesites recibir el buffer para destrabar el socket
        }
    }
    close(cliente_fd);
}

int main(int argc, char* argv[]) {
    // 1. Validar Argumentos
    if (argc != 3) {
        printf("Uso: ./bin/kernel_scheduler [Archivo Config] [Path Proceso Inicial]\n");
        return EXIT_FAILURE;
    }
    char* path_config = argv[1];
    char* path_script_inicial = argv[2];

    // 2. Iniciar Logger y Configuración
    logger = log_create("Scheduler.log", "Scheduler", true, LOG_LEVEL_DEBUG);
    t_config* config = config_create(path_config);
    if (config == NULL) {
        log_error(logger, "No se pudo leer la configuración del scheduler");
        return EXIT_FAILURE;
    }

    char* ip_memory = config_get_string_value(config, "IP_MEMORY");
    char* puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");
    char* puerto_escucha = config_get_string_value(config, "PUERTO_SCHEDULER");
    estratega = crear_estratega_planificacion(config, logger);

    // 3. Conectarse a Memoria
    int socket_memoria = crear_conexion(ip_memory, puerto_memory);
    if(socket_memoria != -1) {
        log_info(logger, "## Conectado a Kernel Memory");
        enviar_mensaje("SCHEDULER", socket_memoria);
    } else {
        log_error(logger, "Fallo conexion a memoria");
        return EXIT_FAILURE;
    }

    // 4. Iniciar el Servidor del Kernel
    int server_fd = iniciar_servidor(puerto_escucha);
    log_info(logger, "Kernel Scheduler listo para recibir clientes en el puerto %s", puerto_escucha);

    // 5. ¡CREACIÓN DEL PROCESO INICIAL!
    log_info(logger, "Se debe iniciar el proceso con script: %s", path_script_inicial);
    
    // a. Creamos el PCB
    t_proceso_planificable* pcb_inicial = malloc(sizeof(t_proceso_planificable));
    pcb_inicial->pid = 0; // Para el primer proceso usamos PID 0
    pcb_inicial->cola_prioridad = 0;
    
    // b. Avisamos a Memoria para que inicialice (le pasamos el PID y el Path)
    t_paquete* paquete_memoria = crear_paquete();
    paquete_memoria->codigo_operacion = CREAR_PROCESO;
    agregar_a_paquete(paquete_memoria, &(pcb_inicial->pid), sizeof(int));
    agregar_a_paquete(paquete_memoria, path_script_inicial, strlen(path_script_inicial) + 1);
    enviar_paquete(paquete_memoria, socket_memoria);
    eliminar_paquete(paquete_memoria);
    
    // c. Metemos el PCB en el sistema de planificacion
    planificador_encolar_new(estratega, pcb_inicial);
    planificador_admitir_a_ready(estratega);

    // 6. Bucle principal para crear hilos
    while (1) {
        int* cliente_fd = malloc(sizeof(int));
        *cliente_fd = esperar_cliente(server_fd);
        
        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_scheduler, cliente_fd);
        pthread_detach(hilo_cliente);
    }

    config_destroy(config);
    destruir_estratega_planificacion(estratega);
    return 0;
}