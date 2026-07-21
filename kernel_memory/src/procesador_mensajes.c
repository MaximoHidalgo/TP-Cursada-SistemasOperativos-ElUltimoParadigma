#include "procesador_mensajes.h"

static void conectar_memory_stick(int cliente_fd, t_memory_context* ctx, char* mensaje) {
    char **partes = string_split(mensaje, "|");
    uint32_t stick_size = atoi(partes[1]);
    char* stick_ip = strdup(partes[2]);
    int stick_puerto = atoi(partes[3]);
    
    pthread_mutex_lock(&ctx->mutex_memoria);
    
    t_memory_stick_info* stick = malloc(sizeof(t_memory_stick_info));
    stick->socket_fd = cliente_fd;
    stick->tamanio = stick_size;
    stick->base_address = ctx->memoria_total;
    stick->id = list_size(ctx->memory_sticks) + 1;
    stick->ip = stick_ip;
    stick->puerto = stick_puerto;
    
    list_add(ctx->memory_sticks, stick);
    
    t_hueco_libre* nuevo_hueco = malloc(sizeof(t_hueco_libre));
    nuevo_hueco->base = ctx->memoria_total;
    nuevo_hueco->limite = ctx->memoria_total + stick_size;
    list_add(ctx->huecos_libres, nuevo_hueco);
    fusionar_huecos_libres(ctx->huecos_libres);
    
    ctx->memoria_total += stick_size;
    log_info(ctx->logger, "## Memory Stick de %d bytes Conectada", stick_size);
    log_info(ctx->logger, "Memory Stick registrada en %s:%d. Memoria total actual: %d bytes", stick_ip, stick_puerto, ctx->memoria_total);
    
    pthread_mutex_unlock(&ctx->mutex_memoria);

    pthread_mutex_lock(&ctx->mutex_scheduler_socket);
    notificar_memoria_expandida(ctx->scheduler_socket);
    pthread_mutex_unlock(&ctx->mutex_scheduler_socket);
    
    string_array_destroy(partes);
}

static void conectar_cpu(int cliente_fd, t_memory_context* ctx, char* mensaje) {
    log_info(ctx->logger, "## CPU %s Conectada", mensaje);
    
    pthread_mutex_lock(&ctx->mutex_memoria);
    t_paquete* paq = crear_paquete();
    paq->codigo_operacion = ENVIO_TABLA_STICKS;
    serializar_tabla_sticks_en_paquete(paq, ctx->memory_sticks);
    enviar_paquete(paq, cliente_fd);
    eliminar_paquete(paq);
    pthread_mutex_unlock(&ctx->mutex_memoria);
}

static void conectar_swap(int cliente_fd, t_memory_context* ctx, char* mensaje) {
    char **partes = string_split(mensaje, "|");
    ctx->swap_file_size = atoi(partes[1]);
    ctx->swap_block_size = atoi(partes[2]);
    ctx->swap_socket = cliente_fd;
    
    int cant_bloques = ctx->swap_file_size / ctx->swap_block_size;
    int bits_bytes = (cant_bloques + 7) / 8;
    ctx->bitarray_data = malloc(bits_bytes);
    memset(ctx->bitarray_data, 0, bits_bytes);
    ctx->swap_bitmap = bitarray_create_with_mode(ctx->bitarray_data, bits_bytes, LSB_FIRST);
    
    log_info(ctx->logger, "## SWAP Conectado. Tamaño: %d, Tamaño Bloque: %d, Total Bloques: %d", ctx->swap_file_size, ctx->swap_block_size, cant_bloques);
    string_array_destroy(partes);
}

bool procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx) {
    char *mensaje = recibir_handshake(cliente_fd);
    bool should_continue = true;
    
    if (string_starts_with(mensaje, "STICK|")) {
        conectar_memory_stick(cliente_fd, ctx, mensaje);
        should_continue = false;
    }
    else if (string_equals_ignore_case(mensaje, HANDSHAKE_SCHEDULER)) {
        ctx->scheduler_socket = cliente_fd;
        log_info(ctx->logger, "## Kernel Scheduler Conectado - FD del socket: %d", cliente_fd);
    }
    else if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        conectar_cpu(cliente_fd, ctx, mensaje);
    }
    else if (string_starts_with(mensaje, "SWAP|")) {
        conectar_swap(cliente_fd, ctx, mensaje);
        should_continue = false;
    }
    else { 
        log_info(ctx->logger, "## Módulo Conectado: %s", mensaje);
    }
    free(mensaje);
    return should_continue;
}

