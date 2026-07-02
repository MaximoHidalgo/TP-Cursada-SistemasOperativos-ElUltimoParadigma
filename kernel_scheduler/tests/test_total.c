#include <stdio.h>

// Firmas de las funciones de prueba definidas en los otros archivos
void test_atender_cliente();
void test_pcb_traduccion_estados();
void test_extremo_encolado_prioridades();
void test_inicializacion_de_planificador();
void test_extremo_logica_desalojo();
void test_herencia_de_prioridades_mutex();

int main() {
    printf("=================================================\n");
    printf("🚀 Iniciando Tests Unitarios del Kernel Scheduler\n");
    printf("=================================================\n\n");
    
    // evaluo la atencion al cliente, implementar otra funcion para modularizar mas void atender_cliente_scheduler(void* arg)
    test_atender_cliente();

    // Llamadas a test_pcb.c
    test_pcb_traduccion_estados();
    
    // Llamadas a test_colas.c
    test_extremo_encolado_prioridades();

    // Llamadas a test_main.c
    test_inicializacion_de_planificador();
    
    // Llamadas a test_planificador.c
    test_extremo_logica_desalojo();
    test_herencia_de_prioridades_mutex();
    
    printf("\n=================================================\n");
    printf("🎯 Todos los tests se ejecutaron.\n");
    printf("=================================================\n");
    return 0;
}