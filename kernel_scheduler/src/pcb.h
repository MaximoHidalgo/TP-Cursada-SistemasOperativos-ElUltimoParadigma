#ifndef PCB_H_
#define PCB_H_

#include <stdint.h>

typedef enum {
    ESTADO_PROCESO_NEW,
    ESTADO_PROCESO_READY,
    ESTADO_PROCESO_EXEC,
    ESTADO_PROCESO_BLOCK,
    ESTADO_PROCESO_EXIT,
    ESTADO_PROCESO_SUSP_READY,
    ESTADO_PROCESO_SUSP_BLOCK
} t_estado_proceso;

typedef struct {
    int pid;
    int cola_prioridad;
    t_estado_proceso estado;
    uint64_t instante_ready;
    uint32_t io_dir_fisica;
    uint32_t io_tamanio;
} t_proceso_planificable;

const char* planificador_nombre_estado(t_estado_proceso estado);

#endif
