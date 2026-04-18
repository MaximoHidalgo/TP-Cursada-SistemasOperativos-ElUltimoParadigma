#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "scheduler.h"

// Definición de colas
t_list* cola_new;
t_list* cola_ready;
t_list* cola_exec;
t_list* cola_block;

void iniciar_colas() {
    cola_new = list_create();
    cola_ready = list_create();
    cola_exec = list_create();
    cola_block = list_create();
}
/*
void pasar_a_ready(proceso_t* p) {
    p->estado = READY;
    list_add(cola_ready, p);
    printf("PID %d pasa a READY\n", p->pid);
}

proceso_t* obtener_siguiente() {
    if (list_is_empty(cola_ready))
        return NULL;

    return list_remove(cola_ready, 0); // FIFO
}

void ejecutar_proceso(proceso_t* p) {
    p->estado = EXEC;
    printf("PID %d en EXEC\n", p->pid);

    sleep(2); // Simulación

    p->estado = EXIT;
    printf("PID %d terminó\n", p->pid);
}

void planificar() {
    while (1) {
        proceso_t* p = obtener_siguiente();

        if (p != NULL) {
            ejecutar_proceso(p);
        } else {
            printf("No hay procesos en READY...\n");
            sleep(1);
        }
    }
}*/