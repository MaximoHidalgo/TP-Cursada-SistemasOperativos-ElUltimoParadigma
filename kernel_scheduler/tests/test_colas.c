#include <stdio.h>
#include <assert.h> //assert
#include <stdlib.h>
#include "../src/colas.h"
#include "../src/pcb.h"

void test_extremo_encolado_prioridades() {
    // 1. Arrange: Creamos listas manualmente (Mock)
    t_list* colas = list_create();
    
    t_cola_planificacion* cola_alta = calloc(1, sizeof(t_cola_planificacion));
    cola_alta->nivel = 0; 
    cola_alta->procesos = queue_create();
    
    t_cola_planificacion* cola_baja = calloc(1, sizeof(t_cola_planificacion));
    cola_baja->nivel = 1; 
    cola_baja->procesos = queue_create();
    
    list_add(colas, cola_alta);
    list_add(colas, cola_baja);
    
    t_proceso_planificable p_alta = {.pid = 1, .cola_prioridad = 0};
    t_proceso_planificable p_baja = {.pid = 2, .cola_prioridad = 1};
    t_proceso_planificable p_invalida = {.pid = 3, .cola_prioridad = 99, .estado = ESTADO_PROCESO_NEW};
    
    // 2. Act: Encolamos
    assert(encolar_en_cola_ready(colas, &p_alta) == true);
    assert(encolar_en_cola_ready(colas, &p_baja) == true); 
    assert(encolar_en_cola_ready(colas, &p_invalida) == false); 
    
    // 3. Assert: Validamos que respetan su nivel
    assert(queue_size(cola_alta->procesos) == 1);
    assert(queue_size(cola_baja->procesos) == 1);
    assert(p_invalida.estado == ESTADO_PROCESO_NEW);
    
    // Limpieza
    queue_destroy(cola_alta->procesos); free(cola_alta);
    queue_destroy(cola_baja->procesos); free(cola_baja);
    list_destroy(colas);

    printf("✅ test_extremo_encolado_prioridades: PASO\n");
}