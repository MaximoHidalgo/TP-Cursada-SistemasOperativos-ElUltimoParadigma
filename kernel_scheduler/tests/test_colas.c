#include <stdio.h>
#include <assert.h> //assert
#include <stdlib.h>
#include "../src/colas.h"
#include "../src/pcb.h"

static t_list* crear_colas_ready_test(int cantidad)
{
    t_list* colas = list_create();

    for (int i = 0; i < cantidad; i++) {
        t_cola_planificacion* cola = calloc(1, sizeof(*cola));

        cola->nivel = i;
        cola->procesos = queue_create();

        list_add(colas, cola);
    }

    return colas;
}

static void destruir_colas_ready_test(t_list* colas)
{
    list_destroy_and_destroy_elements(colas, destruir_cola_planificacion);
}

static t_proceso_planificable* crear_proceso_test(uint32_t pid, uint32_t prioridad)
{
    t_proceso_planificable* proceso = calloc(1, sizeof(*proceso));

    proceso->pid = pid;
    proceso->cola_prioridad = prioridad;

    return proceso;
}

static void verificar(const char* nombre, bool condicion)
{
    printf("[TEST] %s... ", nombre);
    assert(condicion);
    printf("OK\n");
}

void test_extremo_encolado_prioridades()
{
    t_list* colas = crear_colas_ready_test(3);

    t_proceso_planificable* p_baja  = crear_proceso_test(100, 2);
    t_proceso_planificable* p_media = crear_proceso_test(101, 1);
    t_proceso_planificable* p_alta  = crear_proceso_test(102, 0);
    

    verificar("Encolar prioridad baja", encolar_en_cola_ready(colas, p_baja));
    verificar("Encolar prioridad media",encolar_en_cola_ready(colas, p_media));
    verificar("Encolar prioridad alta",encolar_en_cola_ready(colas, p_alta));
    verificar("Sale primero prioridad alta",desencolar_siguiente_ready(colas)->pid == 102);
    verificar("Sale segundo prioridad media",desencolar_siguiente_ready(colas)->pid == 101);
    verificar("Sale tercero prioridad baja",desencolar_siguiente_ready(colas)->pid == 100);

    free(p_alta);
    free(p_media);
    free(p_baja);

    destruir_colas_ready_test(colas);

    printf("✅ test_extremo_encolado_prioridades: PASO\n");

}

void test_encolar_prioridad_invalida()
{
    t_list* colas = crear_colas_ready_test(2);

    t_proceso_planificable* proceso =
        crear_proceso_test(10, 99);


    verificar("No permite prioridad inexistente",encolar_en_cola_ready(colas, proceso) == false);
    verificar("La cola 0 permanece vacia",queue_size(((t_cola_planificacion*)list_get(colas,0))->procesos) == 0);
    verificar("La cola 1 permanece vacia",queue_size(((t_cola_planificacion*)list_get(colas,1))->procesos) == 0);

    free(proceso);
    destruir_colas_ready_test(colas);

    printf("✅ test_encolar_prioridad_invalida: PASO\n");

}