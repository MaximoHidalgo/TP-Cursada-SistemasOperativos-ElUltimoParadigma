#include "scheduler_server.h"



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

void crear_hilo_atencion_cliente(pthread_t* hilo_atencion_cliente,t_args_hilo_scheduler* argumento_cliente, t_log* logger, int socket_cliente){
    int resultado_creacion_hilo = pthread_create(hilo_atencion_cliente,NULL,(void*)atender_cliente_scheduler,argumento_cliente);
    if(resultado_creacion_hilo!=0){
        log_error(logger, "Fallo al crear hilo cliente");
            free(argumento_cliente);
            close(socket_cliente);
    } else pthread_detach(*hilo_atencion_cliente);
}

void manejar_conexion_cliente_scheduler(int socket_servidor,t_log* logger, t_scheduler_context* contexto_scheduler){
    int socket_cliente = esperar_cliente(socket_servidor,logger); if(socket_cliente==-1) return;
    t_args_hilo_scheduler* argumento_cliente = crear_argumento_planificador(socket_cliente,contexto_scheduler);
    pthread_t hilo_atencion_cliente;
    crear_hilo_atencion_cliente(&hilo_atencion_cliente,argumento_cliente,logger,socket_cliente);
}



void iniciar_servidor_clientes(t_scheduler_context* ctx) {
    ctx->socket_escucha = preparar_servidor(ctx->puerto_escucha,ctx->logger,ctx);
    while (1) manejar_conexion_cliente_scheduler(ctx->socket_escucha, ctx->logger,ctx);
}

void remover_cpu_de_lista(t_estratega_planificacion* estratega, int fd) {
    if (estratega->sockets_cpu == NULL) return;
    for (int i = 0; i < list_size(estratega->sockets_cpu); i++) {
        int* fd_ptr = list_get(estratega->sockets_cpu, i);
        if (fd_ptr != NULL && *fd_ptr == fd) {
            list_remove_and_destroy_element(estratega->sockets_cpu, i, free);
            break;
        }
    }
}



int atenderRespuesta(int cliente_fd, t_scheduler_context* ctx, int mensaje_cliente) {
    // 1. Manejo de desconexión
    if (mensaje_cliente == -1) {
        log_warning(ctx->logger, "El cliente se desconectó.");
        return -1; // -1 indica que se debe cortar la conexión
    }

    // 2. Validación de seguridad (Bounds Checking) y función no mapeada
    if (mensaje_cliente < 0 || mensaje_cliente >= 256 || tabla_despacho[mensaje_cliente] == NULL) {
        log_warning(ctx->logger, "No entiendo el mensaje o código de operación: %d", mensaje_cliente);
        return -1; // -1 indica error / cortar conexión
    }

    // 3. ¡Magia! Ejecuta el handler correspondiente en O(1)
    return tabla_despacho[mensaje_cliente](cliente_fd, ctx);
}

void atender_cliente_scheduler(void* arg) {
    t_args_hilo_scheduler* args = (t_args_hilo_scheduler*)arg;
    int cliente_fd = args->cliente_fd;
    t_scheduler_context* ctx = args->ctx;
    free(args);

    while (1) {
        int mensaje_cliente = recibir_operacion(cliente_fd);
        
        // Ejecutamos la respuesta
        int status = atenderRespuesta(cliente_fd, ctx, mensaje_cliente);
        
        // Si la tabla o la validación retornan -1, rompemos el ciclo y cerramos.
        if (status == -1) {
            remover_cpu_de_lista(ctx->estratega, cliente_fd);
            close(cliente_fd);
            break;
        }
    }
}