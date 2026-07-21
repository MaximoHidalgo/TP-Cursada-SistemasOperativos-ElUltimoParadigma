#include "kernel_memory_init.h"
#include "atender_cliente_kernel_memory.h"
#include <utils/sockets/sockets.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

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
        pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_kernel_memory, args);
        pthread_detach(hilo_cliente); 
    }
}

void limpiar_contexto(t_memory_context* ctx) {
    if (ctx == NULL) return;
    
    if (ctx->programas) {
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
