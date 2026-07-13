#include "main.h"
#include "auxiliares.h"

// Functions moved to auxiliares.c

int main(int argc, char *argv[]) {
    t_memory_context* ctx = iniciar_contexto(argc, argv);
    if (ctx == NULL) {
        return EXIT_FAILURE;
    }

    if (iniciar_servidor_memoria(ctx) == -1) {
        limpiar_contexto(ctx);
        return EXIT_FAILURE;
    }

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
    
    limpiar_contexto(ctx);
    return EXIT_SUCCESS;
}