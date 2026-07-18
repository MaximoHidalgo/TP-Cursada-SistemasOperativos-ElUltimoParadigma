#include <stdio.h>

// Firmas de las funciones de prueba definidas en los otros archivos
void test_atender_cliente();
void test_pcb_traduccion_estados();
void test_extremo_encolado_prioridades();
void test_encolar_prioridad_invalida();
void test_inicializacion_de_planificador();
void test_extremo_logica_desalojo();
void test_herencia_de_prioridades_mutex();
//void test_logs_minimos_creacion_y_cambio_estado();
//void test_logs_minimos_io_y_cambio_de_estado();
//void test_logs_minimos_mutex_toma_y_libera();
//void test_logs_minimos_fin_quantum();
void test_log_creacion_proceso();

int main() {
    printf("=================================================\n");
    printf("🚀 Iniciando Tests Unitarios del Kernel Scheduler\n");
    printf("=================================================\n\n");
    
    // evaluo la atencion al cliente, implementar otra funcion para modularizar mas void atender_cliente_scheduler(void* arg)
    printf("==================1==================\n");
    test_atender_cliente();
    //cpu
    //creacion proceso
        printf("==================2==================\n");
    test_log_creacion_proceso();
    // Llamadas a test_pcb.c
        printf("==================3==================\n");
    test_pcb_traduccion_estados();
    
    // Llamadas a test_colas.c
        printf("==================4==================\n");
    test_extremo_encolado_prioridades();

        printf("==================5==================\n");
    test_encolar_prioridad_invalida();

    // Llamadas a test_main.c
        printf("==================6==================\n");
    test_inicializacion_de_planificador();
    
    // Llamadas a test_planificador.c
        printf("==================7==================\n");
    test_extremo_logica_desalojo();
        printf("==================8==================\n");
    test_herencia_de_prioridades_mutex();

    // Llamadas a test_logs_minimos.c
    //test_logs_minimos_creacion_y_cambio_estado();
    //test_logs_minimos_io_y_cambio_de_estado();
    //test_logs_minimos_mutex_toma_y_libera();
    //test_logs_minimos_fin_quantum();
    
    printf("\n=================================================\n");
    printf("🎯 Todos los tests se ejecutaron.\n");
    printf("=================================================\n");
    return 0;
}