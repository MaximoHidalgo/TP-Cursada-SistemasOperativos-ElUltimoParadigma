#include "inicializacion.h"

static void validar_configuracion(t_config* configuracion, t_log* logger) {
    if (!config_has_property(configuracion, "PUERTO_SCHEDULER") ||
        !config_has_property(configuracion, "IP_MEMORY") ||
        !config_has_property(configuracion, "PUERTO_MEMORY") ||
        !config_has_property(configuracion, "PLANIFICATION_ALGORITHM") ||
        !config_has_property(configuracion, "QUEUES_ALGORITHMS") ||
        !config_has_property(configuracion, "RR_QUANTUM") ||
        !config_has_property(configuracion, "QUEUE_PREEMPTION") ||
        !config_has_property(configuracion, "SUSPENSION_TIMEOUT")) {
        log_error(logger, "Faltan claves obligatorias en el archivo de configuracion (PUERTO_SCHEDULER, IP_MEMORY, PUERTO_MEMORY, PLANIFICATION_ALGORITHM, QUEUES_ALGORITHMS, RR_QUANTUM, QUEUE_PREEMPTION, SUSPENSION_TIMEOUT)");
        config_destroy(configuracion);
        log_destroy(logger);
        exit(EXIT_FAILURE);
    }
}

t_scheduler_context* iniciar_contexto(int argc, char* argv[]) {
    char* path_config = argv[1];
    char* ruta_script_inicial = argv[2];

    t_config* configuracion = crear_configuracion(path_config);
    t_log* logger = crear_logger_configurado(configuracion);
    
    validar_configuracion(configuracion, logger);

    t_estratega_planificacion* estratega = crear_estratega_planificacion(configuracion, logger);

    t_scheduler_context* ctx = calloc(1, sizeof(t_scheduler_context));
    if (ctx != NULL) {
        ctx->path_script_inicial = ruta_script_inicial;
        ctx->ip_memory = config_get_string_value(configuracion, "IP_MEMORY");
        ctx->puerto_memory = config_get_string_value(configuracion, "PUERTO_MEMORY");
        ctx->puerto_escucha = config_get_string_value(configuracion, "PUERTO_SCHEDULER");
        ctx->socket_memoria = -1;
        ctx->socket_escucha = -1;
        ctx->logger = logger;
        ctx->config = configuracion;
        ctx->estratega = estratega;
    }

    return ctx;
}

void iniciar_planificacion(t_scheduler_context* ctx) {
    log_info(ctx->logger, "Se debe iniciar el proceso con script: %s", ctx->path_script_inicial);
    
    t_proceso_planificable* pcb_inicial = pcb_crear(0, 0);

    pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
    enviar_peticion_crear_proceso(pcb_inicial->pid, ctx->path_script_inicial, ctx->socket_memoria);
    int resp_mem = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
    if (resp_mem == MENSAJE) {
        int size;
        char *mensaje = recibir_buffer(&size, ctx->socket_memoria);
        if (strcmp(mensaje, "OK") == 0) {
            log_info(ctx->logger, "## (0) Se crea el proceso inicial del scheduler.");
        }
        free(mensaje);
    }
    pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);

    planificador_encolar_new(ctx->estratega, pcb_inicial);
    planificador_admitir_a_ready(ctx->estratega);
}

void conectar_kernel_memory(t_scheduler_context* ctx) {
    ctx->socket_memoria = crear_conexion(ctx->ip_memory, ctx->puerto_memory);
    if(ctx->socket_memoria != -1) {
        log_info(ctx->logger, "## Conectado a Kernel Memory");
        enviar_mensaje(HANDSHAKE_SCHEDULER, ctx->socket_memoria);
        ctx->estratega->socket_memoria = ctx->socket_memoria;
    } else {
        log_error(ctx->logger, "Error al conectar con Kernel Memory en %s:%s", ctx->ip_memory, ctx->puerto_memory);
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }
}