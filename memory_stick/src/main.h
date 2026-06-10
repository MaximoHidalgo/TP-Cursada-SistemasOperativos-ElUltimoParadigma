#ifndef MEMORY_STICK_H_
#define MEMORY_STICK_H_

#include "atender_cpu.h"
#include <commons/string.h>

t_memory_stick_context* iniciar_contexto(int argc, char* argv[]);
void conectar_kernel_memory(t_memory_stick_context* ctx);
void iniciar_servidor_cpus(t_memory_stick_context* ctx);
void limpiar_contexto(t_memory_stick_context* ctx);

#endif // MEMORY_STICK_H_
