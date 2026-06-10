#include "auxiliares_memoria.h"
#include <commons/string.h>
#include <unistd.h>

t_hueco_libre* buscar_hueco_libre(t_memory_context* ctx, uint32_t tamanio) {
    t_hueco_libre* hueco_elegido = NULL;

    for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
        t_hueco_libre* hueco = list_get(ctx->huecos_libres, i);
        uint32_t tam_hueco = hueco->limite - hueco->base;

        if (tam_hueco >= tamanio) {
            if (string_equals_ignore_case(ctx->allocation_strategy, "FIRST")) {
                return hueco;
            } else if (string_equals_ignore_case(ctx->allocation_strategy, "BEST")) {
                if (hueco_elegido == NULL || tam_hueco < (hueco_elegido->limite - hueco_elegido->base)) {
                    hueco_elegido = hueco;
                }
            } else if (string_equals_ignore_case(ctx->allocation_strategy, "WORST")) {
                if (hueco_elegido == NULL || tam_hueco > (hueco_elegido->limite - hueco_elegido->base)) {
                    hueco_elegido = hueco;
                }
            }
        }
    }
    return hueco_elegido;
}

t_segmento* crear_segmento(t_memory_context* ctx, uint32_t id, uint32_t tamanio, int* needs_compaction) {
    if (tamanio > ctx->segment_max_size) {
        log_error(ctx->logger, "Tamanio solicitado %d mayor a SEGMENT_MAX_SIZE %d", tamanio, ctx->segment_max_size);
        return NULL;
    }

    t_hueco_libre* hueco = buscar_hueco_libre(ctx, tamanio);
    
    if (hueco == NULL) {
        uint32_t memoria_libre_total = 0;
        for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
            t_hueco_libre* h = list_get(ctx->huecos_libres, i);
            memoria_libre_total += (h->limite - h->base);
        }

        if (memoria_libre_total >= tamanio) {
            log_info(ctx->logger, "Espacio suficiente pero fragmentado. Compactacion requerida...");
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
    log_info(ctx->logger, "PID: <PID_FALTANTE> - Crear Segmento: %d - Base: %d - TAMAÑO: %d", id, segmento->base, tamanio);
    return segmento;
}

void eliminar_segmento(t_memory_context* ctx, t_segmento* segmento) {
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

    log_info(ctx->logger, "PID: <PID_FALTANTE> - Eliminar Segmento: %d - Base: %d - TAMAÑO: %d", segmento->id, segmento->base, segmento->tamanio);
    free(segmento);
}

bool ordenar_segmentos_por_base(void* a, void* b) {
    t_segmento* seg1 = (t_segmento*)a;
    t_segmento* seg2 = (t_segmento*)b;
    return seg1->base < seg2->base;
}

void compactar_memoria(t_memory_context* ctx) {
    log_info(ctx->logger, "## Inicio de compactación");
    usleep(config_get_int_value(ctx->config, "COMPACTION_DELAY") * 1000);
    
    list_sort(ctx->segmentos_globales, ordenar_segmentos_por_base);
    
    uint32_t offset_actual = 0;
    for (int i = 0; i < list_size(ctx->segmentos_globales); i++) {
        t_segmento* seg = list_get(ctx->segmentos_globales, i);
        if (seg->base != offset_actual) {
            memmove(ctx->espacio_memoria + offset_actual, ctx->espacio_memoria + seg->base, seg->tamanio);
            seg->base = offset_actual;
        }
        offset_actual += seg->tamanio;
    }
    
    list_clean_and_destroy_elements(ctx->huecos_libres, free);
    if (offset_actual < ctx->memoria_total) {
        t_hueco_libre* hueco_final = malloc(sizeof(t_hueco_libre));
        hueco_final->base = offset_actual;
        hueco_final->limite = ctx->memoria_total;
        list_add(ctx->huecos_libres, hueco_final);
    }
    
    log_info(ctx->logger, "## Fin de compactación");
}
