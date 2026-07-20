#ifndef AUXILIARES_MEMORIA_H
#define AUXILIARES_MEMORIA_H

#include "estructuras_memory.h"
#include <utils/dominio/estructuras.h>
#include <commons/string.h>
#include <unistd.h>

bool comparar_por_base(void* a, void* b);
void fusionar_huecos_libres(t_list* huecos_libres);
t_hueco_libre* buscar_hueco_libre_x_lista(t_list* huecos_libres, uint32_t tamanio, t_memory_context* ctx);

void notificar_memoria_corrupta(int scheduler_socket);
void notificar_memoria_expandida(int scheduler_socket);

bool hay_espacio_para_restaurar_segmentos(t_memory_context* ctx, t_contexto_ejecucion* proc);
bool requiere_compactacion(t_memory_context* ctx, uint32_t tamanio);
t_hueco_libre* buscar_hueco_libre(t_memory_context* ctx, uint32_t tamanio);

bool restaurar_segmentos(t_contexto_ejecucion* proc, t_memory_context* ctx, int cliente_fd);
t_segmento* crear_segmento(t_memory_context* ctx, uint32_t pid, uint32_t id, uint32_t tamanio, int* needs_compaction);
void eliminar_segmento(t_memory_context* ctx, uint32_t pid, t_segmento* segmento);

bool ordenar_segmentos_por_base(void* a, void* b);
void compactar_memoria(t_memory_context* ctx);
bool solicitar_compactacion_al_scheduler(int cliente_fd, t_memory_context* ctx);
uint32_t calcular_memoria_necesaria(t_contexto_ejecucion* proc);

void* leer_memoria_distribuida(t_memory_context* ctx, uint32_t dir_fisica, uint32_t tamanio);
void escribir_memoria_distribuida(t_memory_context* ctx, uint32_t dir_fisica, uint32_t tamanio, void* buffer);

void liberar_segmento_de_memoria(t_memory_context* ctx, t_segmento* seg);
void escribir_segmento_en_swap(t_memory_context* ctx, t_segmento* seg, int bloque_inicio, void* buffer_seg, int cant_bloques_necesarios);
void suspender_proceso(t_memory_context* ctx, t_contexto_ejecucion* proc, int cant_bloques_swap);
bool hay_espacio_para_suspender_proceso(t_memory_context* ctx, t_contexto_ejecucion* proc);

bool gestionar_suspension(t_memory_context* ctx, t_contexto_ejecucion* proc);
bool gestionar_desuspension(t_memory_context* ctx, t_contexto_ejecucion* proc, int cliente_fd);
void serializar_tabla_sticks_en_paquete(t_paquete* paq, t_list* memory_sticks);
t_segmento* buscar_y_remover_segmento(t_list* tabla_segmentos, uint32_t id_seg);

#endif
