#include "procesador_mensajes.h"
#include "auxiliares_memoria.h"

void procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx) {
    char *mensaje = recibir_handshake(cliente_fd);
    
    if (string_starts_with(mensaje, "STICK|")) {
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
        combinar_huecos_libres(ctx->huecos_libres);
        
        ctx->memoria_total += stick_size;
        log_info(ctx->logger, "## Memory Stick de %d bytes Conectada en %s:%d. Memoria total: %d", stick_size, stick_ip, stick_puerto, ctx->memoria_total);
        
        pthread_mutex_unlock(&ctx->mutex_memoria);
        
        string_array_destroy(partes);
    }
    else if (string_equals_ignore_case(mensaje, HANDSHAKE_SCHEDULER)) {
        log_info(ctx->logger, "## Kernel Scheduler Conectado - FD del socket: %d", cliente_fd);
    }
    else if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        log_info(ctx->logger, "## CPU %s Conectada", mensaje);
        
        // Enviar lista de memory sticks a la CPU
        pthread_mutex_lock(&ctx->mutex_memoria);
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = ENVIO_TABLA_STICKS; // Requiere opcode nuevo, o enviarlo como mensaje. Mejor opcode
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
    else if (string_starts_with(mensaje, "SWAP|")) {
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
    else { 
        log_info(ctx->logger, "## Módulo Conectado: %s", mensaje);
    }
    free(mensaje);
}

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
    
    char *path_completo = string_new();
    string_append(&path_completo, ctx->base_path);
    string_append(&path_completo, "/");
    string_append(&path_completo, path);
    
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
    
    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->id == 0 || seg->tamanio == 0) continue; 
        
        int cant_bloques_necesarios = (seg->tamanio + ctx->swap_block_size - 1) / ctx->swap_block_size;
        int bloque_inicio = -1;
        
        int cant_bloques_swap = ctx->swap_file_size / ctx->swap_block_size;
        for (int b = 0; b <= cant_bloques_swap - cant_bloques_necesarios; b++) {
            bool libre = true;
            for (int j = 0; j < cant_bloques_necesarios; j++) {
                if (bitarray_test_bit(ctx->swap_bitmap, b + j)) {
                    libre = false;
                    break;
                }
            }
            if (libre) {
                bloque_inicio = b;
                break;
            }
        }
        
        if (bloque_inicio == -1) {
            log_error(ctx->logger, "No hay espacio en SWAP para el segmento %d del PID %d", seg->id, pid);
            continue;
        }
        
        for (int j = 0; j < cant_bloques_necesarios; j++) {
            bitarray_set_bit(ctx->swap_bitmap, bloque_inicio + j);
        }
        
        seg->bloque_swap = bloque_inicio;
        
        void* buffer_seg = leer_memoria_distribuida(ctx, seg->base, seg->tamanio);
        
        int offset = 0;
        for (int j = 0; j < cant_bloques_necesarios; j++) {
            void* bloque_data = malloc(ctx->swap_block_size);
            memset(bloque_data, 0, ctx->swap_block_size);
            int copy_size = (offset + ctx->swap_block_size <= seg->tamanio) ? ctx->swap_block_size : (seg->tamanio - offset);
            memcpy(bloque_data, buffer_seg + offset, copy_size);
            
            enviar_int(ctx->swap_socket, ESCRITURA);
            enviar_int(ctx->swap_socket, bloque_inicio + j);
            enviar_int(ctx->swap_socket, ctx->swap_block_size);
            send(ctx->swap_socket, bloque_data, ctx->swap_block_size, 0);
            
            recibir_mensaje(ctx->swap_socket, ctx->logger);
            free(bloque_data);
            offset += copy_size;
        }
        free(buffer_seg);
        
        // Liberar espacio en memoria principal
        for (int h = 0; h < list_size(ctx->segmentos_globales); h++) {
            t_segmento* gs = list_get(ctx->segmentos_globales, h);
            if (gs->base == seg->base && gs->id == seg->id) {
                t_hueco_libre* hueco = malloc(sizeof(t_hueco_libre));
                hueco->base = seg->base;
                hueco->limite = seg->base + seg->tamanio;
                list_add(ctx->huecos_libres, hueco);
                
                list_remove_and_destroy_element(ctx->segmentos_globales, h, free);
                break;
            }
        }
    }
    
    // Consolidar huecos si es posible
    // (Omitido para brevedad o se puede llamar a una funcion de consolidar)
    
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
    
    pthread_mutex_lock(&ctx->mutex_memoria);
    
    bool hay_espacio = true;
    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->id == 0 || seg->tamanio == 0) continue;
        
        // Buscar hueco en memoria principal (First Fit para simplificar)
        t_hueco_libre* best_hueco = NULL;
        for (int j = 0; j < list_size(ctx->huecos_libres); j++) {
            t_hueco_libre* h = list_get(ctx->huecos_libres, j);
            if ((h->limite - h->base) >= seg->tamanio) {
                best_hueco = h;
                break;
            }
        }
        if (!best_hueco) {
            hay_espacio = false;
            break;
        }
    }
    
    if (!hay_espacio) {
        pthread_mutex_unlock(&ctx->mutex_memoria);
        enviar_mensaje("ERROR", cliente_fd);
        log_error(ctx->logger, "No hay espacio en Memoria Principal para desuspender PID %d", pid);
        return;
    }
    
    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->id == 0 || seg->tamanio == 0) continue;
        
        t_hueco_libre* best_hueco = NULL;
        for (int j = 0; j < list_size(ctx->huecos_libres); j++) {
            t_hueco_libre* h = list_get(ctx->huecos_libres, j);
            if ((h->limite - h->base) >= seg->tamanio) {
                best_hueco = h;
                break;
            }
        }
        
        // Asignar base
        seg->base = best_hueco->base;
        best_hueco->base += seg->tamanio;
        if (best_hueco->base == best_hueco->limite) {
            list_remove_element(ctx->huecos_libres, best_hueco);
            free(best_hueco);
        }
        
        t_segmento* nuevo_seg = malloc(sizeof(t_segmento));
        nuevo_seg->id = seg->id;
        nuevo_seg->tamanio = seg->tamanio;
        nuevo_seg->base = seg->base;
        nuevo_seg->bloque_swap = -1;
        list_add(ctx->segmentos_globales, nuevo_seg);
        
        // Leer de SWAP y escribir en Memoria Principal
        int cant_bloques = (seg->tamanio + ctx->swap_block_size - 1) / ctx->swap_block_size;
        int offset = 0;
        void* buffer_seg = malloc(seg->tamanio);
        
        for (int j = 0; j < cant_bloques; j++) {
            enviar_int(ctx->swap_socket, LECTURA);
            enviar_int(ctx->swap_socket, seg->bloque_swap + j);
            
            int opcode = recibir_operacion(ctx->swap_socket);
            if (opcode == LECTURA) {
                int size_buf;
                void* res = recibir_buffer(&size_buf, ctx->swap_socket);
                int copy_size = (offset + ctx->swap_block_size <= seg->tamanio) ? ctx->swap_block_size : (seg->tamanio - offset);
                memcpy(buffer_seg + offset, res, copy_size);
                free(res);
            }
            offset += ctx->swap_block_size;
            bitarray_clean_bit(ctx->swap_bitmap, seg->bloque_swap + j);
        }
        
        escribir_memoria_distribuida(ctx, seg->base, seg->tamanio, buffer_seg);
        free(buffer_seg);
        seg->bloque_swap = -1;
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
    t_segmento* nuevo_seg = crear_segmento(ctx, id_seg, tamanio_seg, &needs_compaction);
                
    if (needs_compaction) {
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = SOLICITUD_COMPACTACION;
        enviar_paquete(paq, cliente_fd);
        eliminar_paquete(paq);
                    
        int res = recibir_operacion(cliente_fd);
        if (res == RESPUESTA_COMPACTACION) {
            compactar_memoria(ctx);
            nuevo_seg = crear_segmento(ctx, id_seg, tamanio_seg, &needs_compaction);
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
            log_info(ctx->logger, "DEBUG: Added segment to PID %d. Total segments now: %d", pid, list_size(prog->contexto->tabla_segmentos));
            pthread_mutex_unlock(&ctx->mutex_programas);

        } else {

            log_error(ctx->logger, "DEBUG: prog was NULL for PID %d in CREAR_SEGMENTO!", pid);
        }
            enviar_mensaje(MSG_OK, cliente_fd);

    } else {

        enviar_mensaje("OUT_OF_MEMORY", cliente_fd);

    }
    pthread_mutex_unlock(&ctx->mutex_memoria);
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
            {
                t_list* paquete = recibir_paquete(cliente_fd);
                uint32_t pid_a_liberar = *(uint32_t*)list_get(paquete, 0);
                list_destroy_and_destroy_elements(paquete, free);

                pthread_mutex_lock(&ctx->mutex_memoria);
                t_programa* prog = buscar_programa(ctx, pid_a_liberar);
                if (prog != NULL && prog->contexto->tabla_segmentos != NULL) {
                    pthread_mutex_lock(&ctx->mutex_programas);
                    for (int i = 0; i < list_size(prog->contexto->tabla_segmentos); i++) {
                        t_segmento* seg = list_get(prog->contexto->tabla_segmentos, i);
                        eliminar_segmento(ctx, seg);
                    }
                    list_clean(prog->contexto->tabla_segmentos);
                    pthread_mutex_unlock(&ctx->mutex_programas);
                }
                log_info(ctx->logger, "Proceso %d liberado de la memoria", pid_a_liberar);
                pthread_mutex_unlock(&ctx->mutex_memoria);
                break;
            }
            case ELIMINAR_SEGMENTO:
            {
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
                        eliminar_segmento(ctx, seg_a_eliminar);
                        enviar_mensaje(MSG_OK, cliente_fd);
                    } else {
                        enviar_mensaje("SEG_NOT_FOUND", cliente_fd);
                    }
                } else {
                    enviar_mensaje("PROG_NOT_FOUND", cliente_fd);
                }
                pthread_mutex_unlock(&ctx->mutex_memoria);
                break;
            }
            case SOLICITUD_TAM_MAX_SEGMENTO:
            {
                t_paquete* paq = crear_paquete();
                paq->codigo_operacion = TAM_MAX_SEGMENTO;
                agregar_a_paquete(paq, &ctx->segment_max_size, sizeof(int));
                enviar_paquete(paq, cliente_fd);
                eliminar_paquete(paq);
                break;
            }
            case LEER_MEMORIA:
            {
                int size;
                void* buffer = recibir_buffer(&size, cliente_fd);
                uint32_t dir_fisica, tamanio;
                int desp = 0;
                memcpy(&dir_fisica, buffer + desp, sizeof(uint32_t)); desp += sizeof(uint32_t);
                memcpy(&tamanio, buffer + desp, sizeof(uint32_t));
                free(buffer);

                pthread_mutex_lock(&ctx->mutex_memoria);
                void* data_leida = leer_memoria_distribuida(ctx, dir_fisica, tamanio);
                pthread_mutex_unlock(&ctx->mutex_memoria);

                t_paquete* paq = crear_paquete();
                paq->codigo_operacion = MENSAJE; 
                agregar_a_paquete(paq, data_leida, tamanio);
                enviar_paquete(paq, cliente_fd);
                eliminar_paquete(paq);
                free(data_leida);
                
                break;
            }
            case ESCRIBIR_MEMORIA:
            {
                int size;
                void* buffer = recibir_buffer(&size, cliente_fd);
                uint32_t dir_fisica, tamanio;
                int desp = 0;
                memcpy(&dir_fisica, buffer + desp, sizeof(uint32_t)); desp += sizeof(uint32_t);
                memcpy(&tamanio, buffer + desp, sizeof(uint32_t)); desp += sizeof(uint32_t);
                
                int tamanio_rep;
                memcpy(&tamanio_rep, buffer + desp, sizeof(int)); desp += sizeof(int);
                
                pthread_mutex_lock(&ctx->mutex_memoria);
                escribir_memoria_distribuida(ctx, dir_fisica, tamanio, buffer + desp);
                pthread_mutex_unlock(&ctx->mutex_memoria);
                
                free(buffer);
                enviar_mensaje(MSG_OK, cliente_fd);
                break;
            }
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
