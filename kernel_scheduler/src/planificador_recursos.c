#include "planificador_recursos.h"

void destruir_mutex(void* element) {
    t_mutex* m = (t_mutex*)element;
    if (m) {
        free(m->nombre);
        queue_destroy(m->cola_bloqueados);
        pthread_mutex_destroy(&m->mutex_semaforo);
        free(m);
    }
}

void planificador_mutex_create(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso) {
    if (estratega == NULL || pcb == NULL || recurso == NULL) return;

    pthread_mutex_lock(&estratega->mutex_colas);
    if (!dictionary_has_key(estratega->mutex_creados, recurso)) {
        t_mutex* nuevo_mutex = malloc(sizeof(t_mutex));
        nuevo_mutex->nombre = string_duplicate(recurso);
        nuevo_mutex->tomado = 0;
        nuevo_mutex->pid_duenio = -1;
        nuevo_mutex->cola_bloqueados = queue_create();
        pthread_mutex_init(&nuevo_mutex->mutex_semaforo, NULL);

        dictionary_put(estratega->mutex_creados, recurso, nuevo_mutex);
        log_info(estratega->logger, "## Mutex Creado: %s", recurso);
    }
    pthread_mutex_unlock(&estratega->mutex_colas);
}

int planificador_mutex_lock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso) {
    if (estratega == NULL || pcb == NULL || recurso == NULL) return 0;

    pthread_mutex_lock(&estratega->mutex_colas);
    t_mutex* mutex = dictionary_get(estratega->mutex_creados, recurso);
    if (mutex == NULL) {
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 0; // Si no existe, no hacemos nada y sigue en EXEC
    }

    pthread_mutex_lock(&mutex->mutex_semaforo);
    if (mutex->tomado == 0) {
        mutex->tomado = 1;
        mutex->pid_duenio = pcb->pid;
        mutex->pcb_duenio = pcb; 
        
        log_info(estratega->logger, "## (%d) Toma el Mutex %s", pcb->pid, recurso);
        
        pthread_mutex_unlock(&mutex->mutex_semaforo);
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 0; 
    } else {
        pcb->estado = ESTADO_PROCESO_BLOCK;
        queue_push(mutex->cola_bloqueados, pcb);
        log_info(estratega->logger, "## Proceso %d bloqueado esperando Mutex %s", pcb->pid, recurso);
        
        // HERENCIA DE PRIORIDADES
        if (mutex->pcb_duenio != NULL && pcb->cola_prioridad < mutex->pcb_duenio->cola_prioridad) {
            log_info(estratega->logger, "## %d Cambio de prioridad: %d - %d", 
                     mutex->pcb_duenio->pid, mutex->pcb_duenio->cola_prioridad, pcb->cola_prioridad);

            int prioridad_vieja = mutex->pcb_duenio->cola_prioridad;
            mutex->pcb_duenio->cola_prioridad = pcb->cola_prioridad; 

            if (mutex->pcb_duenio->estado == ESTADO_PROCESO_READY) {
                t_cola_planificacion* cola_vieja = obtener_cola_planificacion(estratega->colas, prioridad_vieja);
                list_remove_element(cola_vieja->procesos->elements, mutex->pcb_duenio);
                encolar_en_cola_ready(estratega->colas, mutex->pcb_duenio);
            }
        }

        if (estratega->proceso_exec != NULL && estratega->proceso_exec->pid == pcb->pid) {
            estratega->proceso_exec = NULL;
        }

        pthread_mutex_unlock(&mutex->mutex_semaforo);
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 1; 
    }
}

int planificador_mutex_unlock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso) {
    if (estratega == NULL || pcb == NULL || recurso == NULL) return 0;

    int debe_desalojar = 0;

    pthread_mutex_lock(&estratega->mutex_colas);
    t_mutex* mutex = dictionary_get(estratega->mutex_creados, recurso);
    if (mutex == NULL) {
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 0;
    }

    pthread_mutex_lock(&mutex->mutex_semaforo);
    if (mutex->tomado == 1 && mutex->pid_duenio == pcb->pid) {
        log_info(estratega->logger, "## (%d) Libera el Mutex %s", pcb->pid, recurso);

        if (pcb->cola_prioridad != pcb->prioridad_original) {
            log_info(estratega->logger, "## %d Cambio de prioridad: %d - %d", 
                     pcb->pid, pcb->cola_prioridad, pcb->prioridad_original);
            pcb->cola_prioridad = pcb->prioridad_original;
        }
        
        if (!queue_is_empty(mutex->cola_bloqueados)) {
            t_proceso_planificable* liberado = queue_pop(mutex->cola_bloqueados);
            mutex->pid_duenio = liberado->pid;
            mutex->pcb_duenio = liberado;

            log_info(estratega->logger, "## Proceso %d desencolado y asume Mutex %s", liberado->pid, recurso);
            
            t_estado_proceso estado_anterior = liberado->estado;
            liberado->estado = ESTADO_PROCESO_READY;
            
            log_info(estratega->logger, "## (%d) Pasa del estado %s al estado %s", 
                     liberado->pid, planificador_nombre_estado(estado_anterior), planificador_nombre_estado(liberado->estado));

            encolar_en_cola_ready(estratega->colas, liberado);
            
            if (estratega->proceso_exec != NULL) {
                if (planificador_debe_desalojar(estratega, estratega->proceso_exec->cola_prioridad, liberado->cola_prioridad)) {
                    log_info(estratega->logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
                             estratega->proceso_exec->pid, estratega->proceso_exec->cola_prioridad,
                             liberado->pid, liberado->cola_prioridad);
                    debe_desalojar = 1;
                }
            }
        } else {
            mutex->tomado = 0;
            mutex->pid_duenio = -1;
            mutex->pcb_duenio = NULL;
        }
    }
    pthread_mutex_unlock(&mutex->mutex_semaforo);
    pthread_mutex_unlock(&estratega->mutex_colas);
    return debe_desalojar;
}