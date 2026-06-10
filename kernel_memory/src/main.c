#include "main.h"

t_log* logger;
int main(int argc, char *argv[]) {
    logger = log_create("KernelMemory.log", "KernelMemory", true, LOG_LEVEL_DEBUG);

    t_config *config = config_create("kernelMemory.config");
    if (config == NULL) {
        log_error(logger, "No se pudo leer la configuración de la memoria");
        return EXIT_FAILURE;
    }

    t_memory_context* ctx = malloc(sizeof(t_memory_context));
    ctx->programas = list_create();
    ctx->instruction_delay = config_get_int_value(config, "INSTRUCTION_DELAY");
    ctx->base_path = strdup(config_get_string_value(config, "SCRIPTS_BASEPATH"));
    pthread_mutex_init(&ctx->mutex_programas, NULL);

    char *puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");

    int server_fd = iniciar_servidor(puerto_memory, logger);
    if(server_fd == -1) {
        return EXIT_FAILURE;
    }
    
    log_info(logger, "Kernel Memory listo para recibir clientes en el puerto %s", puerto_memory);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd, logger);
        if(cliente_fd == -1) continue;

        t_args_hilo_cliente* args = malloc(sizeof(t_args_hilo_cliente));
        args->cliente_fd = cliente_fd;
        args->ctx = ctx;

        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, (void*)atender_cliente, args);
        pthread_detach(hilo_cliente); 
    }
    
    // Unreachable, but for completeness
    config_destroy(config);
    return 0;
}