#include <stdio.h>

// Firmas de las funciones de prueba definidas en los otros archivos

void test_procesador_mensaje();
void test_principal_km();
void test_auxiliares();
void test_auxiliares_memoria();

int main() {
    printf("=================================================\n");
    printf(":O Iniciando Tests Unitarios del Kernel Memory\n");
    printf("=================================================\n\n");
    
    // Llamadas a test_procesador_mensaje.c
    test_procesador_mensaje();
    
    // Llamadas a test_main.c
    test_principal_km();
    
    // Llamadas a test_auxiliares.c
    test_auxiliares();

    // Llamadas a test_auxiliares_memoria.c
    test_auxiliares_memoria();
    
    printf("\n=================================================\n");
    printf(">:) Todos los tests se ejecutaron.\n");
    printf("=================================================\n");
    return 0;
}