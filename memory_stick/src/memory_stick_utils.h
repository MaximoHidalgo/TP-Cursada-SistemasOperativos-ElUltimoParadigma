#ifndef MEMORY_STICK_UTILS_H_
#define MEMORY_STICK_UTILS_H_

#include "atender_cpu.h"
#include "estructuras.h"
#include <commons/string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

t_memory_stick_context* inicializar_entorno(int argc, char* argv[]);
void conectar_kernel_memory(t_memory_stick_context* ctx);
void iniciar_servidor_cpus(t_memory_stick_context* ctx);
void limpiar_contexto(t_memory_stick_context* ctx);

#endif // MEMORY_STICK_UTILS_H_
