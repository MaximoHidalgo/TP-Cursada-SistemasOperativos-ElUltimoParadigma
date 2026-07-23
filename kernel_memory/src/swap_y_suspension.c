#include "swap_y_suspension.h"

/*
 Busca un rango contiguo de bloques libres en un bitmap de SWAP.
 Retorna el índice del primer bloque libre, o -1 si no hay espacio.
*/
static int buscar_bloques_libres_en_swap(t_bitarray* bitmap, int total_bloques, int cant_bloques_necesarios) {
    for (int b = 0; b <= total_bloques - cant_bloques_necesarios; b++) {
        bool libre = true;
        for (int j = 0; j < cant_bloques_necesarios; j++) {
            if (bitarray_test_bit(bitmap, b + j)) {
                libre = false;
                break;
            }
        }
        if (libre) return b;
    }
    return -1;
}

bool hay_espacio_para_restaurar_segmentos(t_memory_context* ctx, t_contexto_ejecucion* proc){
    bool hay_espacio = true;

    t_list* huecos_aux = list_create();

    //simulo la asignacion para saber si hay espacio
    for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
        t_hueco_libre* h = list_get(ctx->huecos_libres, i);

        t_hueco_libre* copia = malloc(sizeof(t_hueco_libre));
        copia->base = h->base;
        copia->limite = h->limite;

        list_add(huecos_aux, copia);
    }

    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->tamanio == 0) continue;

        t_hueco_libre* hueco_libre = buscar_hueco_libre_x_lista(huecos_aux, seg->tamanio, ctx);
        if (!hueco_libre) {
            hay_espacio = false;
            break;
        }

        hueco_libre->base += seg->tamanio;

        if (hueco_libre->base == hueco_libre->limite) {
            list_remove_element(huecos_aux, hueco_libre);
            free(hueco_libre);
        }
    }
    list_destroy_and_destroy_elements(huecos_aux, free);

    return hay_espacio;
}

bool requiere_compactacion(t_memory_context* ctx, uint32_t tamanio){
    bool necesita_compactar = false;

    uint32_t memoria_libre_total = 0;
    for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
        t_hueco_libre* h = list_get(ctx->huecos_libres, i);
        memoria_libre_total += (h->limite - h->base);
    }

    if (memoria_libre_total >= tamanio) {
        log_info(ctx->logger, "Espacio suficiente pero fragmentado. Compactacion requerida...");
        necesita_compactar = true;
    }

    return necesita_compactar;
    
}

bool restaurar_segmentos(t_contexto_ejecucion* proc, t_memory_context* ctx, int cliente_fd){
    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->tamanio == 0) continue;
        
        t_hueco_libre* hueco_disponible = buscar_hueco_libre(ctx, seg->tamanio);

        if (hueco_disponible == NULL) {
            enviar_mensaje("ERROR", cliente_fd);
            log_error(ctx->logger, "Error interno: no se encontró un hueco libre");
            return false;
        }
        
        // Asignar base
        seg->base = hueco_disponible->base;
        hueco_disponible->base += seg->tamanio;
        if (hueco_disponible->base == hueco_disponible->limite) {
            list_remove_element(ctx->huecos_libres, hueco_disponible);
            free(hueco_disponible);
        }
        
        list_add(ctx->segmentos_globales, seg);
        
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

    return true;
}

void escribir_segmento_en_swap(t_memory_context* ctx, t_segmento* seg, int bloque_inicio, void* buffer_seg, int cant_bloques_necesarios){

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
            
        int r = recibir_operacion(ctx->swap_socket);
        if (r == MENSAJE) {
            int s; char* m = recibir_buffer(&s, ctx->swap_socket); free(m);
        }
        free(bloque_data);
        offset += copy_size;
    }
    free(buffer_seg);
}