static uint32_t recibir_pid(int cliente_fd) {
    uint32_t pid;
    if (recv(cliente_fd, &pid, sizeof(uint32_t), MSG_WAITALL) <= 0) return (uint32_t)-1;
    return pid;
}

static void enviar_lectura_memoria_respuesta(int cliente_fd, void* data, uint32_t tamanio) {
    int opcode = MENSAJE;
    send(cliente_fd, &opcode, sizeof(int), 0);
    send(cliente_fd, &tamanio, sizeof(uint32_t), 0);
    send(cliente_fd, data, tamanio, 0);
}

void procesar_crear_proceso_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    t_buffer_reader* reader = buffer_reader_create(buffer, size);
    
    uint32_t pid = buffer_read_uint32(reader);
    char* path = buffer_read_string(reader);
    
    buffer_reader_destroy(reader);
    free(buffer);
    
    char* path_completo = string_from_format("%s/%s", ctx->base_path, path);
    
    log_info(ctx->logger, "Creando proceso PID %d con script: %s", pid, path_completo);
    crear_proceso(ctx, pid, path_completo);
    enviar_mensaje(MSG_OK, cliente_fd);
    
    free(path_completo);
    free(path);
}

void procesar_solicitud_contexto_handler(int cliente_fd, t_memory_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    int pid = *(int*)list_get(paquete, 0);
    list_destroy_and_destroy_elements(paquete, free);
    
    t_programa* prog = buscar_programa(ctx, pid);
    if (prog != NULL) {
        int cant = prog->contexto->tabla_segmentos ? list_size(prog->contexto->tabla_segmentos) : 0;
        log_info(ctx->logger, "DEBUG: Enviando contexto para el PID %d con %d segmentos", pid, cant);
        enviar_contexto(prog->contexto, cliente_fd);
    } else {
        log_error(ctx->logger, "Error: Contexto solicitado para PID %d inexistente", pid);
    }
}

void procesar_actualizacion_contexto_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    t_contexto_ejecucion contexto_actualizado;
    deserializar_contexto(buffer, size, &contexto_actualizado);
    free(buffer);
    
    t_programa* prog = buscar_programa(ctx, contexto_actualizado.pid);
    if (prog != NULL) {
        pthread_mutex_lock(&ctx->mutex_programas);

        prog->contexto->registros = contexto_actualizado.registros;
        
        if (contexto_actualizado.tabla_segmentos) {
            list_destroy_and_destroy_elements(contexto_actualizado.tabla_segmentos, free);
        }
        
        pthread_mutex_unlock(&ctx->mutex_programas);
        log_info(ctx->logger, "Contexto actualizado correctamente para PID %d", prog->contexto->pid);
    } else {
        log_error(ctx->logger, "Error: Intento de actualizar contexto de PID %d inexistente", contexto_actualizado.pid);
    }
    enviar_mensaje("OK", cliente_fd);
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

void procesar_suspender_proceso(int cliente_fd, t_memory_context* ctx) {
    uint32_t pid = recibir_pid(cliente_fd);
    
    t_programa* prog = buscar_programa(ctx, pid);
    if (!prog) {
        enviar_mensaje("ERROR", cliente_fd);
        return;
    }
    
    if (gestionar_suspension(ctx, prog->contexto)) {
        enviar_mensaje("OK", cliente_fd);
        log_info(ctx->logger, "Proceso %d suspendido y guardado en SWAP", pid);
    } else {
        enviar_mensaje("ERROR", cliente_fd);
        log_error(ctx->logger, "No hay espacio en SWAP para suspender PID %d", pid);
    }
}

void procesar_desuspender_proceso(int cliente_fd, t_memory_context* ctx) {
    uint32_t pid = recibir_pid(cliente_fd);
    
    t_programa* prog = buscar_programa(ctx, pid);
    if (!prog) {
        enviar_mensaje("ERROR", cliente_fd);
        return;
    }
    
    if (gestionar_desuspension(ctx, prog->contexto, cliente_fd)) {
        enviar_mensaje("OK", cliente_fd);
        log_info(ctx->logger, "Proceso %d desuspendido y traído a Memoria Principal", pid);
    } else {
        enviar_mensaje("ERROR", cliente_fd);
        log_error(ctx->logger, "No hay espacio en Memoria Principal para desuspender PID %d", pid);
    }
}

