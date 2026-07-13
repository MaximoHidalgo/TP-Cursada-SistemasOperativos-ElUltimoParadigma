#include <stdio.h>



void test_escribir_primer_bloque();
void test_escribir_bloque_intermedio();
void test_sobrescribir_bloque();
void test_leer_bloque();
void test_leer_varios_bloques();
void test_crear_archivo_swap();

int main() {
    printf("=================================================\n");
    printf("💿 Iniciando Tests Unitarios de SWAP\n");
    printf("=================================================\n\n");

    test_escribir_primer_bloque();
    test_escribir_bloque_intermedio();
    test_sobrescribir_bloque();
    test_leer_bloque();
    test_leer_varios_bloques();
    test_crear_archivo_swap();
    printf("\n=================================================\n");
    printf(">:) Todos los tests de SWAP se ejecutaron.\n");
    printf("=================================================\n");
    return 0;
}