#include "../src/peticiones_swap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assert.h"

#define BLOCK_SIZE 16

static FILE* crear_archivo_test_old()
{
    FILE* archivo = fopen("swap_test_old.bin", "w+b");
    assert(archivo != NULL);

    return archivo;
}

static void destruir_archivo_test_old(FILE* archivo)
{
    fclose(archivo);
    remove("swap_test_old.bin");
}

void test_escribir_primer_bloque_old()
{
    FILE* archivo = crear_archivo_test_old();
    char esperado[BLOCK_SIZE] = "Hola";
    escribir_bloque_swap(archivo,0,BLOCK_SIZE,esperado);
    char obtenido[BLOCK_SIZE];
    fseek(archivo, 0, SEEK_SET);
    fread(obtenido, 1, BLOCK_SIZE, archivo);
    assert(memcmp(esperado, obtenido, BLOCK_SIZE) == 0);
    destruir_archivo_test_old(archivo);
    printf("test_escribir_primer_bloque PASO\n");
}

void test_escribir_bloque_intermedio_old()
{
    FILE* archivo = crear_archivo_test_old();
    char esperado[BLOCK_SIZE] = "UTNSO";
    escribir_bloque_swap(archivo,5,BLOCK_SIZE,esperado);
    char obtenido[BLOCK_SIZE];
    fseek(archivo, 5 * BLOCK_SIZE, SEEK_SET);
    fread(obtenido, 1, BLOCK_SIZE, archivo);
    assert(memcmp(esperado, obtenido, BLOCK_SIZE) == 0);
    destruir_archivo_test_old(archivo);
    printf("test_escribir_bloque_intermedio PASO\n");
}

void test_sobrescribir_bloque_old()
{
    FILE* archivo = crear_archivo_test_old();
    char primer[BLOCK_SIZE] = "AAAA";
    char segundo[BLOCK_SIZE] = "BBBB";
    escribir_bloque_swap(archivo, 2,BLOCK_SIZE,primer);
    escribir_bloque_swap(archivo,2,BLOCK_SIZE,segundo);
    char obtenido[BLOCK_SIZE];
    fseek(archivo, 2 * BLOCK_SIZE, SEEK_SET);
    fread(obtenido, 1, BLOCK_SIZE, archivo);
    assert(memcmp(segundo, obtenido, BLOCK_SIZE) == 0);
    destruir_archivo_test_old(archivo);
    printf("test_sobrescribir_bloque PASO\n");
}

void test_leer_bloque_old()
{
    FILE* archivo = crear_archivo_test_old();
    char esperado[BLOCK_SIZE] = "Memoria";
    escribir_bloque_swap(archivo,3,BLOCK_SIZE,esperado);
    char obtenido[BLOCK_SIZE];
    leer_bloque_swap(archivo,3,BLOCK_SIZE,obtenido);
    assert(memcmp(esperado, obtenido, BLOCK_SIZE) == 0);
    destruir_archivo_test_old(archivo);
    printf("test_leer_bloque PASO\n");
}

void test_leer_varios_bloques_old()
{
    FILE* archivo = crear_archivo_test_old();
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
    destruir_archivo_test_old(archivo);
    printf("test_leer_varios_bloques PASO\n");
}