void procesar_crear_segmento_handler(int cliente_fd, t_memory_context* ctx){
    
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t pid = *(uint32_t*)list_get(paquete, 0);
    uint32_t id_seg = *(uint32_t*)list_get(paquete, 1);
    uint32_t tamanio_seg = *(uint32_t*)list_get(paquete, 2);
    list_destroy_and_destroy_elements(paquete, free);

    int needs_compaction = 0;
    pthread_mutex_lock(&ctx->mutex_memoria);
    t_segmento* nuevo_seg = crear_segmento(ctx, pid, id_seg, tamanio_seg, &needs_compaction);
                
    if (needs_compaction) {
        if (solicitar_compactacion_al_scheduler(cliente_fd, ctx)) {
            nuevo_seg = crear_segmento(ctx, pid, id_seg, tamanio_seg, &needs_compaction);
        }
    }

    if (nuevo_seg != NULL) {
        t_programa* prog = buscar_programa(ctx, pid);
        if (prog != NULL) {
            pthread_mutex_lock(&ctx->mutex_programas);

            if (prog->contexto->tabla_segmentos == NULL) {
                prog->contexto->tabla_segmentos = list_create();
            }

            list_add(prog->contexto->tabla_segmentos, nuevo_seg);
            pthread_mutex_unlock(&ctx->mutex_programas);
        } else {
            log_error(ctx->logger, "Programa no encontrado para PID %d en CREAR_SEGMENTO", pid);
        }
        enviar_mensaje(MSG_OK, cliente_fd);
    } else {
        enviar_mensaje("OUT_OF_MEMORY", cliente_fd);
    }
    pthread_mutex_unlock(&ctx->mutex_memoria);
}

void procesar_eliminar_segmento_handler(int cliente_fd, t_memory_context* ctx){
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t pid = *(uint32_t*)list_get(paquete, 0);
    uint32_t id_seg = *(uint32_t*)list_get(paquete, 1);
    list_destroy_and_destroy_elements(paquete, free);

    pthread_mutex_lock(&ctx->mutex_memoria);
    t_programa* prog = buscar_programa(ctx, pid);
    if (prog != NULL && prog->contexto->tabla_segmentos != NULL) {
        pthread_mutex_lock(&ctx->mutex_programas);
        t_segmento* seg_a_eliminar = buscar_y_remover_segmento(prog->contexto->tabla_segmentos, id_seg);
        pthread_mutex_unlock(&ctx->mutex_programas);

        if (seg_a_eliminar != NULL) {
            eliminar_segmento(ctx, pid, seg_a_eliminar);
            enviar_mensaje(MSG_OK, cliente_fd);
        } else {
            enviar_mensaje("SEG_NOT_FOUND", cliente_fd);
        }
    } else {
        enviar_mensaje("PROG_NOT_FOUND", cliente_fd);
    }
    pthread_mutex_unlock(&ctx->mutex_memoria);
}

void procesar_liberar_proceso_handler(int cliente_fd, t_memory_context* ctx){
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t pid_a_liberar = *(uint32_t*)list_get(paquete, 0);
    list_destroy_and_destroy_elements(paquete, free);

    pthread_mutex_lock(&ctx->mutex_memoria);
    t_programa* prog = buscar_programa(ctx, pid_a_liberar);
    if (prog == NULL) {
        pthread_mutex_unlock(&ctx->mutex_memoria);
        log_error(ctx->logger, "Intento de liberar PID %d inexistente", pid_a_liberar);
        return;
    }

    destruir_programa(ctx, prog);

    log_info(ctx->logger, "## PID: %d - Proceso Finalizado y liberado de la memoria", pid_a_liberar);
    pthread_mutex_unlock(&ctx->mutex_memoria);
}

void procesar_solicitud_tam_max_segmento_handler(int cliente_fd, t_memory_context* ctx){
    t_paquete* paq = crear_paquete();
    paq->codigo_operacion = TAM_MAX_SEGMENTO;
    agregar_a_paquete(paq, &ctx->segment_max_size, sizeof(int));
    enviar_paquete(paq, cliente_fd);
    eliminar_paquete(paq);
}

