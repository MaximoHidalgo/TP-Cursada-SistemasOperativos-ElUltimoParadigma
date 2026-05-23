#ifndef RR_H_
#define RR_H_

#include <stdbool.h>
#include "planificador.h"

bool rr_iniciar_quantum(t_estratega_planificacion* estratega, t_proceso_planificable* proceso, int socket_cpu);

#endif
