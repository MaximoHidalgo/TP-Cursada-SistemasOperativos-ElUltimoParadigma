#ifndef GESTION_HUECOS_Y_COMPACTACION_H
#define GESTION_HUECOS_Y_COMPACTACION_H

#include "estructuras_memory.h"
#include <utils/dominio/estructuras.h>

bool comparar_por_base(void* a, void* b);
void fusionar_huecos_libres(t_list* huecos_libres);
t_hueco_libre* buscar_hueco_libre_x_lista(t_list* huecos_libres, uint32_t tamanio, t_memory_context* ctx);
t_hueco_libre* buscar_hueco_libre(t_memory_context* ctx, uint32_t tamanio);

bool ordenar_segmentos_por_base(void* a, void* b);
void compactar_memoria(t_memory_context* ctx);

#endif
