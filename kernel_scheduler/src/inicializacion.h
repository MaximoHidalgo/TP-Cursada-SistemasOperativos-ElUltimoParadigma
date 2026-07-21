#ifndef INICIALIZACION_H
#define INICIALIZACION_H

#include "estructuras_scheduler.h"
#include "./commons/log.h"
#include "./commons/config.h"
#include "planificador.h"
#include "pcb.h"
#include "scheduler_syscalls.h"
#include <utils/dominio/opcodes.h>
#include <utils/dominio/estructuras.h>
#include <utils/sockets/sockets.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "utils_scheduler.h"
#include "planificador_memoria.h"

t_scheduler_context* iniciar_contexto(int, char**);
void iniciar_planificacion(t_scheduler_context* ctx); 
void conectar_kernel_memory(t_scheduler_context* );

#endif