#include "main.h"

t_log* logger;
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

    int server_fd = iniciar_servidor(puerto_escucha, logger);
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
        int cliente_fd = esperar_cliente(server_fd, logger);
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