#include "atender_cliente_kernel_memory.h"

void atender_cliente_kernel_memory(void *arg) {
    t_args_hilo_cliente* args = (t_args_hilo_cliente*)arg;
    int cliente_fd = args->cliente_fd;
    t_memory_context* ctx = args->ctx;
    free(args);

    while (1) {
        int cod_op = recibir_operacion(cliente_fd);

        bool is_scheduler = (cliente_fd == ctx->scheduler_socket);
        if (is_scheduler) pthread_mutex_lock(&ctx->mutex_scheduler_socket);

        switch(cod_op) {
            case MENSAJE:
                if (!procesar_mensaje_handshake(cliente_fd, ctx)) { if (is_scheduler) pthread_mutex_unlock(&ctx->mutex_scheduler_socket); return; }
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
            case SUSPENDER_PROCESO:
                procesar_suspender_proceso(cliente_fd, ctx);
                break;
            case DESUSPENDER_PROCESO:
                procesar_desuspender_proceso(cliente_fd, ctx);
                break;
            case FETCH_INSTRUCCION:
                procesar_fetch_instruccion_handler(cliente_fd, ctx);
                break;
            case CREAR_SEGMENTO:
                procesar_crear_segmento_handler(cliente_fd, ctx);
                break;
            case LIBERAR_PROCESO:
                procesar_liberar_proceso_handler(cliente_fd, ctx);
                break;
            case ELIMINAR_SEGMENTO:
                procesar_eliminar_segmento_handler(cliente_fd, ctx);
                break;
            case SOLICITUD_TAM_MAX_SEGMENTO:
                procesar_solicitud_tam_max_segmento_handler(cliente_fd, ctx);
                break;
            case LEER_MEMORIA:
                procesar_leer_memoria_handler(cliente_fd, ctx);
                break;
            case ESCRIBIR_MEMORIA:
                procesar_escribir_memoria_handler(cliente_fd, ctx);
                break;
            case SOLICITUD_TABLA_STICKS:
                procesar_solicitud_tabla_sticks_handler(cliente_fd, ctx);
                break;
            case CONSULTAR_ESPACIO_LIBRE:
                procesar_consultar_espacio_libre_handler(cliente_fd, ctx);
                break;
            case -1:
                if (is_scheduler) pthread_mutex_unlock(&ctx->mutex_scheduler_socket);
                procesar_desconexion_cliente(cliente_fd, ctx);
                return;
            case RETROCEDER_PC_OUT_OF_MEMORY:
                procesar_retroceso_pc_out_of_memory_handler(cliente_fd, ctx);
                break;
            default:
                log_warning(ctx->logger, "Operación desconocida: %d", cod_op);
                close(cliente_fd);
                if (is_scheduler) pthread_mutex_unlock(&ctx->mutex_scheduler_socket);
                return;
        }

        if (is_scheduler) pthread_mutex_unlock(&ctx->mutex_scheduler_socket);
    }
}