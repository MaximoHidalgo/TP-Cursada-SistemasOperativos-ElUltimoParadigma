#include <stdio.h>

// Firmas de las funciones de prueba definidas en los otros archivos
void test_pcb_traduccion_estados();
void test_extremo_encolado_prioridades();
void test_extremo_logica_desalojo();
void test_herencia_de_prioridades_mutex();

int main() {
    printf("=================================================\n");
    printf("🚀 Iniciando Tests Unitarios del Kernel Scheduler\n");
    printf("=================================================\n\n");
    
    // Llamadas a test_pcb.c
    test_pcb_traduccion_estados();
    
    // Llamadas a test_colas.c
    test_extremo_encolado_prioridades();
    
    // Llamadas a test_planificador.c
    test_extremo_logica_desalojo();
    test_herencia_de_prioridades_mutex();
    
    printf("\n=================================================\n");
    printf("🎯 Todos los tests se ejecutaron.\n");
    printf("=================================================\n");
    return 0;
}