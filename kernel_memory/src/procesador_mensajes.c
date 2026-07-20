#include "procesador_mensajes.h"
#include "auxiliares_memoria.h"

#include "auxiliares.h"

void procesar_crear_proceso_handler(int cliente_fd, t_memory_context* ctx) {
    // El sender (enviar_peticion_crear_proceso) usa buffer_write_uint32 + buffer_write_string
    // que escriben directamente al stream, NO con agregar_a_paquete.
    // Por eso debemos deserializar con buffer_reader, no con recibir_paquete.
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
        log_info(ctx->logger, "DEBUG: Sending context for PID %d with %d segments", pid, cant);
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
        
        // Actualizamos solamente los registros (que incluyen el PC)
        // La memoria es la dueña de la tabla de segmentos, no debemos sobreescribirla
        // con una version potencialmente vieja del CPU.
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
    uint32_t pid;
    recv(cliente_fd, &pid, sizeof(uint32_t), MSG_WAITALL);
    
    t_programa* prog = buscar_programa(ctx, pid);
    if (!prog) {
        enviar_mensaje("ERROR", cliente_fd);
        return;
    }
    t_contexto_ejecucion* proc = prog->contexto;
    
    pthread_mutex_lock(&ctx->mutex_memoria);

    int cant_bloques_swap = ctx->swap_file_size / ctx->swap_block_size;

    bool hay_espacio_swap = hay_espacio_para_suspender_proceso(ctx, proc);

    if (hay_espacio_swap)
    {
        suspender_proceso(ctx, proc, cant_bloques_swap);

    }else{
        pthread_mutex_unlock(&ctx->mutex_memoria);
        enviar_mensaje("ERROR", cliente_fd);
        log_error(ctx->logger, "No hay espacio en SWAP para suspender PID %d", pid);
        return;
    }
    
    pthread_mutex_unlock(&ctx->mutex_memoria);
    
    enviar_mensaje("OK", cliente_fd);
    log_info(ctx->logger, "Proceso %d suspendido y guardado en SWAP", pid);
}

