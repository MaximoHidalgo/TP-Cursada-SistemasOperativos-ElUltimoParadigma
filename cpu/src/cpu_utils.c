#include "cpu_utils.h"

void manejar_ejecucion_proceso(int pid_recibido, t_cpu_context* ctx) {
    log_info(ctx->logger, "## PID recibido para ejecutar: %d", pid_recibido);

    t_contexto_ejecucion* contexto = pedir_contexto_a_memoria(pid_recibido, ctx);

    if (contexto != NULL) {
        ejecutar_ciclo_de_instruccion(contexto, ctx);
        free(contexto);
    } else {
        log_error(ctx->logger, "Memoria devolvió un contexto nulo para el PID %d", pid_recibido);
    }
}

static bool inicializar_entorno(t_cpu_context* ctx, char* path_config) {
    char* nombre_log = string_from_format("%s.log", ctx->id_cpu);
    ctx->logger = log_create(nombre_log, "CPU", true, LOG_LEVEL_INFO);
    free(nombre_log);

    ctx->config = config_create(path_config);
    if (ctx->config == NULL) {
        log_error(ctx->logger, "No se pudo leer el archivo de configuracion");
        return false;
    }
    return true;
}

static void procesar_tabla_de_sticks(t_cpu_context* ctx, void* buffer, int size) {
    t_buffer_reader* reader = buffer_reader_create(buffer, size);
    
    int size_payload_cant = buffer_read_int(reader); (void)size_payload_cant;
    int cant = buffer_read_int(reader);

    for (int i = 0; i < cant; i++) {
        t_memory_stick_info* stick = malloc(sizeof(t_memory_stick_info));
        
        int size_id = buffer_read_int(reader); (void)size_id;
        stick->id = buffer_read_int(reader);

        int size_tamanio = buffer_read_int(reader); (void)size_tamanio;
        stick->tamanio = buffer_read_uint32(reader);

        int size_base_address = buffer_read_int(reader); (void)size_base_address;
        stick->base_address = buffer_read_uint32(reader);

        int size_puerto = buffer_read_int(reader); (void)size_puerto;
        stick->puerto = buffer_read_int(reader);

        int size_ip_len = buffer_read_int(reader); (void)size_ip_len;
        int ip_len = buffer_read_int(reader); (void)ip_len;

        stick->ip = buffer_read_string(reader);

        char* str_puerto = string_itoa(stick->puerto);
        stick->socket_fd = crear_conexion(stick->ip, str_puerto); 
        free(str_puerto);

        if (stick->socket_fd != -1) {
            enviar_mensaje(ctx->id_cpu, stick->socket_fd);
            list_add(ctx->memory_sticks, stick);
            log_info(ctx->logger, "Conectado a Memory Stick ID %d en %s:%d", stick->id, stick->ip, stick->puerto);
        } else {
            log_error(ctx->logger, "Error al conectar con Memory Stick ID %d", stick->id);
        }
    }
    buffer_reader_destroy(reader);
}

static void conectar_con_memoria(t_cpu_context* ctx, char* ip, char* puerto) {
    log_info(ctx->logger, "Intentando conectar Kernel Memory...");
    ctx->conexiones->memoria = crear_conexion(ip, puerto);
    if (ctx->conexiones->memoria != -1) {
        log_info(ctx->logger, "## Conectado a Kernel Memory");
        enviar_mensaje(ctx->id_cpu, ctx->conexiones->memoria);

        int op = SOLICITUD_TAM_MAX_SEGMENTO;
        send(ctx->conexiones->memoria, &op, sizeof(int), 0);
        
        int recibidos = 0;
        while (recibidos < 2) {
            int resp = recibir_operacion(ctx->conexiones->memoria);
            if (resp == TAM_MAX_SEGMENTO) {
                t_list* paq = recibir_paquete(ctx->conexiones->memoria);
                int* val = list_get(paq, 0);
                ctx->tam_max_segmento = *val;
                list_destroy_and_destroy_elements(paq, free);
                log_info(ctx->logger, "Tamaño maximo de segmento recibido: %d", ctx->tam_max_segmento);
                recibidos++;
            } else if (resp == ENVIO_TABLA_STICKS) {
                int size;
                void* buf = recibir_buffer(&size, ctx->conexiones->memoria);
                procesar_tabla_de_sticks(ctx, buf, size);
                free(buf);
                recibidos++;
            } else {
                log_error(ctx->logger, "Operacion no esperada en handshake de memory: %d", resp);
                break;
            }
        }
    } else {
        log_error(ctx->logger, "Error al conectar a Kernel Memory");
    }
}

