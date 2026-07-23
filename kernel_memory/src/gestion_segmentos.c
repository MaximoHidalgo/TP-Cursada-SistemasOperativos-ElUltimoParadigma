#include "gestion_segmentos.h"

t_segmento* crear_segmento(t_memory_context* ctx, uint32_t pid, uint32_t id, uint32_t tamanio, int* needs_compaction) {
    if (tamanio > ctx->segment_max_size) {
        log_error(ctx->logger, "Tamanio solicitado %d mayor a SEGMENT_MAX_SIZE %d", tamanio, ctx->segment_max_size);
        return NULL;
    }

    t_hueco_libre* hueco = buscar_hueco_libre(ctx, tamanio);
    
    if (hueco == NULL) {
        if (requiere_compactacion(ctx, tamanio)) {
            *needs_compaction = 1;
            return NULL;
        }
        
        log_error(ctx->logger, "Out of Memory para segmento %d de %d bytes", id, tamanio);
        return NULL; // OOM
    }

    t_segmento* segmento = malloc(sizeof(t_segmento));
    segmento->id = id;
    segmento->base = hueco->base;
    segmento->tamanio = tamanio;
    segmento->bloque_swap = -1;

    hueco->base += tamanio;
    if (hueco->base == hueco->limite) {
        for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
            if (list_get(ctx->huecos_libres, i) == hueco) {
                list_remove_and_destroy_element(ctx->huecos_libres, i, free);
                break;
            }
        }
    }

    list_add(ctx->segmentos_globales, segmento);
    // Log obligatorio del TP (strict format)
    log_info(ctx->logger, "## PID: %d - Segmento Creado %d - Tamaño: %d", pid, id, tamanio);
    return segmento;
}

void eliminar_segmento(t_memory_context* ctx, uint32_t pid, t_segmento* segmento) {
    for (int i = 0; i < list_size(ctx->segmentos_globales); i++) {
        if (list_get(ctx->segmentos_globales, i) == segmento) {
            list_remove(ctx->segmentos_globales, i);
            break;
        }
    }

    t_hueco_libre* nuevo_hueco = malloc(sizeof(t_hueco_libre));
    nuevo_hueco->base = segmento->base;
    nuevo_hueco->limite = segmento->base + segmento->tamanio;
    list_add(ctx->huecos_libres, nuevo_hueco);
    fusionar_huecos_libres(ctx->huecos_libres);

    log_info(ctx->logger, "## PID: %d - Eliminar Segmento: %d - Base: %d - TAMAÑO: %d", pid, segmento->id, segmento->base, segmento->tamanio);
    free(segmento);
}

void liberar_segmento_de_memoria(t_memory_context* ctx, t_segmento* seg){
    // Liberar espacio en memoria principal
    for (int h = 0; h < list_size(ctx->segmentos_globales); h++) {
        t_segmento* gs = list_get(ctx->segmentos_globales, h);
        if (gs->base == seg->base && gs->id == seg->id) {
            t_hueco_libre* hueco = malloc(sizeof(t_hueco_libre));
            hueco->base = seg->base;
            hueco->limite = seg->base + seg->tamanio;
            list_add(ctx->huecos_libres, hueco);
            fusionar_huecos_libres(ctx->huecos_libres);
                
            list_remove(ctx->segmentos_globales, h);
            break;
        }
    }
}

t_segmento* buscar_y_remover_segmento(t_list* tabla_segmentos, uint32_t id_seg) {
    if (tabla_segmentos == NULL) return NULL;
    
    for (int i = 0; i < list_size(tabla_segmentos); i++) {
        t_segmento* seg = list_get(tabla_segmentos, i);
        if (seg->id == id_seg) {
            return list_remove(tabla_segmentos, i);
        }
    }
    return NULL;
}