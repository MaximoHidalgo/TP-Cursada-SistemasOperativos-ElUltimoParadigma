#include "atender_cpu.h"

void* atender_cliente_cpu(void* arg) {
    t_args_hilo_cpu* args = (t_args_hilo_cpu*)arg;
    int socket_cpu = args->socket_cliente;
    t_memory_stick_context* ctx = args->ctx;
    free(args);

    while (1) {
        int cod_op = recibir_operacion(socket_cpu);

        switch (cod_op) {
            case MENSAJE:
                procesar_mensaje_handshake(socket_cpu, ctx->logger);
                break;
            case LEER_MEMORIA:
                procesar_lectura(socket_cpu, ctx);
                break;
            case ESCRIBIR_MEMORIA:
                procesar_escritura(socket_cpu, ctx);
                break;
            case -1:
                log_info(ctx->logger, "Cliente (CPU) desconectado");
                close(socket_cpu);
                return NULL;
            default:
                log_warning(ctx->logger, "Operación desconocida de la CPU: %d", cod_op);
                close(socket_cpu);
                return NULL;
        }
    }
}

void procesar_mensaje_handshake(int socket_cpu, t_log* logger) {
    char *id_cpu = recibir_handshake(socket_cpu);
    log_info(logger, "## CPU %s Conectada", id_cpu);
    free(id_cpu);
}

void procesar_lectura(int socket_cpu, t_memory_stick_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, socket_cpu);
    t_peticion_lectura* peticion = deserializar_peticion_lectura(buffer);
    free(buffer);

    void* datos_leidos = malloc(peticion->tamanio);
    
    // Lectura sincronizada con mutex
    pthread_mutex_lock(&ctx->mutex_memoria);
    if (peticion->direccion_fisica + peticion->tamanio <= ctx->tamanio_total) {
        memcpy(datos_leidos, ctx->espacio_memoria + peticion->direccion_fisica, peticion->tamanio);
        log_info(ctx->logger, "## Lectura de %u bytes", peticion->tamanio);
    } else {
        log_error(ctx->logger, "Error: Intento de lectura fuera de los límites de la memoria");
        memset(datos_leidos, 0, peticion->tamanio);
    }
    pthread_mutex_unlock(&ctx->mutex_memoria);

    // Devolver resultado
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = LECTURA;
    agregar_a_paquete(paquete, datos_leidos, peticion->tamanio);
    enviar_paquete(paquete, socket_cpu);
    
    eliminar_paquete(paquete);
    free(datos_leidos);
    destruir_peticion_lectura(peticion);
}

void procesar_escritura(int socket_cpu, t_memory_stick_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, socket_cpu);
    t_peticion_escritura* peticion = deserializar_peticion_escritura(buffer);
    free(buffer);

    // Escritura sincronizada con mutex
    pthread_mutex_lock(&ctx->mutex_memoria);
    if (peticion->direccion_fisica + peticion->tamanio <= ctx->tamanio_total) {
        memcpy(ctx->espacio_memoria + peticion->direccion_fisica, peticion->contenido, peticion->tamanio);
        log_info(ctx->logger, "## Escritura de %u bytes", peticion->tamanio);
    } else {
        log_error(ctx->logger, "Error: Intento de escritura fuera de los límites de la memoria");
    }
    pthread_mutex_unlock(&ctx->mutex_memoria);

    // Responder OK
    enviar_mensaje(MSG_OK, socket_cpu);
    
    destruir_peticion_escritura(peticion);
}
