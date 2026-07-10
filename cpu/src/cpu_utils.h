#ifndef CPU_H
#define CPU_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/opcodes.h>
#include "estructuras_cpu.h"
#include "ciclo_instruccion.h"

t_cpu_context* iniciar_contexto(int argc, char* argv[]);
void limpiar_contexto(t_cpu_context* ctx);
void actualizar_tabla_sticks(t_cpu_context* ctx);
void manejar_ejecucion_proceso(int pid_recibido, t_cpu_context* ctx);
int desalojar_proceso(t_contexto_ejecucion* contexto, t_cpu_context* ctx, op_code motivo);
void atender_kernel_scheduler(t_cpu_context* ctx);

void notificar_desalojo_a_scheduler(uint32_t pid, int motivo_desalojo, t_cpu_context* ctx);
void notificar_syscall_sleep_a_scheduler(uint32_t pid, uint32_t tiempo_sleep, t_cpu_context* ctx);

#endif //CPU_H