void suspender_proceso(t_memory_context* ctx, t_contexto_ejecucion* proc, int cant_bloques_swap){

    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->tamanio == 0) continue;
        
        int cant_bloques_necesarios = (seg->tamanio + ctx->swap_block_size - 1) / ctx->swap_block_size;
        int bloque_inicio = buscar_bloques_libres_en_swap(ctx->swap_bitmap, cant_bloques_swap, cant_bloques_necesarios);
        
        if (bloque_inicio == -1) {
            log_error(ctx->logger, "Error interno: la verificación previa indicó que había espacio en SWAP");
            return;
        }
        
        for (int j = 0; j < cant_bloques_necesarios; j++) {
            bitarray_set_bit(ctx->swap_bitmap, bloque_inicio + j);
        }
        
        seg->bloque_swap = bloque_inicio;
        
        void* buffer_seg = leer_memoria_distribuida(ctx, seg->base, seg->tamanio);
        
        escribir_segmento_en_swap(ctx, seg, bloque_inicio, buffer_seg, cant_bloques_necesarios);
        
        liberar_segmento_de_memoria(ctx, seg);
    }

}

bool hay_espacio_para_suspender_proceso(t_memory_context* ctx, t_contexto_ejecucion* proc)
{
    int cant_bloques_swap = ctx->swap_file_size / ctx->swap_block_size;

    int bitmap_size = (cant_bloques_swap + 7) / 8;
    void* bitmap_aux_data = malloc(bitmap_size);
    memcpy(bitmap_aux_data, ctx->bitarray_data, bitmap_size);

    t_bitarray* bitmap_aux = bitarray_create_with_mode(
        bitmap_aux_data,
        bitmap_size,
        LSB_FIRST
    );

    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {

        t_segmento* seg = list_get(proc->tabla_segmentos, i);

        if (seg->tamanio == 0)
            continue;

        int cant_bloques_necesarios =
            (seg->tamanio + ctx->swap_block_size - 1) / ctx->swap_block_size;

        int bloque_inicio = buscar_bloques_libres_en_swap(bitmap_aux, cant_bloques_swap, cant_bloques_necesarios);

        if (bloque_inicio == -1) {
            bitarray_destroy(bitmap_aux);
            free(bitmap_aux_data);
            return false;
        }

        for (int j = 0; j < cant_bloques_necesarios; j++) {
            bitarray_set_bit(bitmap_aux, bloque_inicio + j);
        }
    }

    bitarray_destroy(bitmap_aux);
    free(bitmap_aux_data);

    return true;
}

uint32_t calcular_memoria_necesaria(t_contexto_ejecucion* proc) {
    uint32_t memoria_necesaria = 0;
    for (int k = 0; k < list_size(proc->tabla_segmentos); k++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, k);
        if (seg->tamanio == 0) continue;
        memoria_necesaria += seg->tamanio;
    }
    return memoria_necesaria;
}

bool gestionar_suspension(t_memory_context* ctx, t_contexto_ejecucion* proc) {
    pthread_mutex_lock(&ctx->mutex_memoria);

    int cant_bloques_swap = ctx->swap_file_size / ctx->swap_block_size;
    bool hay_espacio_swap = hay_espacio_para_suspender_proceso(ctx, proc);

    if (hay_espacio_swap) {
        suspender_proceso(ctx, proc, cant_bloques_swap);
        pthread_mutex_unlock(&ctx->mutex_memoria);
        return true;
    } else {
        pthread_mutex_unlock(&ctx->mutex_memoria);
        return false;
    }
}

bool gestionar_desuspension(t_memory_context* ctx, t_contexto_ejecucion* proc, int cliente_fd) {
    uint32_t memoria_necesaria = calcular_memoria_necesaria(proc);
    
    pthread_mutex_lock(&ctx->mutex_memoria);
    bool hay_espacio = hay_espacio_para_restaurar_segmentos(ctx, proc);
    
    if (!hay_espacio) {
        if (requiere_compactacion(ctx, memoria_necesaria)) {
            if (solicitar_compactacion_al_scheduler(cliente_fd, ctx)) {
                if (!hay_espacio_para_restaurar_segmentos(ctx, proc)) {
                    pthread_mutex_unlock(&ctx->mutex_memoria);
                    return false;
                }
            } else {
                pthread_mutex_unlock(&ctx->mutex_memoria);
                return false;
            }
        } else {
            pthread_mutex_unlock(&ctx->mutex_memoria);
            return false;
        }
    }

    if (!restaurar_segmentos(proc, ctx, cliente_fd)) {
        pthread_mutex_unlock(&ctx->mutex_memoria);
        return false;
    }
    
    pthread_mutex_unlock(&ctx->mutex_memoria);
    return true;
}