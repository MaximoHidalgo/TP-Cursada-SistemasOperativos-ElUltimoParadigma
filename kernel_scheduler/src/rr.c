#include "rr.h"

static void dormir_milisegundos(int milisegundos) {
    struct timespec tiempo;
    tiempo.tv_sec = milisegundos / 1000;
    tiempo.tv_nsec = (milisegundos % 1000) * 1000000L;
    nanosleep(&tiempo, NULL);
}

static void* controlar_quantum(void* args_void) {
    t_args_quantum* args = args_void;

    dormir_milisegundos(args->quantum_ms);

    pthread_mutex_lock(&args->estratega->mutex_colas);
    bool sigue_en_exec = args->estratega->proceso_exec != NULL
        && args->estratega->proceso_exec->pid == args->pid;
    pthread_mutex_unlock(&args->estratega->mutex_colas);

    if (sigue_en_exec) {
        t_paquete* paquete = crear_paquete();
        paquete->codigo_operacion = INTERRUPCION_QUANTUM;
        
        agregar_a_paquete(paquete, &(args->pid), sizeof(uint32_t));

        enviar_paquete(paquete, args->socket_cpu);

        eliminar_paquete(paquete);

        log_info(args->estratega->logger, "## (%d) - Desalojado por fin de quantum", args->pid);
    }
    free(args);
    return NULL;
}

bool rr_iniciar_quantum(t_estratega_planificacion* estratega, t_proceso_planificable* proceso, int socket_cpu) {
    int quantum = planificador_quantum_para(estratega, proceso != NULL ? proceso->cola_prioridad : -1);

    if (estratega == NULL || proceso == NULL || socket_cpu < 0 || quantum <= 0) {
        return false;
    }

    t_args_quantum* args = malloc(sizeof(t_args_quantum));
    args->estratega = estratega;
    args->pid = proceso->pid;
    args->socket_cpu = socket_cpu;
    args->quantum_ms = quantum;

    pthread_t hilo_quantum;
    if (pthread_create(&hilo_quantum, NULL, controlar_quantum, args) != 0) {
        free(args);
        return false;
    }

    pthread_detach(hilo_quantum);
    return true;
}