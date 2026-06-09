#include "main.h"

int main(int argc, char *argv[]) {
    // 1. Inicialización y Validación
    logger = log_create("memory_stick.log", "MEMORY_STICK", true, LOG_LEVEL_INFO);

    if (argc != 3) {
        log_error(logger, "Uso: ./bin/memory_stick [Config] [Tamaño]");
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    
    char* path_config = argv[1];
    uint32_t tamanio_stick = (uint32_t)atoi(argv[2]);

    if (tamanio_stick <= 0) {
        log_error(logger, "Error: El tamaño del Memory Stick debe ser mayor a 0");
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    t_config *config = config_create(path_config);
    if (config == NULL) {
        log_error(logger, "No se pudo cargar la configuración desde %s", path_config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    // 2. Creación del contexto global (Espacio de memoria)
    t_memory_stick_context ctx;
    ctx.espacio_memoria = malloc(tamanio_stick);
    if (ctx.espacio_memoria == NULL) {
        log_error(logger, "Error: No se pudo asignar %u bytes para el Memory Stick", tamanio_stick);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    memset(ctx.espacio_memoria, 0, tamanio_stick); // Inicializar en 0
    ctx.tamanio_total = tamanio_stick;
    ctx.logger = logger;
    pthread_mutex_init(&ctx.mutex_memoria, NULL);

    char *puerto_escucha = config_get_string_value(config, "PUERTO_STICK");
    char *ip_memory = config_get_string_value(config, "IP_MEMORY");
    char *puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");

    // 3. Conexión a Kernel Memory
    int socket_kernel_memory = crear_conexion(ip_memory, puerto_memory);
    if (socket_kernel_memory == -1) {
        log_error(logger, "No se pudo conectar a Kernel Memory en %s:%s", ip_memory, puerto_memory);
        free(ctx.espacio_memoria);
        pthread_mutex_destroy(&ctx.mutex_memoria);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    log_info(logger, "## Conectado a Kernel Memory");

    // Mandar Handshake indicando el tamaño
    char *msj_handshake = string_from_format("%s|%u", HANDSHAKE_STICK, tamanio_stick);
    enviar_mensaje(msj_handshake, socket_kernel_memory);
    free(msj_handshake);

    // 4. Iniciar servidor para recibir a las CPUs
    int socket_escucha = iniciar_servidor(puerto_escucha);
    if (socket_escucha == -1) {
        log_error(logger, "Fallo al iniciar el servidor del Memory Stick en el puerto %s", puerto_escucha);
        close(socket_kernel_memory);
        free(ctx.espacio_memoria);
        pthread_mutex_destroy(&ctx.mutex_memoria);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    log_info(logger, "Memory Stick listo para recibir CPUs en el puerto %s", puerto_escucha);

    // 5. Ciclo principal para atender a las CPUs concurrentemente
    while (1) {
        int socket_cpu = esperar_cliente(socket_escucha);
        if (socket_cpu == -1) {
            log_warning(logger, "Error al aceptar una nueva conexión de CPU");
            continue;
        }

        t_args_hilo_cpu* args = malloc(sizeof(t_args_hilo_cpu));
        args->socket_cliente = socket_cpu;
        args->ctx = &ctx;

        pthread_t hilo_cpu;
        if (pthread_create(&hilo_cpu, NULL, atender_cliente_cpu, args) != 0) {
            log_error(logger, "Fallo al crear el hilo para la CPU");
            free(args);
            close(socket_cpu);
        } else {
            pthread_detach(hilo_cpu); // Permite al hilo liberar sus propios recursos al terminar
        }
    }

    // 6. Cleanup (Inalcanzable en la práctica con un while(1), pero se añade por completitud)
    close(socket_escucha);
    close(socket_kernel_memory);
    free(ctx.espacio_memoria);
    pthread_mutex_destroy(&ctx.mutex_memoria);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
