#ifndef AUXILIARES_MEMORIA_H
#define AUXILIARES_MEMORIA_H

#include "estructuras_memory.h"
#include <utils/dominio/estructuras.h>

t_hueco_libre* buscar_hueco_libre(t_memory_context* ctx, uint32_t tamanio);
bool comparar_por_base(void* a, void* b);
void fusionar_huecos_libres(t_list* huecos_libres);
t_segmento* crear_segmento(t_memory_context* ctx, uint32_t id, uint32_t tamanio, int* needs_compaction);
void eliminar_segmento(t_memory_context* ctx, t_segmento* segmento);
void compactar_memoria(t_memory_context* ctx);

void* leer_memoria_distribuida(t_memory_context* ctx, uint32_t dir_fisica, uint32_t tamanio);
void escribir_memoria_distribuida(t_memory_context* ctx, uint32_t dir_fisica, uint32_t tamanio, void* buffer);

#endif
