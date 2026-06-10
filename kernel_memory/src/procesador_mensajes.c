#include "procesador_mensajes.h"
#include "auxiliares_memoria.h"

void procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx) {
    char *mensaje = recibir_handshake(cliente_fd);
    
    if (string_starts_with(mensaje, "STICK|")) {
        char **partes = string_split(mensaje, "|");
        int stick_size = atoi(partes[1]);
        
        pthread_mutex_lock(&ctx->mutex_memoria);
        
        // Ampliar espacio
        void* nuevo_espacio = realloc(ctx->espacio_memoria, ctx->memoria_total + stick_size);
        if (nuevo_espacio) {
            ctx->espacio_memoria = nuevo_espacio;
            
            t_hueco_libre* nuevo_hueco = malloc(sizeof(t_hueco_libre));
            nuevo_hueco->base = ctx->memoria_total;
            nuevo_hueco->limite = ctx->memoria_total + stick_size;
            list_add(ctx->huecos_libres, nuevo_hueco);
            
            ctx->memoria_total += stick_size;
            log_info(ctx->logger, "## Memory Stick de %d bytes Conectada. Memoria total: %d", stick_size, ctx->memoria_total);
        } else {
            log_error(ctx->logger, "Error al hacer realloc de la memoria para el Memory Stick");
        }
        
        pthread_mutex_unlock(&ctx->mutex_memoria);
        
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
    deserializar_contexto(buffer, size, &contexto_actualizado);
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
            case CREAR_SEGMENTO:
            {
                int size;
                void* buffer = recibir_buffer(&size, cliente_fd);
                uint32_t pid, id_seg, tamanio_seg;
                int desp = 0;
                memcpy(&pid, buffer + desp, sizeof(uint32_t)); desp += sizeof(uint32_t);
                memcpy(&id_seg, buffer + desp, sizeof(uint32_t)); desp += sizeof(uint32_t);
                memcpy(&tamanio_seg, buffer + desp, sizeof(uint32_t));
                free(buffer);

                pthread_mutex_lock(&ctx->mutex_memoria);
                t_segmento* nuevo_seg = crear_segmento(ctx, id_seg, tamanio_seg);
                if (nuevo_seg != NULL) {
                    t_programa* prog = buscar_programa(ctx, pid);
                    if (prog != NULL) {
                        pthread_mutex_lock(&ctx->mutex_programas);
                        if (prog->contexto->tabla_segmentos == NULL) {
                            prog->contexto->tabla_segmentos = list_create();
                        }
                        list_add(prog->contexto->tabla_segmentos, nuevo_seg);
                        pthread_mutex_unlock(&ctx->mutex_programas);
                    }
                    enviar_mensaje(MSG_OK, cliente_fd);
                } else {
                    enviar_mensaje("OUT_OF_MEMORY", cliente_fd);
                }
                pthread_mutex_unlock(&ctx->mutex_memoria);
                break;
            }
            case ELIMINAR_SEGMENTO:
            {
                int size;
                void* buffer = recibir_buffer(&size, cliente_fd);
                uint32_t pid, id_seg;
                int desp = 0;
                memcpy(&pid, buffer + desp, sizeof(uint32_t)); desp += sizeof(uint32_t);
                memcpy(&id_seg, buffer + desp, sizeof(uint32_t));
                free(buffer);

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
                void* data_leida = malloc(tamanio);
                memcpy(data_leida, ctx->espacio_memoria + dir_fisica, tamanio);
                pthread_mutex_unlock(&ctx->mutex_memoria);

                t_paquete* paq = crear_paquete();
                paq->codigo_operacion = MENSAJE; // Enviamos como si fuera un mensaje con la data, o un op_code nuevo. No, utilizeremos buffer crudo
                // Para recibirlo en cpu, cpu espera ENVIO_DATOS o manda LEER_MEMORIA y usa recv()
                // Wait, el deserializador de utils NO contempla lectura de datos crudos. Espera MSG_OK?
                // Vamos a mandar MSG_OK y pegarle el contenido de data_leida.
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
                
                // Si mandamos el tamanio en la peticion por deserializar_peticion_escritura que lee buffer_read_int?
                // Revisemos deserializar_peticion_escritura... wait, en kernel_memory no usamos deserializar_peticion_escritura, 
                // lo desarmamos a mano. Pero cpu envia usando enviar_peticion_escritura que es:
                // buffer_write_uint32(dir_fisica), buffer_write_uint32(tamanio), buffer_write_int(tamanio), buffer_write(contenido, tamanio)
                // Wait! Enviar peticion escritura hace:
                // buffer_write_uint32(paquete->buffer, direccion_fisica);
                // buffer_write_uint32(paquete->buffer, tamanio);
                // buffer_write_int(paquete->buffer, tamanio); 
                // buffer_write(paquete->buffer, contenido, tamanio);
                
                int tamanio_rep;
                memcpy(&tamanio_rep, buffer + desp, sizeof(int)); desp += sizeof(int);
                
                pthread_mutex_lock(&ctx->mutex_memoria);
                memcpy(ctx->espacio_memoria + dir_fisica, buffer + desp, tamanio);
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
