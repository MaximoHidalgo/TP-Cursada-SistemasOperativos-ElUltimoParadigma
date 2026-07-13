#include <stdio.h>



void test_escribir_primer_bloque();
void test_escribir_bloque_intermedio();
void test_sobrescribir_bloque();
void test_leer_bloque();
void test_leer_varios_bloques();
void test_crear_archivo_swap();

int main() {

    test_escribir_primer_bloque();
    test_escribir_bloque_intermedio();
    test_sobrescribir_bloque();
    test_leer_bloque();
    test_leer_varios_bloques();
    test_crear_archivo_swap();
    
    printf("Todos los test de SWAP fueron pasados correctamente\n");
    return 0;
}