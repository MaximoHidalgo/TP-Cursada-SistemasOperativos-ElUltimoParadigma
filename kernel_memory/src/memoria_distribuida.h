#ifndef MEMORIA_DISTRIBUIDA_H
#define MEMORIA_DISTRIBUIDA_H

#include "estructuras_memory.h"

void* leer_memoria_distribuida(t_memory_context* ctx, uint32_t dir_fisica, uint32_t tamanio);
void escribir_memoria_distribuida(t_memory_context* ctx, uint32_t dir_fisica, uint32_t tamanio, void* buffer);
void serializar_tabla_sticks_en_paquete(t_paquete* paq, t_list* memory_sticks);

#endif