void procesar_desuspender_proceso(int cliente_fd, t_memory_context* ctx) {
    uint32_t pid;
    recv(cliente_fd, &pid, sizeof(uint32_t), MSG_WAITALL);
    
    t_programa* prog = buscar_programa(ctx, pid);
    if (!prog) {
        enviar_mensaje("ERROR", cliente_fd);
        return;
    }
    t_contexto_ejecucion* proc = prog->contexto;

    uint32_t memoria_necesaria = 0;

    for (int k = 0; k < list_size(proc->tabla_segmentos); k++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, k);

        if(seg->tamanio == 0) continue;

        memoria_necesaria += seg->tamanio;
    }
    
    pthread_mutex_lock(&ctx->mutex_memoria);

    bool hay_espacio = hay_espacio_para_restaurar_segmentos(ctx, proc);
    
    if (!hay_espacio) {

        if (requiere_compactacion(ctx, memoria_necesaria))
        {
            int op_sol = SOLICITUD_COMPACTACION;
            send(cliente_fd, &op_sol, sizeof(int), 0);

                    
            pthread_mutex_unlock(&ctx->mutex_memoria);
            int res = recibir_operacion(cliente_fd);
            pthread_mutex_lock(&ctx->mutex_memoria);
            
            if (res == RESPUESTA_COMPACTACION) {
                compactar_memoria(ctx);
                if (!hay_espacio_para_restaurar_segmentos(ctx, proc)) {
                    pthread_mutex_unlock(&ctx->mutex_memoria);
                    enviar_mensaje("ERROR", cliente_fd);
                    log_error(ctx->logger, "No hay espacio en Memoria Principal para desuspender PID %d, incluso con la compactación", pid);
                    return;
                }
            }
        }else{
            pthread_mutex_unlock(&ctx->mutex_memoria);
            enviar_mensaje("ERROR", cliente_fd);
            log_error(ctx->logger, "No hay espacio en Memoria Principal para desuspender PID %d", pid);
            return;
        }
    }

    if(!restaurar_segmentos(proc, ctx, cliente_fd)){
        pthread_mutex_unlock(&ctx->mutex_memoria);
        return;
    }
    pthread_mutex_unlock(&ctx->mutex_memoria);
    enviar_mensaje("OK", cliente_fd);
    log_info(ctx->logger, "Proceso %d desuspendido y traído a Memoria Principal", pid);
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
        int op_sol = SOLICITUD_COMPACTACION;
        send(cliente_fd, &op_sol, sizeof(int), 0);

                    
        pthread_mutex_unlock(&ctx->mutex_memoria);
        int res = recibir_operacion(cliente_fd);
        pthread_mutex_lock(&ctx->mutex_memoria);
        
        if (res == RESPUESTA_COMPACTACION) {
            compactar_memoria(ctx);
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
        t_segmento* seg_a_eliminar = NULL;
        for (int i = 0; i < list_size(prog->contexto->tabla_segmentos); i++) {
            t_segmento* seg = list_get(prog->contexto->tabla_segmentos, i);
            if (seg->id == id_seg) {
                seg_a_eliminar = seg;
                list_remove(prog->contexto->tabla_segmentos, i);
                break;
            }
        }
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

    // 1. Liberar todos los segmentos de memoria asociados al proceso
    if (prog->contexto->tabla_segmentos != NULL) {
        pthread_mutex_lock(&ctx->mutex_programas);
        for (int i = 0; i < list_size(prog->contexto->tabla_segmentos); i++) {
            t_segmento* seg = list_get(prog->contexto->tabla_segmentos, i);
            eliminar_segmento(ctx, pid_a_liberar, seg);
        }
        list_clean(prog->contexto->tabla_segmentos);
        pthread_mutex_unlock(&ctx->mutex_programas);
    }

    // 2. Eliminar el programa de la lista global y liberar todas sus estructuras
    pthread_mutex_lock(&ctx->mutex_programas);
    for (int i = 0; i < list_size(ctx->programas); i++) {
        if (list_get(ctx->programas, i) == prog) {
            list_remove(ctx->programas, i);
            break;
        }
    }
    pthread_mutex_unlock(&ctx->mutex_programas);

    // 3. Liberar instrucciones
    if (prog->instrucciones != NULL) {
        for (int i = 0; i < prog->cantidad_instrucciones; i++) {
            free(prog->instrucciones[i]);
        }
        free(prog->instrucciones);
    }

    // 4. Liberar contexto de ejecución
    if (prog->contexto != NULL) {
        if (prog->contexto->tabla_segmentos != NULL) {
            list_destroy(prog->contexto->tabla_segmentos);
        }
        free(prog->contexto);
    }

    // 5. Liberar el programa
    free(prog);

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

    t_paquete* paq = crear_paquete();
    paq->codigo_operacion = MENSAJE; 
    
    // Evitamos usar agregar_a_paquete para no agregar el prefijo de tamaño adicional (4 bytes)
    // ya que el Scheduler lee con recibir_buffer y espera los bytes limpios.
    paq->buffer->size = tamanio;
    paq->buffer->stream = malloc(tamanio);
    memcpy(paq->buffer->stream, data_leida, tamanio);
    
    enviar_paquete(paq, cliente_fd);
    eliminar_paquete(paq);
    free(data_leida);
}

void procesar_escribir_memoria_handler(int cliente_fd, t_memory_context* ctx){
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t pid = *(uint32_t*)list_get(paquete, 0);
    uint32_t dir_fisica = *(uint32_t*)list_get(paquete, 1);
    uint32_t tamanio = *(uint32_t*)list_get(paquete, 2);
    
    void* data_a_escribir = malloc(tamanio);
    memcpy(data_a_escribir, list_get(paquete, 3), tamanio);
    
    list_destroy_and_destroy_elements(paquete, free);
                
    pthread_mutex_lock(&ctx->mutex_memoria);
    escribir_memoria_distribuida(ctx, dir_fisica, tamanio, data_a_escribir);
    pthread_mutex_unlock(&ctx->mutex_memoria);
                
    log_info(ctx->logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", pid, dir_fisica, tamanio);

    free(data_a_escribir);
    enviar_mensaje(MSG_OK, cliente_fd);
}

void procesar_solicitud_tabla_sticks_handler(int cliente_fd, t_memory_context* ctx){
    pthread_mutex_lock(&ctx->mutex_memoria);
    t_paquete* paq = crear_paquete();
    paq->codigo_operacion = ENVIO_TABLA_STICKS;
    int cant = list_size(ctx->memory_sticks);
    agregar_a_paquete(paq, &cant, sizeof(int));
    for (int i = 0; i < cant; i++) {
        t_memory_stick_info* stick = list_get(ctx->memory_sticks, i);
        agregar_a_paquete(paq, &(stick->id), sizeof(int));
        agregar_a_paquete(paq, &(stick->tamanio), sizeof(uint32_t));
        agregar_a_paquete(paq, &(stick->base_address), sizeof(uint32_t));
        agregar_a_paquete(paq, &(stick->puerto), sizeof(int));
        int ip_len = strlen(stick->ip) + 1;
        agregar_a_paquete(paq, &ip_len, sizeof(int));
        agregar_a_paquete(paq, stick->ip, ip_len);
    }
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

void atender_cliente(void *arg) {
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