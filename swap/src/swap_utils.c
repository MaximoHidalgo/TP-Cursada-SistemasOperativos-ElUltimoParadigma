#include "swap_utils.h"

static bool inicializar_entorno(t_swap_context* ctx, char* path_config) {
    ctx->config = config_create(path_config);
    if (ctx->config == NULL) {
        printf("Error: No se pudo leer el config en %s\n", path_config);
        return false;
    }
    
    t_log_level level = LOG_LEVEL_INFO;
    bool has_log = config_has_property(ctx->config, "LOG_LEVEL");
    if (has_log) {
        char* log_level_str = config_get_string_value(ctx->config, "LOG_LEVEL");
        level = log_level_from_string(log_level_str);
    }
    
    ctx->logger = log_create("swap.log", "SWAP", true, level);
    
    if (!has_log) {
        log_warning(ctx->logger, "Falta LOG_LEVEL, asumiendo INFO");
    }

    ctx->ip_memory = config_get_string_value(ctx->config, "IP_MEMORY");
    ctx->puerto_memory = config_get_string_value(ctx->config, "PUERTO_MEMORY");
    ctx->swap_file_path = config_get_string_value(ctx->config, "SWAP_FILE_PATH");
    ctx->swap_file_size = config_get_int_value(ctx->config, "SWAP_FILE_SIZE");
    ctx->block_size = config_get_int_value(ctx->config, "BLOCK_SIZE");
    
    return true;
}

static void validar_y_cargar_configuracion(t_swap_context* ctx) {
    if (!config_has_property(ctx->config, "IP_MEMORY") ||
        !config_has_property(ctx->config, "PUERTO_MEMORY") ||
        !config_has_property(ctx->config, "SWAP_FILE_PATH") ||
        !config_has_property(ctx->config, "SWAP_FILE_SIZE") ||
        !config_has_property(ctx->config, "BLOCK_SIZE")) {
        log_error(ctx->logger, "Faltan claves obligatorias en el archivo de configuracion (IP/PUERTO_MEMORY, SWAP_FILE_PATH, SWAP_FILE_SIZE, BLOCK_SIZE)");
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }
}

t_swap_context* iniciar_contexto(int argc, char* argv[]) {
    t_swap_context* ctx = calloc(1, sizeof(t_swap_context));
    if (!ctx) return NULL;
    
    char* config_path = "swap.config";
    if (argc > 1) {
        config_path = argv[1];
    }
    
    ctx->swap_file = NULL;
    ctx->socket_kernel_memory = -1;
    
    if (!inicializar_entorno(ctx, config_path)) {
        free(ctx);
        exit(EXIT_FAILURE);
    }

    validar_y_cargar_configuracion(ctx);

    return ctx;
}

void crear_archivo_swap(t_swap_context* ctx) {
    ctx->swap_file = fopen(ctx->swap_file_path, "wb+");
    
    if (ctx->swap_file == NULL) {
        log_error(ctx->logger, "No se pudo crear el archivo de SWAP");
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }

    // Inicializamos el archivo truncándolo al tamaño especificado
    fseek(ctx->swap_file, ctx->swap_file_size - 1, SEEK_SET);
    fputc('\0', ctx->swap_file);
}

void conectar_kernel_memory(t_swap_context* ctx) {
    ctx->socket_kernel_memory = crear_conexion(ctx->ip_memory, ctx->puerto_memory);

    if (ctx->socket_kernel_memory == -1) {
        log_error(ctx->logger, "No se pudo conectar a Kernel Memory");
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }
    
    log_info(ctx->logger, "## Conectado a Kernel Memory");

    // Handshake
    char* msj_handshake = string_from_format("SWAP|%d|%d", ctx->swap_file_size, ctx->block_size);
    enviar_mensaje(msj_handshake, ctx->socket_kernel_memory);
    free(msj_handshake);
    
    log_info(ctx->logger, "Mensaje de handshake enviado a Kernel Memory correctamente");
}

void atender_kernel_memory(t_swap_context* ctx) {
    while(1) {
        log_info(ctx->logger, "Esperando peticiones de Kernel Memory...");
        int cod_op = recibir_operacion(ctx->socket_kernel_memory);
        
        switch(cod_op) {
             case LECTURA:
                  procesar_peticion_lectura(ctx);
                  break;
             case ESCRITURA:
                  procesar_peticion_escritura(ctx);
                  break;
             case -1:
                  log_error(ctx->logger, "Desconexión de Kernel Memory");
                  return; // Sale del bucle para ir a limpiar_contexto()
             default:
                  log_warning(ctx->logger, "Operacion desconocida: %d", cod_op);
                  break;
         }
    }
}

void limpiar_contexto(t_swap_context* ctx) {
    if (ctx->swap_file != NULL) {
        fclose(ctx->swap_file); 
    }
    if (ctx->socket_kernel_memory != -1) {
        close(ctx->socket_kernel_memory);
    }
    if (ctx->config != NULL) {
        config_destroy(ctx->config);
    }
    if (ctx->logger != NULL) {
        log_destroy(ctx->logger);
    }
    free(ctx);
}