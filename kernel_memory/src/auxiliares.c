#include "auxiliares.h"
#include "procesador_mensajes.h"

t_programa* buscar_programa(t_memory_context* ctx, int pid) {
    pthread_mutex_lock(&ctx->mutex_programas);
    for (int i = 0; i < list_size(ctx->programas); i++) {
        t_programa* prog = list_get(ctx->programas, i);
        if (prog->pid == pid) {
            pthread_mutex_unlock(&ctx->mutex_programas);
            return prog;
        }
    }
    pthread_mutex_unlock(&ctx->mutex_programas);
    return NULL;
}

char** leer_lineas(FILE* f, int* cantidad) {
    char** lineas = NULL;
    char buffer[256];
    int count = 0;

    while (fgets(buffer, sizeof(buffer), f)) {
        lineas = realloc(lineas, sizeof(char*) * (count + 1));
        char* linea = strdup(buffer);
        string_trim(&linea);
        lineas[count] = linea;
        count++;
    }
    *cantidad = count;
    return lineas;
}

void crear_proceso(t_memory_context* ctx, int pid, char* path) {
    FILE* f = fopen(path, "r");

    if (!f) {
        // Fallback for tests
        char fallback_path[512];
        snprintf(fallback_path, sizeof(fallback_path), "../utils/scripts/%s", path);
        f = fopen(fallback_path, "r");
        
        if (!f) {
            log_error(ctx->logger, "No se pudo abrir el archivo %s ni %s", path, fallback_path);
            return;
        }
    }

    t_programa* prog = malloc(sizeof(t_programa));
    prog->pid = pid;
    prog->instrucciones = leer_lineas(f, &prog->cantidad_instrucciones);
    fclose(f);

    prog->contexto = malloc(sizeof(t_contexto_ejecucion));
    prog->contexto->pid = pid;
    memset(&(prog->contexto->registros), 0, sizeof(t_registros));
    prog->contexto->tabla_segmentos = list_create();

    pthread_mutex_lock(&ctx->mutex_programas);
    list_add(ctx->programas, prog);
    pthread_mutex_unlock(&ctx->mutex_programas);

    // Log obligatorio del TP (strict format)
    log_info(ctx->logger, "## PID: %d - Proceso Creado", pid);
    log_info(ctx->logger, "Se leyeron %d instrucciones para el proceso %d", prog->cantidad_instrucciones, pid);
}

char* obtener_instruccion(t_memory_context* ctx, int pid, int pc) {
    t_programa* prog = buscar_programa(ctx, pid);

    if (!prog || pc >= prog->cantidad_instrucciones)
        return strdup("EXIT");

    usleep(ctx->instruction_delay * 1000);

    char* instruccion = strdup(prog->instrucciones[pc]);

    log_info(ctx->logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s",
             pid, pc, instruccion);

    return instruccion;
}

void inicializar_entorno(t_memory_context* ctx, int argc, char* argv[]) {
    ctx->config = config_create(argv[1]);
    if (ctx->config == NULL) {
        printf("Error: No se pudo leer la configuración de la memoria en %s\n", argv[1]);
        return;
    }

    t_log_level nivel_log = LOG_LEVEL_DEBUG;
    if (config_has_property(ctx->config, "LOG_LEVEL")) {
        char* nivel_str = config_get_string_value(ctx->config, "LOG_LEVEL");
        nivel_log = log_level_from_string(nivel_str);
    }
    
    ctx->logger = log_create("KernelMemory.log", "KernelMemory", true, nivel_log);
}

t_memory_context* iniciar_contexto(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Error: Faltan argumentos. Uso: %s <path_config>\n", argv[0]);
        return NULL;
    }

    t_memory_context* ctx = malloc(sizeof(t_memory_context));
    if (ctx == NULL) return NULL;

    inicializar_entorno(ctx, argc, argv);
    
    if (ctx->config == NULL) {
        free(ctx);
        return NULL;
    }
    
    ctx->programas = list_create();
    ctx->instruction_delay = config_get_int_value(ctx->config, "INSTRUCTION_DELAY");
    ctx->compaction_delay = config_get_int_value(ctx->config, "COMPACTION_DELAY");
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
        pthread_create(&hilo_cliente, NULL, (void*)atender_cliente, args);
        pthread_detach(hilo_cliente); 
    }
}

void limpiar_contexto(t_memory_context* ctx) {
    if (ctx == NULL) return;
    
    if (ctx->programas) {
        // En un futuro se puede iterar y liberar cada programa
        list_destroy(ctx->programas);
    }
    
    pthread_mutex_destroy(&ctx->mutex_programas);
    
    if (ctx->espacio_memoria) free(ctx->espacio_memoria);
    if (ctx->allocation_strategy) free(ctx->allocation_strategy);
    if (ctx->huecos_libres) list_destroy_and_destroy_elements(ctx->huecos_libres, free);
    if (ctx->segmentos_globales) list_destroy_and_destroy_elements(ctx->segmentos_globales, free);
    pthread_mutex_destroy(&ctx->mutex_memoria);
    
    if (ctx->swap_bitmap) bitarray_destroy(ctx->swap_bitmap);
    if (ctx->bitarray_data) free(ctx->bitarray_data);
    if (ctx->swap_socket != -1) close(ctx->swap_socket);
    
    if (ctx->base_path) free(ctx->base_path);
    if (ctx->config) config_destroy(ctx->config);
    if (ctx->logger) log_destroy(ctx->logger);
    if (ctx->socket_servidor != -1) close(ctx->socket_servidor);
    free(ctx);
}