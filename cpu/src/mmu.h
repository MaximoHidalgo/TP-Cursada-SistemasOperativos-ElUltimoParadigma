#ifndef MMU_H_
#define MMU_H_

#include "cpu_utils.h"
#include <math.h>

int mmu_traducir(t_cpu_context* ctx, t_contexto_ejecucion* contexto, int dir_logica, int tam_bytes, int tam_max_segmento, uint32_t* dir_fisica);
void* leer_memoria_distribuida_cpu(t_cpu_context* ctx, uint32_t dir_fisica, uint32_t tamanio);
void escribir_memoria_distribuida_cpu(t_cpu_context* ctx, uint32_t dir_fisica, uint32_t tamanio, void* buffer);

#endif // MMU_H_