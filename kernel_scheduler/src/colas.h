#ifndef COLAS_H_
#define COLAS_H_

#include <stdbool.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include <commons/log.h>
#include "pcb.h"

typedef enum {
    ALGORITMO_COLA_FIFO,
    ALGORITMO_COLA_RR,
    ALGORITMO_COLA_INVALIDO
} t_algoritmo_cola;

typedef struct {
    int nivel;
    t_algoritmo_cola algoritmo;
    int quantum;
    t_queue* procesos;
} t_cola_planificacion;

t_list* crear_colas_ready_desde_config(t_config* config, int quantum_rr, t_log* logger);
void destruir_cola_planificacion(void* elemento);

t_cola_planificacion* obtener_cola_planificacion(t_list* colas, int cola_prioridad);
bool encolar_en_cola_ready(t_list* colas, t_proceso_planificable* proceso);
t_proceso_planificable* desencolar_siguiente_ready(t_list* colas);
int quantum_para_cola(t_list* colas, int cola_prioridad);

t_algoritmo_cola algoritmo_cola_desde_string(char* algoritmo);
const char* planificador_nombre_algoritmo_cola(t_algoritmo_cola algoritmo);

#endif
