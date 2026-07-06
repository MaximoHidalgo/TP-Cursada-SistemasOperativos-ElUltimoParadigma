#include "../src/peticiones_swap.h"
#include "assert.h"


#define BLOCK_SIZE 16


//FUNCIONES AUXILIARES DEL TEST


static FILE* crear_archivo_test()
{
    FILE* archivo = fopen("swap_test.bin", "w+b");
    assert(archivo != NULL);

    return archivo;
}

static void destruir_archivo_test(FILE* archivo)
{
    fclose(archivo);
    remove("swap_test.bin");
}

/*=====================================================
//TEST 1 (ESCRITURA DE PRIMER BLOQUE)
=====================================================*/

void test_escribir_primer_bloque()
{
    FILE* archivo = crear_archivo_test();

    char esperado[BLOCK_SIZE] = "Hola";

    escribir_bloque_swap(archivo,0,BLOCK_SIZE,esperado);

    char obtenido[BLOCK_SIZE];

    fseek(archivo, 0, SEEK_SET);
    fread(obtenido, 1, BLOCK_SIZE, archivo);

    assert(memcmp(esperado, obtenido, BLOCK_SIZE) == 0);

    destruir_archivo_test(archivo);
    printf("test_escribir_primer_bloque PASO\n");
}

/*=====================================================
         TEST 2 (ESCIRTURA BLOQUE DE INTERMEDIO)
=====================================================*/

void test_escribir_bloque_intermedio()
{
    FILE* archivo = crear_archivo_test();

    char esperado[BLOCK_SIZE] = "UTNSO";

    escribir_bloque_swap(archivo,5,BLOCK_SIZE,esperado);

    char obtenido[BLOCK_SIZE];

    fseek(archivo, 5 * BLOCK_SIZE, SEEK_SET);
    fread(obtenido, 1, BLOCK_SIZE, archivo);

    assert(memcmp(esperado, obtenido, BLOCK_SIZE) == 0);

    destruir_archivo_test(archivo);
    printf("test_escribir_bloque_intermedio PASO\n");
}

/*=====================================================
                TEST 3 (SOBRESCRIBIR BLOQUE)
=====================================================*/

void test_sobrescribir_bloque()
{
    FILE* archivo = crear_archivo_test();

    char primer[BLOCK_SIZE] = "AAAA";
    char segundo[BLOCK_SIZE] = "BBBB";

    escribir_bloque_swap(archivo, 2,BLOCK_SIZE,primer);

    escribir_bloque_swap(archivo,2,BLOCK_SIZE,segundo);

    char obtenido[BLOCK_SIZE];

    fseek(archivo, 2 * BLOCK_SIZE, SEEK_SET);
    fread(obtenido, 1, BLOCK_SIZE, archivo);

    assert(memcmp(segundo, obtenido, BLOCK_SIZE) == 0);

    destruir_archivo_test(archivo);
    printf("test_sobrescribir_bloque PASO\n");
}

/*=====================================================
                TEST 4 (LECTURA DE UN BLOQUE)
=====================================================*/

void test_leer_bloque()
{
    FILE* archivo = crear_archivo_test();

    char esperado[BLOCK_SIZE] = "Memoria";

    escribir_bloque_swap(archivo,3,BLOCK_SIZE,esperado);

    char obtenido[BLOCK_SIZE];

    leer_bloque_swap(archivo,3,BLOCK_SIZE,obtenido);

    assert(memcmp(esperado, obtenido, BLOCK_SIZE) == 0);

    destruir_archivo_test(archivo);
    printf("test_leer_bloque PASO\n");
}

/*=====================================================
                TEST 5 (LECTURA DE VARIOS BLOQUES)
=====================================================*/

void test_leer_varios_bloques()
{
    FILE* archivo = crear_archivo_test();

    char bloque0[BLOCK_SIZE] = "Uno";
    char bloque1[BLOCK_SIZE] = "Dos";
    char bloque2[BLOCK_SIZE] = "Tres";

    escribir_bloque_swap(archivo, 0, BLOCK_SIZE, bloque0);
    escribir_bloque_swap(archivo, 1, BLOCK_SIZE, bloque1);
    escribir_bloque_swap(archivo, 2, BLOCK_SIZE, bloque2);

    char lectura[BLOCK_SIZE];

    leer_bloque_swap(archivo, 0, BLOCK_SIZE, lectura);
    assert(memcmp(bloque0, lectura, BLOCK_SIZE) == 0);

    leer_bloque_swap(archivo, 1, BLOCK_SIZE, lectura);
    assert(memcmp(bloque1, lectura, BLOCK_SIZE) == 0);

    leer_bloque_swap(archivo, 2, BLOCK_SIZE, lectura);
    assert(memcmp(bloque2, lectura, BLOCK_SIZE) == 0);

    destruir_archivo_test(archivo);
    printf("test_leer_varios_bloques PASO\n");
}
