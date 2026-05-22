#ifndef PCB_H_
#define PCB_H_

#include <stdint.h>

typedef enum {
    ESTADO_PROCESO_NEW,
    ESTADO_PROCESO_READY,
    ESTADO_PROCESO_EXEC
} t_estado_proceso;

typedef struct {
    int pid;
    int cola_prioridad;
    t_estado_proceso estado;
    uint64_t instante_ready;
} t_proceso_planificable;

const char* planificador_nombre_estado(t_estado_proceso estado);

#endif
