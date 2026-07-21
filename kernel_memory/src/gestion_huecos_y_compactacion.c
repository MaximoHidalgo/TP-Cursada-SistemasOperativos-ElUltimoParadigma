#include "gestion_huecos_y_compactacion.h"
#include "memoria_distribuida.h"
#include <commons/string.h>
#include <unistd.h>
#include <pthread.h>

bool comparar_por_base(void* a, void* b)
{
    t_hueco_libre* h1 = (t_hueco_libre*) a;
    t_hueco_libre* h2 = (t_hueco_libre*) b;

    return h1->base < h2->base;
}

void fusionar_huecos_libres(t_list* huecos_libres){
    if (list_size(huecos_libres) < 2){
        return;
    }

    list_sort(huecos_libres, comparar_por_base);

    for (int i = 0; i < list_size(huecos_libres) - 1; )
    {
        t_hueco_libre* actual = list_get(huecos_libres, i);
        t_hueco_libre* siguiente = list_get(huecos_libres, i + 1);

        if (actual->limite >= siguiente->base)
        {
            actual->limite = siguiente->limite;

            list_remove(huecos_libres, i + 1);
            free(siguiente);
        }
        else
        {
            i++;
        }
    }                    
}

t_hueco_libre* buscar_hueco_libre_x_lista(t_list* huecos_libres, uint32_t tamanio, t_memory_context* ctx) {
    t_hueco_libre* hueco_elegido = NULL;

    for (int i = 0; i < list_size(huecos_libres); i++) {
        t_hueco_libre* hueco = list_get(huecos_libres, i);
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

t_hueco_libre* buscar_hueco_libre(t_memory_context* ctx, uint32_t tamanio) {
    return buscar_hueco_libre_x_lista(ctx->huecos_libres, tamanio, ctx);
}

bool ordenar_segmentos_por_base(void* a, void* b) {
    t_segmento* seg1 = (t_segmento*)a;
    t_segmento* seg2 = (t_segmento*)b;
    return seg1->base < seg2->base;
}

static void actualizar_tablas_segmentos(t_memory_context* ctx, uint32_t base_vieja, uint32_t id, uint32_t nueva_base)
{
    pthread_mutex_lock(&ctx->mutex_programas);
    for (int i = 0; i < list_size(ctx->programas); i++) {
        t_programa* prog = list_get(ctx->programas, i);
        for (int j = 0; j < list_size(prog->contexto->tabla_segmentos); j++) {
            t_segmento* seg = list_get(prog->contexto->tabla_segmentos, j);
            if (seg->base == base_vieja && seg->id == id) {
                seg->base = nueva_base;
                pthread_mutex_unlock(&ctx->mutex_programas);
                return;
            }
        }
    }
    pthread_mutex_unlock(&ctx->mutex_programas);
}

void compactar_memoria(t_memory_context* ctx) {
    log_info(ctx->logger, "## Inicio de compactación");
    usleep(ctx->compaction_delay * 1000);
    
    list_sort(ctx->segmentos_globales, ordenar_segmentos_por_base);
    
    uint32_t offset_actual = 0;
    for (int i = 0; i < list_size(ctx->segmentos_globales); i++) {
        t_segmento* seg = list_get(ctx->segmentos_globales, i);
        if (seg->base != offset_actual) {
            uint32_t base_vieja = seg->base;
            void* temp_buffer = leer_memoria_distribuida(ctx, seg->base, seg->tamanio);
            if (temp_buffer != NULL) {
                escribir_memoria_distribuida(ctx, offset_actual, seg->tamanio, temp_buffer);
                free(temp_buffer);
            }
            seg->base = offset_actual;

            actualizar_tablas_segmentos(ctx, base_vieja, seg->id, offset_actual);
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