static void conectar_con_scheduler(t_cpu_context* ctx, char* ip, char* puerto) {
    log_info(ctx->logger, "Intentando conectar Kernel Scheduler...");
    ctx->conexiones->scheduler = crear_conexion(ip, puerto);
    if (ctx->conexiones->scheduler != -1) {
        log_info(ctx->logger, "## Conectado a Kernel Scheduler");
        enviar_mensaje(ctx->id_cpu, ctx->conexiones->scheduler);
    } else {
        log_error(ctx->logger, "Error al conectar a Kernel Scheduler");
    }
}

t_cpu_context* iniciar_contexto(int argc, char* argv[]) {
    validar_argumentos(argc);

    t_cpu_context* ctx = malloc(sizeof(t_cpu_context));
    if (!ctx) return NULL;

    ctx->id_cpu = strdup(argv[2]);
    char* path_config = argv[1];

    printf("Se arranco el CPU correctamente con el id: %s y el path del config: %s\n", ctx->id_cpu, path_config);

    if (!inicializar_entorno(ctx, path_config)) {
        free(ctx->id_cpu);
        free(ctx);
        return NULL;
    }

    ctx->conexiones = malloc(sizeof(t_conexiones_cpu));
    ctx->conexiones->sticks = -1;
    ctx->memory_sticks = list_create();

    char* ip_memory = config_get_string_value(ctx->config, "IP_MEMORY");
    char* puerto_memory = config_get_string_value(ctx->config, "PUERTO_MEMORY");
    conectar_con_memoria(ctx, ip_memory, puerto_memory);

    char* ip_scheduler = config_get_string_value(ctx->config, "IP_SCHEDULER");
    char* puerto_scheduler = config_get_string_value(ctx->config, "PUERTO_SCHEDULER");
    conectar_con_scheduler(ctx, ip_scheduler, puerto_scheduler);

    return ctx;
}

void limpiar_contexto(t_cpu_context* ctx) {
    if (!ctx) return;
    
    // Si la limpieza vieja existe, la reusamos.
    if (ctx->conexiones) {
        limpieza_programa(ctx->conexiones, ctx->logger, ctx->config);
    } else {
        if (ctx->logger) log_destroy(ctx->logger);
        if (ctx->config) config_destroy(ctx->config);
    }
    
    if (ctx->id_cpu) free(ctx->id_cpu);
    free(ctx);
}

static void procesar_pid_a_ejecutar(t_cpu_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, ctx->conexiones->scheduler);
    
    t_buffer_reader* reader = buffer_reader_create(buffer, size);
    int size_payload = buffer_read_int(reader); // ignoramos el payload size
    (void)size_payload;
    int pid_recibido = buffer_read_int(reader);
    
    buffer_reader_destroy(reader);
    free(buffer);

    manejar_ejecucion_proceso(pid_recibido, ctx);
}

void atender_kernel_scheduler(t_cpu_context* ctx) {
    while (1) {
        log_info(ctx->logger, "Esperando un nuevo PID del Kernel Scheduler...");
        int process_ID = recibir_operacion(ctx->conexiones->scheduler);
        
        switch(process_ID) {
            case PID_A_EJECUTAR:
                procesar_pid_a_ejecutar(ctx);
                break;
            case -1:
                log_error(ctx->logger, "El Kernel Scheduler se desconectó inesperadamente.");
                printf("Termino la CPU y se cerro correctamente\n");
                return;
            default:
                log_warning(ctx->logger, "Operación desconocida recibida del Scheduler: %d", process_ID);
                return;
        }
    }
}
