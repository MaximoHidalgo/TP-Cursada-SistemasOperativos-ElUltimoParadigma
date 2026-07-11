#ifndef CPU_INIT_H
#define CPU_INIT_H

#include "cpu_utils.h"

t_cpu_context* iniciar_contexto(int argc, char* argv[]);
void limpiar_contexto(t_cpu_context* ctx);
void atender_kernel_scheduler(t_cpu_context* ctx);

#endif // CPU_INIT_H
