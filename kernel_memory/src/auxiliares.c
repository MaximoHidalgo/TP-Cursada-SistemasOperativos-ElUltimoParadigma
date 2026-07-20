#include "auxiliares.h"
#include "procesador_mensajes.h"
#include "auxiliares_memoria.h"

static void conectar_memory_stick(int cliente_fd, t_memory_context* ctx, char* mensaje) {
    char **partes = string_split(mensaje, "|");
    uint32_t stick_size = atoi(partes[1]);
    char* stick_ip = strdup(partes[2]);
    int stick_puerto = atoi(partes[3]);
    
    pthread_mutex_lock(&ctx->mutex_memoria);
    
    t_memory_stick_info* stick = malloc(sizeof(t_memory_stick_info));
    stick->socket_fd = cliente_fd;
    stick->tamanio = stick_size;
    stick->base_address = ctx->memoria_total;
    stick->id = list_size(ctx->memory_sticks) + 1;
    stick->ip = stick_ip;
    stick->puerto = stick_puerto;
    
    list_add(ctx->memory_sticks, stick);
    
    t_hueco_libre* nuevo_hueco = malloc(sizeof(t_hueco_libre));
    nuevo_hueco->base = ctx->memoria_total;
    nuevo_hueco->limite = ctx->memoria_total + stick_size;
    list_add(ctx->huecos_libres, nuevo_hueco);
    fusionar_huecos_libres(ctx->huecos_libres);
    
    ctx->memoria_total += stick_size;
    // Log obligatorio del TP (strict format)
    log_info(ctx->logger, "## Memory Stick de %d bytes Conectada", stick_size);
    log_info(ctx->logger, "Memory Stick registrada en %s:%d. Memoria total actual: %d bytes", stick_ip, stick_puerto, ctx->memoria_total);
    
    pthread_mutex_unlock(&ctx->mutex_memoria);

    pthread_mutex_lock(&ctx->mutex_scheduler_socket);
    notificar_memoria_expandida(ctx->scheduler_socket);
    pthread_mutex_unlock(&ctx->mutex_scheduler_socket);
    
    string_array_destroy(partes);
}

static void conectar_cpu(int cliente_fd, t_memory_context* ctx, char* mensaje) {
    log_info(ctx->logger, "## CPU %s Conectada", mensaje);
    
    pthread_mutex_lock(&ctx->mutex_memoria);
    t_paquete* paq = crear_paquete();
    paq->codigo_operacion = ENVIO_TABLA_STICKS;
    int cant = list_size(ctx->memory_sticks);
    agregar_a_paquete(paq, &cant, sizeof(int));
    for (int i = 0; i < cant; i++) {
        t_memory_stick_info* stick = list_get(ctx->memory_sticks, i);
        agregar_a_paquete(paq, &(stick->id), sizeof(int));
        agregar_a_paquete(paq, &(stick->tamanio), sizeof(uint32_t));
        agregar_a_paquete(paq, &(stick->base_address), sizeof(uint32_t));
        agregar_a_paquete(paq, &(stick->puerto), sizeof(int));
        int ip_len = strlen(stick->ip) + 1;
        agregar_a_paquete(paq, &ip_len, sizeof(int));
        agregar_a_paquete(paq, stick->ip, ip_len);
    }
    enviar_paquete(paq, cliente_fd);
    eliminar_paquete(paq);
    pthread_mutex_unlock(&ctx->mutex_memoria);
}

static void conectar_swap(int cliente_fd, t_memory_context* ctx, char* mensaje) {
    char **partes = string_split(mensaje, "|");
    ctx->swap_file_size = atoi(partes[1]);
    ctx->swap_block_size = atoi(partes[2]);
    ctx->swap_socket = cliente_fd;
    
    int cant_bloques = ctx->swap_file_size / ctx->swap_block_size;
    int bits_bytes = (cant_bloques + 7) / 8;
    ctx->bitarray_data = malloc(bits_bytes);
    memset(ctx->bitarray_data, 0, bits_bytes);
    ctx->swap_bitmap = bitarray_create_with_mode(ctx->bitarray_data, bits_bytes, LSB_FIRST);
    
    log_info(ctx->logger, "## SWAP Conectado. Tamaño: %d, Tamaño Bloque: %d, Total Bloques: %d", ctx->swap_file_size, ctx->swap_block_size, cant_bloques);
    string_array_destroy(partes);
}

bool procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx) {
    char *mensaje = recibir_handshake(cliente_fd);
    bool should_continue = true;
    
    if (string_starts_with(mensaje, "STICK|")) {
        conectar_memory_stick(cliente_fd, ctx, mensaje);
        should_continue = false;
    }
    else if (string_equals_ignore_case(mensaje, HANDSHAKE_SCHEDULER)) {
        ctx->scheduler_socket = cliente_fd;
        log_info(ctx->logger, "## Kernel Scheduler Conectado - FD del socket: %d", cliente_fd);
    }
    else if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        conectar_cpu(cliente_fd, ctx, mensaje);
    }
    else if (string_starts_with(mensaje, "SWAP|")) {
        conectar_swap(cliente_fd, ctx, mensaje);
        should_continue = false;
    }
    else { 
        log_info(ctx->logger, "## Módulo Conectado: %s", mensaje);
    }
    free(mensaje);
    return should_continue;
}

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

void destruir_programa(t_memory_context* ctx, t_programa* prog) {
    if (prog->contexto->tabla_segmentos != NULL) {
        pthread_mutex_lock(&ctx->mutex_programas);
        for (int i = 0; i < list_size(prog->contexto->tabla_segmentos); i++) {
            t_segmento* seg = list_get(prog->contexto->tabla_segmentos, i);
            eliminar_segmento(ctx, prog->pid, seg);
        }
        list_clean(prog->contexto->tabla_segmentos);
        pthread_mutex_unlock(&ctx->mutex_programas);
    }

    pthread_mutex_lock(&ctx->mutex_programas);
    for (int i = 0; i < list_size(ctx->programas); i++) {
        if (list_get(ctx->programas, i) == prog) {
            list_remove(ctx->programas, i);
            break;
        }
    }
    pthread_mutex_unlock(&ctx->mutex_programas);

    if (prog->instrucciones != NULL) {
        for (int i = 0; i < prog->cantidad_instrucciones; i++) {
            free(prog->instrucciones[i]);
        }
        free(prog->instrucciones);
    }

    if (prog->contexto != NULL) {
        if (prog->contexto->tabla_segmentos != NULL) {
            list_destroy(prog->contexto->tabla_segmentos);
        }
        free(prog->contexto);
    }

    free(prog);
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