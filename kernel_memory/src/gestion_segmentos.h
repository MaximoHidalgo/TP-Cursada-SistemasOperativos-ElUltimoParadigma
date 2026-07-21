#ifndef GESTION_SEGMENTOS_H
#define GESTION_SEGMENTOS_H

#include "estructuras_memory.h"
#include "gestion_huecos_y_compactacion.h"
#include "swap_y_suspension.h"

t_segmento* crear_segmento(t_memory_context* ctx, uint32_t pid, uint32_t id, uint32_t tamanio, int* needs_compaction);
void eliminar_segmento(t_memory_context* ctx, uint32_t pid, t_segmento* segmento);
void liberar_segmento_de_memoria(t_memory_context* ctx, t_segmento* seg);
t_segmento* buscar_y_remover_segmento(t_list* tabla_segmentos, uint32_t id_seg);

#endif
