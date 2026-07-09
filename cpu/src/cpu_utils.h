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
void atender_kernel_scheduler(t_cpu_context* ctx);

#endif //CPU_H