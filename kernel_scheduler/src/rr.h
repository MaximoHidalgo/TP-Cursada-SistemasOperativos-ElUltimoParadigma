#ifndef RR_H_
#define RR_H_

#include <stdbool.h>
#include "planificador.h"
#include <stdlib.h>
#include <time.h>
#include <utils/utils_client.h>

typedef struct {
    t_estratega_planificacion* estratega;
    int pid;
    int socket_cpu;
    int quantum_ms;
} t_args_quantum;

bool rr_iniciar_quantum(t_estratega_planificacion* estratega, t_proceso_planificable* proceso, int socket_cpu);

#endif
