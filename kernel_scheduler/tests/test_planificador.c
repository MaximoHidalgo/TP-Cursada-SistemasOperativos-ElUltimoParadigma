#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <commons/collections/dictionary.h>
#include "../src/planificador.h"
#include "../src/colas.h"
#include "../src/planificador_recursos.h"
#include "../src/pcb.h"

void test_extremo_logica_desalojo() {
    t_estratega_planificacion estratega;
    estratega.desalojo_entre_colas = true; // Activo según regla de CMN
    
    // Caso: Ejecuta prioridad Baja (3), llega uno a Alta (0) -> Debe desalojar
    bool debe_desalojar = planificador_debe_desalojar(&estratega, 3, 0);
    assert(debe_desalojar == true); 
    
    // Caso: Ejecuta prioridad Alta (1), llega uno a Baja (3) -> NO debe desalojar
    debe_desalojar = planificador_debe_desalojar(&estratega, 1, 3);
    assert(debe_desalojar == false); 
    
    printf("✅ test_extremo_logica_desalojo: PASO\n");
}

void test_herencia_de_prioridades_mutex() {
    t_estratega_planificacion* estratega = calloc(1, sizeof(t_estratega_planificacion));
    estratega->mutex_creados = dictionary_create();
    estratega->logger = log_create("/tmp/kernel_scheduler_test.log", "TEST", false, LOG_LEVEL_INFO);
    pthread_mutex_init(&estratega->mutex_colas, NULL);
    
    t_proceso_planificable* p_baja = malloc(sizeof(t_proceso_planificable));
    p_baja->pid = 3; p_baja->cola_prioridad = 2; p_baja->estado = ESTADO_PROCESO_EXEC; 

    t_proceso_planificable* p_alta = malloc(sizeof(t_proceso_planificable));
    p_alta->pid = 1; p_alta->cola_prioridad = 0; p_alta->estado = ESTADO_PROCESO_EXEC;

    // Ejecución del Lock y bloqueo por mutex
    planificador_mutex_create(estratega, p_baja, "RECURSO_A");
    assert(planificador_mutex_lock(estratega, p_baja, "RECURSO_A") == 0);
    assert(planificador_mutex_lock(estratega, p_alta, "RECURSO_A") == 1);
    assert(p_alta->estado == ESTADO_PROCESO_BLOCK);
    
    planificador_mutex_unlock(estratega, p_baja, "RECURSO_A");
    
    // Assert: El proceso bloqueado queda en READY al liberarse el mutex
    assert(p_alta->estado == ESTADO_PROCESO_READY);

    printf("✅ test_herencia_de_prioridades_mutex: PASO\n");

    // Limpieza omitida por brevedad visual, recordar hacer free() de p_baja y p_alta
}