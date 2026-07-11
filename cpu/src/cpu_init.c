#include "cpu_init.h"

// -------------------------------------------------------------
// PRIVATE FUNCTIONS
// -------------------------------------------------------------

static bool inicializar_entorno(t_cpu_context* ctx, char* path_config) {
    ctx->config = config_create(path_config);
    if (ctx->config == NULL) {
        // Fallback logger for errors if config doesn't exist
        ctx->logger = log_create(ctx->id_cpu, ctx->id_cpu, true, LOG_LEVEL_INFO);
        log_error(ctx->logger, "No se pudo leer el archivo de configuracion");
        return false;
    }

    char* nivel_str = config_get_string_value(ctx->config, "LOG_LEVEL");
    t_log_level nivel;
    if (nivel_str != NULL) {
        nivel = log_level_from_string(nivel_str);
    } else {
        nivel = LOG_LEVEL_INFO;
    }

    char* nombre_log = string_from_format("%s.log", ctx->id_cpu);
    ctx->logger = log_create(nombre_log, ctx->id_cpu, true, nivel);
    free(nombre_log);

    return true;
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

static void procesar_pid_a_ejecutar(t_cpu_context* ctx) {
    t_list* paquete = recibir_paquete(ctx->conexiones->scheduler);
    if (!paquete || list_is_empty(paquete)) {
        log_error(ctx->logger, "Paquete PID_A_EJECUTAR invalido o vacio");
        if(paquete) list_destroy_and_destroy_elements(paquete, free);
        return;
    }
    int pid_recibido = *(int*)list_get(paquete, 0);
    list_destroy_and_destroy_elements(paquete, free);

    manejar_ejecucion_proceso(pid_recibido, ctx);
}

// -------------------------------------------------------------
// PUBLIC FUNCTIONS
// -------------------------------------------------------------

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