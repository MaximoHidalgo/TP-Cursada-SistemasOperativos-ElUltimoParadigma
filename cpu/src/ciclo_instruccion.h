#ifndef CICLO_INSTRUCCION_H_
#define CICLO_INSTRUCCION_H_

#include "estructuras.h"
#include "estructuras_cpu.h"

void ejecutar_ciclo_de_instruccion(t_contexto_ejecucion* contexto, t_cpu_context* ctx);
void notificar_desalojo_a_scheduler(uint32_t pid, int motivo_desalojo, t_cpu_context* ctx);
void notificar_syscall_sleep_a_scheduler(uint32_t pid, uint32_t tiempo_sleep, t_cpu_context* ctx);

#endif //CICLO_INSTRUCCION_H_