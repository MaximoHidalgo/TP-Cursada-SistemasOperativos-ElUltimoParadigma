#include "cpu_memoria.h"

// -------------------------------------------------------------
// PRIVATE FUNCTION
// -------------------------------------------------------------

static void loguear_segmentos_contexto(t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    int cant = contexto->tabla_segmentos ? list_size(contexto->tabla_segmentos) : 0;
    log_info(ctx->logger, "Contexto de ejecución del PID %d recibido exitosamente con %d segmentos.", contexto->pid, cant);
    
    for(int i = 0; i < cant; i++) {
        t_segmento* s = list_get(contexto->tabla_segmentos, i);
        log_info(ctx->logger, "  -> Seg ID: %d, Base: %d, Tamanio: %d", s->id, s->base, s->tamanio);
    }
}

// -------------------------------------------------------------
// OPERACIONES DE MEMORIA
// -------------------------------------------------------------

t_contexto_ejecucion* pedir_contexto_a_memoria(int pid_recibido, t_cpu_context* ctx) {
    // Enviamos el PID mediante el helper limpio de utils
    enviar_operacion_con_pid(SOLICITUD_CONTEXTO, pid_recibido, ctx->conexiones->memoria);

    log_info(ctx->logger, "Solicitando contexto del PID %d a Kernel Memory...", pid_recibido);

    // Quedarnos esperando la respuesta de la Memoria
    int operacion = recibir_operacion(ctx->conexiones->memoria);

    if (operacion != ENVIO_CONTEXTO) {
        log_error(ctx->logger, "Error: Se esperaba el contexto de ejecución, pero llegó la operación: %d", operacion);
        return NULL;
    }

    int size;
    void* buffer = recibir_buffer(&size, ctx->conexiones->memoria);

    // Deserializar el buffer recibido para armar el struct localmente
    t_contexto_ejecucion* contexto = malloc(sizeof(t_contexto_ejecucion));
    deserializar_contexto(buffer, size, contexto);
    free(buffer);

    loguear_segmentos_contexto(contexto, ctx);

    return contexto;
}

void enviar_contexto_a_memoria(t_contexto_ejecucion* contexto, int conexion_memory) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ACTUALIZACION_CONTEXTO;
    serializar_contexto(paquete, contexto);
    enviar_paquete(paquete, conexion_memory);
    eliminar_paquete(paquete);

    // Esperamos confirmación (ACK) para evitar Race Conditions con los mensajes
    // que Kernel Scheduler manda asíncronamente a Kernel Memory (EJ: OOM/EXIT)
    int op = recibir_operacion(conexion_memory);
    if (op == MENSAJE) {
        int size;
        char* msg = recibir_buffer(&size, conexion_memory);
        free(msg);
    }
}