#include "main.h"

t_io_context* iniciar_contexto(int argc, char* argv[]) {
    if (argc != 3){
        printf("Error en parametros. Uso: ./io <path_config> <nombre_io>\n");
        return NULL;
    }

    t_io_context* ctx = malloc(sizeof(t_io_context));
    if (ctx == NULL) return NULL;

    ctx->io_name = strdup(argv[2]);
    ctx->config = config_create(argv[1]);
    
    if (ctx->config == NULL){
        printf("No se pudo leer el archivo de config del modulo IO (%s)\n", argv[1]);
        free(ctx->io_name);
        free(ctx);
        return NULL;
    }

    char* ipKernel = config_get_string_value(ctx->config, "IP_SCHEDULER");
    char* puertoKernel = config_get_string_value(ctx->config, "PUERTO_SCHEDULER");
    char* log_level_str = config_get_string_value(ctx->config, "LOG_LEVEL");
    
    t_log_level nivelLog = log_level_from_string(log_level_str);
    ctx->logger = log_create("io.log", ctx->io_name, true, nivelLog);

    if (ctx->logger == NULL){
        printf("No se pudo crear el logger en modulo IO\n");
        config_destroy(ctx->config);
        free(ctx->io_name);
        free(ctx);
        return NULL;
    }

    ctx->socket_scheduler = crear_conexion(ipKernel, puertoKernel);

    if(ctx->socket_scheduler == -1){
        log_error(ctx->logger, "No se pudo establecer conexion con el Kernel Scheduler\n");
        log_destroy(ctx->logger);
        config_destroy(ctx->config);
        free(ctx->io_name);
        free(ctx);
        return NULL;
    }

    log_info(ctx->logger, "## Conectado a Kernel Scheduler");
    enviar_mensaje(ctx->io_name, ctx->socket_scheduler);
    log_info(ctx->logger, "Interfaz %s lista.", ctx->io_name);
    
    return ctx;
}

void limpiar_contexto(t_io_context* ctx) {
    if (ctx == NULL) return;
    
    if (ctx->socket_scheduler != -1) {
        liberar_conexion(ctx->socket_scheduler);
    }
    if (ctx->logger) log_destroy(ctx->logger);
    if (ctx->config) config_destroy(ctx->config);
    if (ctx->io_name) free(ctx->io_name);
    
    free(ctx);
}

int main(int argc, char* argv[]) {
    t_io_context* ctx = iniciar_contexto(argc, argv);
    if (ctx == NULL) {
        return EXIT_FAILURE;
    }

    while (1) {
        int cod_op = recibir_operacion(ctx->socket_scheduler);
        
        switch(cod_op) {
            case PETICION_IO:
                procesar_peticion_io(ctx);
                break;
            case -1:
                log_error(ctx->logger, "Se desconectó el Kernel Scheduler.");
                limpiar_contexto(ctx);
                return EXIT_FAILURE;
            default:
                log_warning(ctx->logger, "Operación desconocida: %d", cod_op);
                break;
        }
    }

    limpiar_contexto(ctx);
    return 0;
}
