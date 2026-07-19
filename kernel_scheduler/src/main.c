#include "main.h"

int main(int argc, char* argv[]) {
    t_scheduler_context* ctx = iniciar_contexto(argc, argv);

    conectar_kernel_memory(ctx);
    iniciar_planificacion(ctx);
    iniciar_servidor_clientes(ctx);

    limpiar_contexto(ctx);
    return EXIT_SUCCESS;
}

void conectar_kernel_memory(t_scheduler_context* ctx) {
    ctx->socket_memoria = crear_conexion(ctx->ip_memory, ctx->puerto_memory);
    if(ctx->socket_memoria != -1) {
        log_info(ctx->logger, "## Conectado a Kernel Memory");
        enviar_mensaje(HANDSHAKE_SCHEDULER, ctx->socket_memoria);
        ctx->estratega->socket_memoria = ctx->socket_memoria;
    } else {
        log_error(ctx->logger, "Error al conectar con Kernel Memory en %s:%s", ctx->ip_memory, ctx->puerto_memory);
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }
}


