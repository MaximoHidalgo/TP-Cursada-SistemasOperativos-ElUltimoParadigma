#ifndef SCHEDULER_OPERACIONES_H
#define SCHEDULER_OPERACIONES_H

#include "estructuras_scheduler.h"
#include "planificador_io.h"
#include "planificador_memoria.h"
#include "planificador.h"
void despachar_proceso_a_cpu(t_scheduler_context* ctx, int socket_cpu);
void procesar_fin_quantum(int cliente_fd, t_scheduler_context* ctx);
void devolver_ejecucion_a_cpu(t_scheduler_context* ctx, int cliente_fd);
void procesar_mensaje_handshake(int cliente_fd, t_scheduler_context* ctx);

#endif