void procesar_leer_memoria_handler(int cliente_fd, t_memory_context* ctx){
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t pid = *(uint32_t*)list_get(paquete, 0);
    uint32_t dir_fisica = *(uint32_t*)list_get(paquete, 1);
    uint32_t tamanio = *(uint32_t*)list_get(paquete, 2);
    list_destroy_and_destroy_elements(paquete, free);

    pthread_mutex_lock(&ctx->mutex_memoria);
    void* data_leida = leer_memoria_distribuida(ctx, dir_fisica, tamanio);
    pthread_mutex_unlock(&ctx->mutex_memoria);

    log_info(ctx->logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", pid, dir_fisica, tamanio);

    enviar_lectura_memoria_respuesta(cliente_fd, data_leida, tamanio);
    free(data_leida);
}

void procesar_escribir_memoria_handler(int cliente_fd, t_memory_context* ctx){
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t pid = *(uint32_t*)list_get(paquete, 0);
    uint32_t dir_fisica = *(uint32_t*)list_get(paquete, 1);
    uint32_t tamanio = *(uint32_t*)list_get(paquete, 2);
    void* data_a_escribir = list_get(paquete, 3);
                
    pthread_mutex_lock(&ctx->mutex_memoria);
    escribir_memoria_distribuida(ctx, dir_fisica, tamanio, data_a_escribir);
    pthread_mutex_unlock(&ctx->mutex_memoria);
                
    log_info(ctx->logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", pid, dir_fisica, tamanio);

    list_destroy_and_destroy_elements(paquete, free);
    enviar_mensaje(MSG_OK, cliente_fd);
}

void procesar_solicitud_tabla_sticks_handler(int cliente_fd, t_memory_context* ctx){
    pthread_mutex_lock(&ctx->mutex_memoria);
    
    t_paquete* paq = crear_paquete();
    paq->codigo_operacion = ENVIO_TABLA_STICKS;
    serializar_tabla_sticks_en_paquete(paq, ctx->memory_sticks);
    enviar_paquete(paq, cliente_fd);
    eliminar_paquete(paq);
    
    pthread_mutex_unlock(&ctx->mutex_memoria);
}

void procesar_consultar_espacio_libre_handler(int cliente_fd, t_memory_context* ctx) {
    int espacio_libre_total = 0;
    
    pthread_mutex_lock(&ctx->mutex_memoria);
    for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
        t_hueco_libre* hueco = list_get(ctx->huecos_libres, i);
        espacio_libre_total += (hueco->limite - hueco->base);
    }
    pthread_mutex_unlock(&ctx->mutex_memoria);
    
    // El Kernel Scheduler espera un opcode (MENSAJE) y luego el int
    enviar_int(cliente_fd, MENSAJE);
    send(cliente_fd, &espacio_libre_total, sizeof(int), 0); 
}

void procesar_desconexion_cliente(int cliente_fd, t_memory_context* ctx) {
    log_warning(ctx->logger, "Cliente desconectado");
    for (int i = 0; i < list_size(ctx->memory_sticks); i++)
    {
        t_memory_stick_info* stick = list_get(ctx->memory_sticks, i);

        if (cliente_fd == stick->socket_fd)
        {
            pthread_mutex_lock(&ctx->mutex_scheduler_socket);
            notificar_memoria_corrupta(ctx->scheduler_socket);
            pthread_mutex_unlock(&ctx->mutex_scheduler_socket);
        }
    }
    
    close(cliente_fd);
}

void procesar_retroceso_pc_out_of_memory_handler(int cliente_fd, t_memory_context* ctx) {
    t_list* paq = recibir_paquete(cliente_fd);
    uint32_t pid_aj = *(uint32_t*)list_get(paq, 0);
    list_destroy_and_destroy_elements(paq, free);
    t_programa* p = buscar_programa(ctx, pid_aj);
    if (p) {
        pthread_mutex_lock(&ctx->mutex_programas);
        p->contexto->registros.PC--;
        pthread_mutex_unlock(&ctx->mutex_programas);
    }
    enviar_mensaje("OK", cliente_fd);
}