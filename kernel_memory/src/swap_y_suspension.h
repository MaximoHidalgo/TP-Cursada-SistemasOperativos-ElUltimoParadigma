#ifndef SWAP_Y_SUSPENSION_H
#define SWAP_Y_SUSPENSION_H

#include "estructuras_memory.h"
#include <utils/dominio/estructuras.h>

bool requiere_compactacion(t_memory_context* ctx, uint32_t tamanio);
uint32_t calcular_memoria_necesaria(t_contexto_ejecucion* proc);
bool hay_espacio_para_restaurar_segmentos(t_memory_context* ctx, t_contexto_ejecucion* proc);
bool restaurar_segmentos(t_contexto_ejecucion* proc, t_memory_context* ctx, int cliente_fd);
void escribir_segmento_en_swap(t_memory_context* ctx, t_segmento* seg, int bloque_inicio, void* buffer_seg, int cant_bloques_necesarios);
void suspender_proceso(t_memory_context* ctx, t_contexto_ejecucion* proc, int cant_bloques_swap);
bool hay_espacio_para_suspender_proceso(t_memory_context* ctx, t_contexto_ejecucion* proc);
bool gestionar_suspension(t_memory_context* ctx, t_contexto_ejecucion* proc);
bool gestionar_desuspension(t_memory_context* ctx, t_contexto_ejecucion* proc, int cliente_fd);

#endif
