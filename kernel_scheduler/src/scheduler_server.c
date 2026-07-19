#include "scheduler_server.h"

int preparar_servidor(char*puerto,t_log*logger,t_scheduler_context* ctx){
    int socket_escucha = iniciar_servidor(puerto,logger);
    if (socket_escucha == -1) {
        log_error(logger, "No se pudo iniciar el servidor en el puerto %s", puerto);
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }
    log_info(ctx->logger, "Kernel Scheduler listo para recibir clientes en el puerto %s", ctx->puerto_escucha);
    return socket_escucha;
}


int esperar_cliente_scheduler(int socket_servidor, t_log* logger){
    int cliente_fd;
    do cliente_fd = esperar_cliente(socket_servidor,logger); while (cliente_fd==-1);
    return cliente_fd;
}


/*
typedef struct {
    int cliente_fd;
    t_scheduler_context* ctx;
} t_args_hilo_scheduler;
 */
t_args_hilo_scheduler* crear_argumento_planificador(int socket_cliente,t_scheduler_context* contexto_planificacion){
    t_args_hilo_scheduler* argumento_cliente = malloc(sizeof(t_args_hilo_scheduler));
    if (!argumento_cliente) {
        log_error(contexto_planificacion->logger, "No se pudo asignar memoria para hilo cliente.");
        return NULL;
    }
    argumento_cliente->cliente_fd = socket_cliente;
    argumento_cliente ->ctx = contexto_planificacion;
    return argumento_cliente;
}


void iniciar_servidor_clientes(t_scheduler_context* ctx) {
    
    ctx->socket_escucha = preparar_servidor(ctx->puerto_escucha,ctx->logger,ctx);

    while (1) {
        int socket_cliente = esperar_cliente_scheduler(ctx->socket_escucha, ctx->logger);
        t_args_hilo_scheduler* argumento_cliente = crear_argumento_planificador(socket_cliente,ctx);

        pthread_t hilo_cliente;
        if (pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_scheduler, argumento_cliente) != 0) {
            log_error(ctx->logger, "Fallo al crear hilo cliente");
            free(argumento_cliente);
            close(socket_cliente);
        } else {
            pthread_detach(hilo_cliente);
        }
    }
}


/*  bloque real
void iniciar_servidor_clientes(t_scheduler_context* ctx) {
    ctx->socket_escucha = iniciar_servidor(ctx->puerto_escucha, ctx->logger);
    if (ctx->socket_escucha == -1) {
        log_error(ctx->logger, "No se pudo iniciar el servidor en el puerto %s", ctx->puerto_escucha);
        limpiar_contexto(ctx);
        exit(EXIT_FAILURE);
    }

    log_info(ctx->logger, "Kernel Scheduler listo para recibir clientes en el puerto %s", ctx->puerto_escucha);

    while (1) {
        int cliente_fd = esperar_cliente(ctx->socket_escucha, ctx->logger);
        if (cliente_fd == -1) continue;
        
        t_args_hilo_scheduler* args = malloc(sizeof(t_args_hilo_scheduler));
        args->cliente_fd = cliente_fd;
        args->ctx = ctx;

        pthread_t hilo_cliente;
        if (pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_scheduler, args) != 0) {
            log_error(ctx->logger, "Fallo al crear hilo cliente");
            free(args);
            close(cliente_fd);
        } else {
            pthread_detach(hilo_cliente);
        }
    }
}

 */