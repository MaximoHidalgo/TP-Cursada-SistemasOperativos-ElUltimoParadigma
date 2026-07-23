#ifndef PLANIFICADOR_RECURSOS_H
#define PLANIFICADOR_RECURSOS_H

#include "estructuras_scheduler.h"

void destruir_mutex(void* element);
void planificador_mutex_create(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso);
int planificador_mutex_lock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso);
int planificador_mutex_unlock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso);

#endif
