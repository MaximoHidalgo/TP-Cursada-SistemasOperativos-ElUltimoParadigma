#include "cpu_utils.h"

// -------------------------------------------------------------
// FUNCIONES PRIVADAS DE LA TABLA DE STICKS
// -------------------------------------------------------------

static t_memory_stick_info* deserializar_stick_info(t_buffer_reader* reader) {
    int size_id = buffer_read_int(reader); (void)size_id;
    int stick_id = buffer_read_int(reader);

    int size_tamanio = buffer_read_int(reader); (void)size_tamanio;
    uint32_t stick_tamanio = buffer_read_uint32(reader);

    int size_base_address = buffer_read_int(reader); (void)size_base_address;
    uint32_t stick_base_address = buffer_read_uint32(reader);

    int size_puerto = buffer_read_int(reader); (void)size_puerto;
    int stick_puerto = buffer_read_int(reader);

    int size_ip_len = buffer_read_int(reader); (void)size_ip_len;
    int ip_len = buffer_read_int(reader); (void)ip_len;
    char* stick_ip = buffer_read_string(reader);

    t_memory_stick_info* stick = malloc(sizeof(t_memory_stick_info));
    stick->id = stick_id;
    stick->tamanio = stick_tamanio;
    stick->base_address = stick_base_address;
    stick->puerto = stick_puerto;
    stick->ip = stick_ip;
    stick->socket_fd = -1;

    return stick;
}

static bool stick_ya_existe(t_list* sticks, int stick_id) {
    for (int j = 0; j < list_size(sticks); j++) {
        t_memory_stick_info* s = list_get(sticks, j);
        if (s->id == stick_id) {
            return true;
        }
    }
    return false;
}

void destruir_stick_info(void* stick_ptr) {
    t_memory_stick_info* stick = (t_memory_stick_info*)stick_ptr;
    if (stick->socket_fd != -1) {
        liberar_conexion(stick->socket_fd);
    }
    if (stick->ip != NULL) {
        free(stick->ip);
    }
    free(stick);
}

static bool conectar_stick(t_memory_stick_info* stick, t_cpu_context* ctx) {
    char* str_puerto = string_itoa(stick->puerto);
    stick->socket_fd = crear_conexion(stick->ip, str_puerto);
    free(str_puerto);

    if (stick->socket_fd != -1) {
        enviar_mensaje(ctx->id_cpu, stick->socket_fd);
        log_info(ctx->logger, "Conectado a Memory Stick ID %d en %s:%d", stick->id, stick->ip, stick->puerto);
        return true;
    } else {
        log_error(ctx->logger, "Error al conectar con Memory Stick ID %d", stick->id);
        return false;
    }
}

// -------------------------------------------------------------
// FUNCIONES PUBLICAS
// -------------------------------------------------------------

void manejar_ejecucion_proceso(int pid_recibido, t_cpu_context* ctx) {
    log_info(ctx->logger, "## PID recibido para ejecutar: %d", pid_recibido);

    t_contexto_ejecucion* contexto = pedir_contexto_a_memoria(pid_recibido, ctx);

    if (contexto != NULL) {
        ejecutar_ciclo_de_instruccion(contexto, ctx);
        if (contexto->tabla_segmentos != NULL) {
            list_destroy_and_destroy_elements(contexto->tabla_segmentos, free);
        }
        free(contexto);
    } else {
        log_error(ctx->logger, "Memoria devolvió un contexto nulo para el PID %d", pid_recibido);
    }
}

int desalojar_proceso(t_contexto_ejecucion* contexto, t_cpu_context* ctx, op_code motivo) {
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    enviar_operacion_con_pid(motivo, contexto->pid, ctx->conexiones->scheduler);
    return 1;
}

void procesar_tabla_de_sticks(t_cpu_context* ctx, void* buffer, int size) {
    t_buffer_reader* reader = buffer_reader_create(buffer, size);
    
    int size_payload_cant = buffer_read_int(reader); (void)size_payload_cant;
    int cant = buffer_read_int(reader);

    for (int i = 0; i < cant; i++) {
        t_memory_stick_info* stick = deserializar_stick_info(reader);

        if (stick_ya_existe(ctx->memory_sticks, stick->id)) {
            destruir_stick_info(stick);
            continue;
        }

        if (conectar_stick(stick, ctx)) {
            list_add(ctx->memory_sticks, stick);
        } else {
            destruir_stick_info(stick);
        }
    }
    
    buffer_reader_destroy(reader);
}

void actualizar_tabla_sticks(t_cpu_context* ctx) {
    t_paquete* paq = crear_paquete();
    paq->codigo_operacion = SOLICITUD_TABLA_STICKS;
    enviar_paquete(paq, ctx->conexiones->memoria);
    eliminar_paquete(paq);

    log_info(ctx->logger, "Solicitando actualización de tabla de Memory Sticks al Kernel Memory...");

    int resp = recibir_operacion(ctx->conexiones->memoria);
    if (resp == ENVIO_TABLA_STICKS) {
        int size;
        void* buf = recibir_buffer(&size, ctx->conexiones->memoria);
        procesar_tabla_de_sticks(ctx, buf, size);
        free(buf);
    } else {
        log_error(ctx->logger, "Error al solicitar actualización de sticks. Operación recibida: %d", resp);
    }
}