#include "io_utils.h"

static bool es_tipo_valido(char* io_name) {
    return string_equals_ignore_case(io_name, "SLEEP")
        || string_equals_ignore_case(io_name, "STDIN")
        || string_equals_ignore_case(io_name, "STDOUT");
}

static bool inicializar_entorno(t_io_context* ctx, char* path_config) {
    if (!es_tipo_valido(ctx->io_name)) {
        printf("Tipo de interfaz invalido: %s. Debe ser SLEEP, STDIN o STDOUT\n", ctx->io_name);
        return false;
    }

    ctx->config = config_create(path_config);
    if (ctx->config == NULL){
        printf("No se pudo leer el archivo de config del modulo IO (%s)\n", path_config);
        return false;
    }

    char* log_level_str = config_get_string_value(ctx->config, "LOG_LEVEL");
    t_log_level nivelLog = log_level_from_string(log_level_str);
    ctx->logger = log_create("io.log", ctx->io_name, true, nivelLog);

    if (ctx->logger == NULL){
        printf("No se pudo crear el logger en modulo IO\n");
        return false;
    }
    
    return true;
}

static bool conectar_kernel_scheduler(t_io_context* ctx) {
    char* ipKernel = config_get_string_value(ctx->config, "IP_SCHEDULER");
    char* puertoKernel = config_get_string_value(ctx->config, "PUERTO_SCHEDULER");

    ctx->socket_scheduler = crear_conexion(ipKernel, puertoKernel);

    if(ctx->socket_scheduler == -1){
        log_error(ctx->logger, "No se pudo establecer conexion con el Kernel Scheduler\n");
        return false;
    }

    log_info(ctx->logger, "## Conectado a Kernel Scheduler");
    enviar_mensaje(ctx->io_name, ctx->socket_scheduler);
    log_info(ctx->logger, "Interfaz %s lista.", ctx->io_name);
    
    return true;
}

t_io_context* iniciar_contexto(int argc, char* argv[]) {
    if (argc != 3){
        printf("Error en parametros. Uso: ./io <path_config> <nombre_io>\n");
        return NULL;
    }

    t_io_context* ctx = malloc(sizeof(t_io_context));
    if (ctx == NULL) return NULL;

    ctx->io_name = strdup(argv[2]);
    ctx->socket_scheduler = -1;
    ctx->logger = NULL;
    ctx->config = NULL;

    if (!inicializar_entorno(ctx, argv[1])) {
        limpiar_contexto(ctx);
        return NULL;
    }

    if (!conectar_kernel_scheduler(ctx)) {
        limpiar_contexto(ctx);
        return NULL;
    }

    return ctx;
}

void atender_kernel_scheduler(t_io_context* ctx) {
    while (1) {
        int cod_op = recibir_operacion(ctx->socket_scheduler);
        
        switch(cod_op) {
            case PETICION_IO:
                procesar_peticion_io(ctx);
                break;
            case -1:
                log_error(ctx->logger, "Se desconectó el Kernel Scheduler.");
                return;
            default:
                log_warning(ctx->logger, "Operación desconocida: %d", cod_op);
                break;
        }
    }
}

void limpiar_contexto(t_io_context* ctx) {
    if (ctx == NULL) return;
    
    if (ctx->socket_scheduler != -1) liberar_conexion(ctx->socket_scheduler);
    if (ctx->logger) log_destroy(ctx->logger);
    if (ctx->config) config_destroy(ctx->config);
    if (ctx->io_name) free(ctx->io_name);
    
    free(ctx);
}
