#include "main.h"

int main(int argc, char* argv[]) {
    t_scheduler_context* ctx = iniciar_contexto(argc, argv);

    conectar_kernel_memory(ctx);
    iniciar_planificacion(ctx);
    iniciar_servidor_clientes(ctx);

    limpiar_contexto(ctx);
    return EXIT_SUCCESS;
}

t_scheduler_context* iniciar_contexto(int argc, char* argv[]) {
    t_scheduler_context* ctx = malloc(sizeof(t_scheduler_context));
    
    if (argc != 3) {
        printf("Uso: ./bin/kernel_scheduler [Archivo Config] [Path Proceso Inicial]\n");
        free(ctx);
        exit(EXIT_FAILURE);
    }
    
    char* path_config = argv[1];
    ctx->path_script_inicial = argv[2];

    ctx->config = config_create(path_config);
    if (ctx->config == NULL) {
        printf("Error: No se pudo leer la configuración en %s\n", path_config);
        free(ctx);
        exit(EXIT_FAILURE);
    }

    char* log_level_str = config_get_string_value(ctx->config, "LOG_LEVEL");
    t_log_level level = LOG_LEVEL_DEBUG;
    if (log_level_str != NULL) {
        if (string_equals_ignore_case(log_level_str, "TRACE")) level = LOG_LEVEL_TRACE;
        else if (string_equals_ignore_case(log_level_str, "DEBUG")) level = LOG_LEVEL_DEBUG;
        else if (string_equals_ignore_case(log_level_str, "INFO")) level = LOG_LEVEL_INFO;
        else if (string_equals_ignore_case(log_level_str, "WARNING")) level = LOG_LEVEL_WARNING;
        else if (string_equals_ignore_case(log_level_str, "ERROR")) level = LOG_LEVEL_ERROR;
    }

    ctx->logger = log_create("Scheduler.log", "Scheduler", true, level);

    ctx->ip_memory = config_get_string_value(ctx->config, "IP_MEMORY");
    ctx->puerto_memory = config_get_string_value(ctx->config, "PUERTO_MEMORY");
    ctx->puerto_escucha = config_get_string_value(ctx->config, "PUERTO_SCHEDULER");
    ctx->socket_memoria = -1;
    ctx->socket_escucha = -1;

    // Crear el estratega (contexto de negocio)
    ctx->estratega = crear_estratega_planificacion(ctx->config, ctx->logger);

    return ctx;
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

void iniciar_planificacion(t_scheduler_context* ctx) {
    log_info(ctx->logger, "Se debe iniciar el proceso con script: %s", ctx->path_script_inicial);
    
    t_proceso_planificable* pcb_inicial = malloc(sizeof(t_proceso_planificable));
    pcb_inicial->pid = 0; 
    pcb_inicial->cola_prioridad = 0;
    pcb_inicial->bloqueo_id = 0;
    
    enviar_peticion_crear_proceso(pcb_inicial->pid, ctx->path_script_inicial, ctx->socket_memoria);
    
    planificador_encolar_new(ctx->estratega, pcb_inicial);
    planificador_admitir_a_ready(ctx->estratega);
}

void iniciar_servidor_clientes(t_scheduler_context* ctx) {
    ctx->socket_escucha = iniciar_servidor(ctx->puerto_escucha, ctx->logger);
    if (ctx->socket_escucha == -1) {
        log_error(ctx->logger, "No se pudo iniciar el servidor en el puerto %s", ctx->puerto_escucha);
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }

    log_info(ctx->logger, "Kernel Scheduler listo para recibir clientes en el puerto %s", ctx->puerto_escucha);

    while (1) {
        int cliente_fd = esperar_cliente(ctx->socket_escucha, ctx->logger);
        if (cliente_fd == -1) continue;
        
        t_args_hilo_scheduler* args = malloc(sizeof(t_args_hilo_scheduler));
        args->cliente_fd = cliente_fd;
        args->ctx = ctx;

        pthread_t hilo_cliente;
        if (pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_scheduler, args) != 0) {
            log_error(ctx->logger, "Fallo al crear hilo cliente");
            free(args);
            close(cliente_fd);
        } else {
            pthread_detach(hilo_cliente);
        }
    }
}

