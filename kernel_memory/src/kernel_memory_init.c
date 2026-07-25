#include "kernel_memory_init.h"

void inicializar_entorno(t_memory_context* ctx, int argc, char* argv[]) {
    ctx->config = config_create(argv[1]);
    if (ctx->config == NULL) {
        printf("Error: No se pudo leer la configuración de la memoria en %s\n", argv[1]);
        return;
    }

    t_log_level nivel_log = LOG_LEVEL_INFO;
    bool has_log_level = config_has_property(ctx->config, "LOG_LEVEL");
    if (has_log_level) {
        char* nivel_str = config_get_string_value(ctx->config, "LOG_LEVEL");
        nivel_log = log_level_from_string(nivel_str);
    }
    
    ctx->logger = log_create("KernelMemory.log", "KernelMemory", true, nivel_log);
    
    if (!has_log_level) {
        log_warning(ctx->logger, "Falta LOG_LEVEL en la configuracion, se asume INFO");
    }
}

static void validar_y_cargar_configuracion(t_memory_context* ctx) {
    if (!config_has_property(ctx->config, "PUERTO_MEMORY") ||
        !config_has_property(ctx->config, "SEGMENT_MAX_SIZE") ||
        !config_has_property(ctx->config, "ALLOCATION_STRATEGY") ||
        !config_has_property(ctx->config, "SCRIPTS_BASEPATH")) {
        log_error(ctx->logger, "Faltan claves obligatorias en el archivo de configuracion (PUERTO_MEMORY, SEGMENT_MAX_SIZE, ALLOCATION_STRATEGY, SCRIPTS_BASEPATH)");
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }
}

t_memory_context* iniciar_contexto(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Error: Faltan argumentos. Uso: %s <path_config>\n", argv[0]);
        return NULL;
    }

    t_memory_context* ctx = calloc(1, sizeof(t_memory_context));
    if (ctx == NULL) return NULL;
    ctx->socket_servidor = -1;
    ctx->scheduler_socket = -1;
    ctx->swap_socket = -1;

    inicializar_entorno(ctx, argc, argv);
    
    if (ctx->config == NULL) {
        free(ctx);
        return NULL;
    }
    
    validar_y_cargar_configuracion(ctx);
    
    ctx->programas = list_create();
    
    if (config_has_property(ctx->config, "INSTRUCTION_DELAY")) {
        ctx->instruction_delay = config_get_int_value(ctx->config, "INSTRUCTION_DELAY");
    } else {
        log_warning(ctx->logger, "Falta INSTRUCTION_DELAY, asumiendo 0");
        ctx->instruction_delay = 0;
    }

    if (config_has_property(ctx->config, "COMPACTION_DELAY")) {
        ctx->compaction_delay = config_get_int_value(ctx->config, "COMPACTION_DELAY");
    } else {
        log_warning(ctx->logger, "Falta COMPACTION_DELAY, asumiendo 0");
        ctx->compaction_delay = 0;
    }
    
    ctx->base_path = strdup(config_get_string_value(ctx->config, "SCRIPTS_BASEPATH"));
    pthread_mutex_init(&ctx->mutex_programas, NULL);
    
    ctx->espacio_memoria = NULL;
    ctx->memoria_total = 0;
    ctx->segment_max_size = config_get_int_value(ctx->config, "SEGMENT_MAX_SIZE");
    ctx->allocation_strategy = strdup(config_get_string_value(ctx->config, "ALLOCATION_STRATEGY"));
    ctx->huecos_libres = list_create();
    ctx->segmentos_globales = list_create();
    ctx->scheduler_socket = -1;
    ctx->memory_sticks = list_create();
    ctx->swap_socket = -1;
    ctx->swap_bitmap = NULL;
    ctx->bitarray_data = NULL;
    pthread_mutex_init(&ctx->mutex_memoria, NULL);
    pthread_mutex_init(&ctx->mutex_scheduler_socket, NULL);
    
    return ctx;
}

int iniciar_servidor_memoria(t_memory_context* ctx) {
    char *puerto_memory = config_get_string_value(ctx->config, "PUERTO_MEMORY");

    ctx->socket_servidor = iniciar_servidor(puerto_memory, ctx->logger);
    if(ctx->socket_servidor == -1) {
        return -1;
    }
    
    log_info(ctx->logger, "Kernel Memory listo para recibir clientes en el puerto %s", puerto_memory);
    return 0;
}

void iniciar_escucha_servidor(t_memory_context* ctx) {
    while (1) {
        int cliente_fd = esperar_cliente(ctx->socket_servidor, ctx->logger);
        if(cliente_fd == -1) continue;

        t_args_hilo_cliente* args = malloc(sizeof(t_args_hilo_cliente));
        args->cliente_fd = cliente_fd;
        args->ctx = ctx;

        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_kernel_memory, args);
        pthread_detach(hilo_cliente); 
    }
}

static void destruir_memory_stick(void* element) {
    t_memory_stick_info* stick = (t_memory_stick_info*)element;
    if (stick->ip) free(stick->ip);
    free(stick);
}

void limpiar_contexto(t_memory_context* ctx) {
    if (ctx == NULL) return;
    
    if (ctx->programas) {
        while (!list_is_empty(ctx->programas)) {
            t_programa* p = list_get(ctx->programas, 0);
            destruir_programa(ctx, p);
        }
        list_destroy(ctx->programas);
    }
    
    pthread_mutex_destroy(&ctx->mutex_programas);
    
    if (ctx->espacio_memoria) free(ctx->espacio_memoria);
    if (ctx->allocation_strategy) free(ctx->allocation_strategy);
    if (ctx->huecos_libres) list_destroy_and_destroy_elements(ctx->huecos_libres, free);
    if (ctx->segmentos_globales) list_destroy_and_destroy_elements(ctx->segmentos_globales, free);
    if (ctx->memory_sticks) list_destroy_and_destroy_elements(ctx->memory_sticks, destruir_memory_stick);
    
    pthread_mutex_destroy(&ctx->mutex_memoria);
    pthread_mutex_destroy(&ctx->mutex_scheduler_socket);
    
    if (ctx->swap_bitmap) bitarray_destroy(ctx->swap_bitmap);
    if (ctx->bitarray_data) free(ctx->bitarray_data);
    if (ctx->swap_socket != -1) close(ctx->swap_socket);
    
    if (ctx->base_path) free(ctx->base_path);
    if (ctx->config) config_destroy(ctx->config);
    if (ctx->logger) log_destroy(ctx->logger);
    if (ctx->socket_servidor != -1) close(ctx->socket_servidor);
    free(ctx);
}
