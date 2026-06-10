#include "procesador_mensajes.h"

void procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx) {
    char *mensaje = recibir_handshake(cliente_fd);
    
    if (string_starts_with(mensaje, "STICK|")) {
        char **partes = string_split(mensaje, "|");
        log_info(ctx->logger, "## Memory Stick de %s bytes Conectada", partes[1]);
        string_array_destroy(partes);
    }
    else if (string_equals_ignore_case(mensaje, HANDSHAKE_SCHEDULER)) {
        log_info(ctx->logger, "## Kernel Scheduler Conectado - FD del socket: %d", cliente_fd);
    }
    else if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        log_info(ctx->logger, "## CPU %s Conectada", mensaje);
    }
    else { 
        log_info(ctx->logger, "## Módulo Conectado: %s", mensaje);
    }
    free(mensaje);
}

void procesar_crear_proceso_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    t_peticion_crear_proceso* peticion = deserializar_peticion_crear_proceso(buffer);
    free(buffer);
    
    char *path_completo = string_new();
    string_append(&path_completo, ctx->base_path);
    string_append(&path_completo, "/");
    string_append(&path_completo, peticion->path);
    
    crear_proceso(ctx, peticion->pid, path_completo);
    enviar_mensaje(MSG_OK, cliente_fd);
    
    free(path_completo);
    destruir_peticion_crear_proceso(peticion);
}

void procesar_solicitud_contexto_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    int desplazamiento = 0;
    int tam_pid, pid;
    memcpy(&tam_pid, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&pid, buffer + desplazamiento, sizeof(int));
    free(buffer);
    
    t_programa* prog = buscar_programa(ctx, pid);
    if (prog != NULL) {
        enviar_contexto(prog->contexto, cliente_fd);
    } else {
        log_error(ctx->logger, "Error: Contexto solicitado para PID %d inexistente", pid);
    }
}

void procesar_actualizacion_contexto_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    t_contexto_ejecucion contexto_actualizado;
    deserializar_contexto(buffer, &contexto_actualizado);
    free(buffer);
    
    t_programa* prog = buscar_programa(ctx, contexto_actualizado.pid);
    if (prog != NULL) {
        pthread_mutex_lock(&ctx->mutex_programas);
        *(prog->contexto) = contexto_actualizado;
        pthread_mutex_unlock(&ctx->mutex_programas);
        log_info(ctx->logger, "Contexto actualizado correctamente para PID %d", prog->contexto->pid);
    } else {
        log_error(ctx->logger, "Error: Intento de actualizar contexto de PID %d inexistente", contexto_actualizado.pid);
    }
}

void procesar_fetch_instruccion_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    
    t_peticion_fetch_instruccion* peticion = deserializar_peticion_fetch_instruccion(buffer);
    free(buffer);
    
    char *instr = obtener_instruccion(ctx, peticion->pid, peticion->pc);
    
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ENVIO_INSTRUCCION;
    agregar_a_paquete(paquete, instr, strlen(instr) + 1);
    enviar_paquete(paquete, cliente_fd);
    eliminar_paquete(paquete);
    
    free(instr);
    destruir_peticion_fetch_instruccion(peticion);
}

void atender_cliente(void *arg) {
    t_args_hilo_cliente* args = (t_args_hilo_cliente*)arg;
    int cliente_fd = args->cliente_fd;
    t_memory_context* ctx = args->ctx;
    free(args);

    while (1) {
        int cod_op = recibir_operacion(cliente_fd);

        switch(cod_op) {
            case MENSAJE:
                procesar_mensaje_handshake(cliente_fd, ctx);
                break;
            case CREAR_PROCESO:
                procesar_crear_proceso_handler(cliente_fd, ctx);
                break;
            case SOLICITUD_CONTEXTO:
                procesar_solicitud_contexto_handler(cliente_fd, ctx);
                break;
            case ACTUALIZACION_CONTEXTO:
                procesar_actualizacion_contexto_handler(cliente_fd, ctx);
                break;
            case FETCH_INSTRUCCION:
                procesar_fetch_instruccion_handler(cliente_fd, ctx);
                break;
            case LEER_MEMORIA:
            case ESCRIBIR_MEMORIA:
                enviar_mensaje(MSG_OK, cliente_fd);
                break;
            case CONSULTAR_ESPACIO_LIBRE:
            {
                int espacio_fijo_mock = 999999; 
                send(cliente_fd, &espacio_fijo_mock, sizeof(int), 0); 
                break;
            }
            case -1:
                log_warning(ctx->logger, "Cliente desconectado");
                close(cliente_fd);
                return;
            default:
                log_warning(ctx->logger, "Operación desconocida: %d", cod_op);
                close(cliente_fd);
                return;
        }
    }
}
