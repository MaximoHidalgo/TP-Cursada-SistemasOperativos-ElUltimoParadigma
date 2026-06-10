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

t_segmento* crear_segmento(t_memory_context* ctx, uint32_t id, uint32_t tamanio) {
    if (tamanio > ctx->segment_max_size) {
        log_error(ctx->logger, "Tamanio solicitado %d mayor a SEGMENT_MAX_SIZE %d", tamanio, ctx->segment_max_size);
        return NULL;
    }

    t_hueco_libre* hueco = buscar_hueco_libre(ctx, tamanio);
    
    if (hueco == NULL) {
        // Necesitamos compactar o no hay memoria
        // Verificamos si hay espacio sumando todos los huecos
        uint32_t memoria_libre_total = 0;
        for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
            t_hueco_libre* h = list_get(ctx->huecos_libres, i);
            memoria_libre_total += (h->limite - h->base);
        }

        if (memoria_libre_total >= tamanio) {
            log_info(ctx->logger, "Espacio suficiente pero fragmentado. Iniciando compactacion...");
            compactar_memoria(ctx);
            // Re-intentar buscar hueco después de compactar
            hueco = buscar_hueco_libre(ctx, tamanio);
        }
        
        if (hueco == NULL) {
            log_error(ctx->logger, "Out of Memory para segmento %d de %d bytes", id, tamanio);
            return NULL; // OOM
        }
    }

    t_segmento* segmento = malloc(sizeof(t_segmento));
    segmento->id = id;
    segmento->base = hueco->base;
    segmento->tamanio = tamanio;

    // Actualizamos el hueco
    hueco->base += tamanio;
    if (hueco->base == hueco->limite) {
        // Eliminar el hueco porque se consumió entero
        for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
            if (list_get(ctx->huecos_libres, i) == hueco) {
                list_remove_and_destroy_element(ctx->huecos_libres, i, free);
                break;
            }
        }
    }

    list_add(ctx->segmentos_globales, segmento);
    
    // Log obligatorio
    log_info(ctx->logger, "PID: <PID_FALTANTE> - Crear Segmento: %d - Base: %d - TAMAÑO: %d", id, segmento->base, tamanio);
    return segmento;
}

void eliminar_segmento(t_memory_context* ctx, t_segmento* segmento) {
    // Buscar y eliminar de segmentos_globales
    for (int i = 0; i < list_size(ctx->segmentos_globales); i++) {
        if (list_get(ctx->segmentos_globales, i) == segmento) {
            list_remove(ctx->segmentos_globales, i);
            break;
        }
    }

    // Crear un nuevo hueco
    t_hueco_libre* nuevo_hueco = malloc(sizeof(t_hueco_libre));
    nuevo_hueco->base = segmento->base;
    nuevo_hueco->limite = segmento->base + segmento->tamanio;
    list_add(ctx->huecos_libres, nuevo_hueco);

    log_info(ctx->logger, "PID: <PID_FALTANTE> - Eliminar Segmento: %d - Base: %d - TAMAÑO: %d", segmento->id, segmento->base, segmento->tamanio);
    free(segmento);

    // Consolidar huecos contiguos
    // (Por simplicidad en la primera versión lo omitiremos, la compactación lo resolverá)
}

void compactar_memoria(t_memory_context* ctx) {
    // Usar usleep para el retardo de compactación
    usleep(config_get_int_value(ctx->config, "COMPACTION_DELAY") * 1000);
    // TODO: Implementar compactacion moviendo bloques en espacio_memoria
}
