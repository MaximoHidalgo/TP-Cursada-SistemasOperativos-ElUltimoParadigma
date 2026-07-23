#include "pcb.h"
#include <stdlib.h>

t_proceso_planificable* pcb_crear(int pid, int prioridad) {
    t_proceso_planificable* pcb = malloc(sizeof(t_proceso_planificable));
    if (pcb != NULL) {
        pcb->pid = pid;
        pcb->cola_prioridad = prioridad;
        pcb->prioridad_original = prioridad;
        pcb->bloqueo_id = 0;
    }
    return pcb;
}

void pcb_destruir(t_proceso_planificable* pcb) {
    if (pcb != NULL) {
        free(pcb);
    }
}

const char* planificador_nombre_estado(t_estado_proceso estado) {
    switch (estado) {
        case ESTADO_PROCESO_NEW:
            return "NEW";
        case ESTADO_PROCESO_READY:
            return "READY";
        case ESTADO_PROCESO_EXEC:
            return "EXEC";
        case ESTADO_PROCESO_BLOCK:
            return "BLOCK";
        case ESTADO_PROCESO_EXIT:
            return "EXIT";
        case ESTADO_PROCESO_SUSP_READY:
            return "SUSP_READY";
        case ESTADO_PROCESO_SUSP_BLOCK:
            return "SUSP_BLOCK";
        default:
            return "DESCONOCIDO";
    }
}