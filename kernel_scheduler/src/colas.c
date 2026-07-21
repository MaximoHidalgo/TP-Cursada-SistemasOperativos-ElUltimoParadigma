#include "colas.h"

t_algoritmo_cola algoritmo_cola_desde_string(char* algoritmo) {
    if (algoritmo == NULL) {
        return ALGORITMO_COLA_INVALIDO;
    }

    if (string_equals_ignore_case(algoritmo, "FIFO")) {
        return ALGORITMO_COLA_FIFO;
    }

    if (string_equals_ignore_case(algoritmo, "RR")) {
        return ALGORITMO_COLA_RR;
    }

    return ALGORITMO_COLA_INVALIDO;
}

t_list* crear_colas_ready_desde_config(t_config* config, int quantum_rr, t_log* logger) {
    t_list* colas = list_create();
    char** algoritmos_colas = config_get_array_value(config, "QUEUES_ALGORITHMS");

    for (int i = 0; algoritmos_colas != NULL && algoritmos_colas[i] != NULL; i++) {
        t_algoritmo_cola algoritmo = algoritmo_cola_desde_string(algoritmos_colas[i]);

        if (algoritmo == ALGORITMO_COLA_INVALIDO) {
            log_error(logger, "Algoritmo de cola invalido en nivel %d: %s", i, algoritmos_colas[i]);
            string_array_destroy(algoritmos_colas);
            list_destroy_and_destroy_elements(colas, destruir_cola_planificacion);
            return NULL;
        }

        t_cola_planificacion* cola = calloc(1, sizeof(t_cola_planificacion));
        cola->nivel = i;
        cola->algoritmo = algoritmo;
        cola->quantum = algoritmo == ALGORITMO_COLA_RR ? quantum_rr : 0;
        cola->procesos = queue_create();
        list_add(colas, cola);
    }

    if (algoritmos_colas != NULL) {
        string_array_destroy(algoritmos_colas);
    }

    return colas;
}

void destruir_cola_planificacion(void* elemento) {
    t_cola_planificacion* cola = elemento;

    if (cola == NULL) {
        return;
    }

    queue_destroy(cola->procesos);
    free(cola);
}

t_cola_planificacion* obtener_cola_planificacion(t_list* colas, int cola_prioridad) {
    if (colas == NULL || cola_prioridad < 0 || cola_prioridad >= list_size(colas)) {
        return NULL;
    }

    return list_get(colas, cola_prioridad);
}

bool encolar_en_cola_ready(t_list* colas, t_proceso_planificable* proceso) {
    t_cola_planificacion* cola = obtener_cola_planificacion(colas, proceso != NULL ? proceso->cola_prioridad : -1);

    if (cola == NULL) {
        return false;
    }

    proceso->estado = ESTADO_PROCESO_READY;
    queue_push(cola->procesos, proceso);
    return true;
}

t_proceso_planificable* desencolar_siguiente_ready(t_list* colas) {
    if (colas == NULL) {
        return NULL;
    }

    for (int i = 0; i < list_size(colas); i++) {
        t_cola_planificacion* cola = list_get(colas, i);

        if (!queue_is_empty(cola->procesos)) {
            return queue_pop(cola->procesos);
        }
    }

    return NULL;
}

int quantum_para_cola(t_list* colas, int cola_prioridad) {
    t_cola_planificacion* cola = obtener_cola_planificacion(colas, cola_prioridad);

    if (cola == NULL) {
        return 0;
    }

    return cola->quantum;
}

const char* planificador_nombre_algoritmo_cola(t_algoritmo_cola algoritmo) {
    switch (algoritmo) {
    case ALGORITMO_COLA_FIFO:
        return "FIFO";
    case ALGORITMO_COLA_RR:
        return "RR";
    default:
        return "INVALIDO";
    }
}