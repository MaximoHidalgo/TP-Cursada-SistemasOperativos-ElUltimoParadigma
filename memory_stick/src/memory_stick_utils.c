#include "memory_stick_utils.h"
#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>

t_memory_stick_context* inicializar_entorno(int argc, char* argv[]) {
    t_memory_stick_context* ctx = malloc(sizeof(t_memory_stick_context));
    
    if (argc != 3) {
        printf("Uso: ./bin/memory_stick [Config] [Tamaño]\n");
        free(ctx);
        exit(EXIT_FAILURE);
    }
    
    char* path_config = argv[1];
    int size_arg = atoi(argv[2]);
    if (size_arg <= 0) {
        printf("Error: El tamaño del Memory Stick debe ser mayor a 0\n");
        free(ctx);
        exit(EXIT_FAILURE);
    }
    uint32_t tamanio_stick = (uint32_t)size_arg;

    ctx->config = config_create(path_config);
    if (ctx->config == NULL) {
        printf("No se pudo cargar la configuración desde %s\n", path_config);
        free(ctx);
        exit(EXIT_FAILURE);
    }
    
    char* log_level_str = config_get_string_value(ctx->config, "LOG_LEVEL");
    t_log_level level = log_level_str ? log_level_from_string(log_level_str) : LOG_LEVEL_INFO;
    ctx->logger = log_create("memory_stick.log", "MEMORY_STICK", true, level);

    ctx->tamanio_total = tamanio_stick;
    ctx->espacio_memoria = malloc(tamanio_stick);
    if (ctx->espacio_memoria == NULL) {
        log_error(ctx->logger, "Error: No se pudo asignar %u bytes para el Memory Stick", tamanio_stick);
        config_destroy(ctx->config);
        log_destroy(ctx->logger);
        free(ctx);
        exit(EXIT_FAILURE);
    }
    memset(ctx->espacio_memoria, 0, tamanio_stick); // Inicializar en 0

    pthread_mutex_init(&ctx->mutex_memoria, NULL);

    ctx->puerto_escucha = config_get_string_value(ctx->config, "PUERTO_STICK");
    ctx->ip_memory = config_get_string_value(ctx->config, "IP_MEMORY");
    ctx->puerto_memory = config_get_string_value(ctx->config, "PUERTO_MEMORY");
    ctx->memory_delay = config_get_int_value(ctx->config, "MEMORY_DELAY");
    ctx->socket_kernel_memory = -1;
    ctx->socket_escucha = -1;

    return ctx;
}

void conectar_kernel_memory(t_memory_stick_context* ctx) {
    ctx->socket_kernel_memory = crear_conexion(ctx->ip_memory, ctx->puerto_memory);
    if (ctx->socket_kernel_memory == -1) {
        log_error(ctx->logger, "No se pudo conectar a Kernel Memory en %s:%s", ctx->ip_memory, ctx->puerto_memory);
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }
    log_info(ctx->logger, "## Conectado a Kernel Memory");

    // Mandar Handshake indicando el tamaño, IP y puerto de escucha
    char *msj_handshake = string_from_format("%s|%u|%s|%s", HANDSHAKE_STICK, ctx->tamanio_total, "127.0.0.1", ctx->puerto_escucha);
    enviar_mensaje(msj_handshake, ctx->socket_kernel_memory);
    free(msj_handshake);
    
    t_args_hilo_cpu* args = malloc(sizeof(t_args_hilo_cpu));
    args->socket_cliente = ctx->socket_kernel_memory;
    args->ctx = ctx;

    pthread_t hilo_km;
    if (pthread_create(&hilo_km, NULL, atender_cliente_cpu, args) != 0) {
        log_error(ctx->logger, "Fallo al crear el hilo para Kernel Memory");
        free(args);
    } else {
        pthread_detach(hilo_km);
    }
}

void iniciar_servidor_cpus(t_memory_stick_context* ctx) {
    ctx->socket_escucha = iniciar_servidor(ctx->puerto_escucha, ctx->logger);
    if (ctx->socket_escucha == -1) {
        log_error(ctx->logger, "Fallo al iniciar el servidor del Memory Stick en el puerto %s", ctx->puerto_escucha);
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }
    log_info(ctx->logger, "Memory Stick listo para recibir CPUs en el puerto %s", ctx->puerto_escucha);

    while (1) {
        int socket_cpu = esperar_cliente(ctx->socket_escucha, ctx->logger);
        if (socket_cpu == -1) {
            log_warning(ctx->logger, "Error al aceptar una nueva conexión de CPU");
            continue;
        }

        t_args_hilo_cpu* args = malloc(sizeof(t_args_hilo_cpu));
        args->socket_cliente = socket_cpu;
        args->ctx = ctx;

        pthread_t hilo_cpu;
        if (pthread_create(&hilo_cpu, NULL, atender_cliente_cpu, args) != 0) {
            log_error(ctx->logger, "Fallo al crear el hilo para la CPU");
            free(args);
            close(socket_cpu);
        } else {
            pthread_detach(hilo_cpu); // Permite al hilo liberar sus propios recursos al terminar
        }
    }
}

void limpiar_contexto(t_memory_stick_context* ctx) {
    if (ctx->socket_escucha != -1) close(ctx->socket_escucha);
    if (ctx->socket_kernel_memory != -1) close(ctx->socket_kernel_memory);
    
    if (ctx->espacio_memoria != NULL) {
        free(ctx->espacio_memoria);
    }
    pthread_mutex_destroy(&ctx->mutex_memoria);
    
    if (ctx->config != NULL) config_destroy(ctx->config);
    if (ctx->logger != NULL) log_destroy(ctx->logger);
    
    free(ctx);
}