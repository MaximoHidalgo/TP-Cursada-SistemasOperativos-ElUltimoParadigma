#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <pthread.h>
#include "colas.h"

typedef enum {
    ALGORITMO_PLANIFICACION_CMN, //El de colas multinivel
    ALGORITMO_PLANIFICACION_FIFO, //El de first in first out
    ALGORITMO_PLANIFICACION_RR, //El de Round Robin
    ALGORITMO_PLANIFICACION_INVALIDO
} t_algoritmo_planificacion;

typedef struct {
    t_algoritmo_planificacion algoritmo;
    t_queue* cola_new;
    t_list* colas;
    t_proceso_planificable* proceso_exec;
    bool desalojo_entre_colas;
    int quantum_rr;
    t_dictionary* colas_io;      
    t_dictionary* mutex_creados; 
    pthread_mutex_t mutex_colas;
    t_log* logger;
} t_estratega_planificacion;

t_estratega_planificacion* crear_estratega_planificacion(t_config* config, t_log* logger);
void destruir_estratega_planificacion(t_estratega_planificacion* estratega);

bool planificador_encolar(t_estratega_planificacion* estratega, t_proceso_planificable* proceso);
t_proceso_planificable* planificador_obtener_siguiente(t_estratega_planificacion* estratega);
bool planificador_reencolar_por_quantum(t_estratega_planificacion* estratega, t_proceso_planificable* proceso);
bool planificador_debe_desalojar(t_estratega_planificacion* estratega, int cola_en_ejecucion, int cola_nuevo_proceso);
int planificador_quantum_para(t_estratega_planificacion* estratega, int cola_prioridad);
bool planificador_encolar_new(t_estratega_planificacion* estratega, t_proceso_planificable* proceso);
t_proceso_planificable* planificador_admitir_a_ready(t_estratega_planificacion* estratega);
t_proceso_planificable* planificador_despachar_siguiente(t_estratega_planificacion* estratega);
void planificador_liberar_exec(t_estratega_planificacion* estratega, int pid);
bool planificador_iniciar_quantum_si_corresponde(t_estratega_planificacion* estratega, t_proceso_planificable* proceso, int socket_cpu);

const char* planificador_nombre_algoritmo(t_algoritmo_planificacion algoritmo);

#endif
