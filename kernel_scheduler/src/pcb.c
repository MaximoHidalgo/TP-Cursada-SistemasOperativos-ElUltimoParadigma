#include "pcb.h"

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