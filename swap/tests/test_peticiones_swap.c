#include "../src/peticiones_swap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cspecs/cspec.h>

#ifdef __INTELLISENSE__
    #undef context
    #define context(name) void cspec_context_##name()
    #undef describe
    #define describe(desc) 
    #undef before
    #define before 
    #undef after
    #define after 
    #undef it
    #define it(desc) 
    #undef end
    #define end 
#endif

#define BLOCK_SIZE 16

static FILE* archivo;

context(test_peticiones_swap) {
    describe("Probando peticiones SWAP (Lectura y Escritura)") {
        
        before {
            archivo = fopen("swap_test.bin", "w+b");
        } end
        
        after {
            fclose(archivo);
            remove("swap_test.bin");
        } end

        it("test_escribir_primer_bloque") {
            char esperado[BLOCK_SIZE] = "Hola";
            escribir_bloque_swap(archivo, 0, BLOCK_SIZE, esperado);

            char obtenido[BLOCK_SIZE];
            fseek(archivo, 0, SEEK_SET);
            fread(obtenido, 1, BLOCK_SIZE, archivo);

            should_int(memcmp(esperado, obtenido, BLOCK_SIZE)) be equal to(0);
        } end

        it("test_escribir_bloque_intermedio") {
            char esperado[BLOCK_SIZE] = "UTNSO";
            escribir_bloque_swap(archivo, 5, BLOCK_SIZE, esperado);

            char obtenido[BLOCK_SIZE];
            fseek(archivo, 5 * BLOCK_SIZE, SEEK_SET);
            fread(obtenido, 1, BLOCK_SIZE, archivo);

            should_int(memcmp(esperado, obtenido, BLOCK_SIZE)) be equal to(0);
        } end

        it("test_sobrescribir_bloque") {
            char primer[BLOCK_SIZE] = "AAAA";
            char segundo[BLOCK_SIZE] = "BBBB";
            
            escribir_bloque_swap(archivo, 2, BLOCK_SIZE, primer);
            escribir_bloque_swap(archivo, 2, BLOCK_SIZE, segundo);

            char obtenido[BLOCK_SIZE];
            fseek(archivo, 2 * BLOCK_SIZE, SEEK_SET);
            fread(obtenido, 1, BLOCK_SIZE, archivo);

            should_int(memcmp(segundo, obtenido, BLOCK_SIZE)) be equal to(0);
        } end

        it("test_leer_bloque") {
            char esperado[BLOCK_SIZE] = "Memoria";
            escribir_bloque_swap(archivo, 3, BLOCK_SIZE, esperado);

            char obtenido[BLOCK_SIZE];
            leer_bloque_swap(archivo, 3, BLOCK_SIZE, obtenido);

            should_int(memcmp(esperado, obtenido, BLOCK_SIZE)) be equal to(0);
        } end

        it("test_leer_varios_bloques") {
            char bloque0[BLOCK_SIZE] = "Uno";
            char bloque1[BLOCK_SIZE] = "Dos";
            char bloque2[BLOCK_SIZE] = "Tres";

            escribir_bloque_swap(archivo, 0, BLOCK_SIZE, bloque0);
            escribir_bloque_swap(archivo, 1, BLOCK_SIZE, bloque1);
            escribir_bloque_swap(archivo, 2, BLOCK_SIZE, bloque2);

            char lectura[BLOCK_SIZE];

            leer_bloque_swap(archivo, 0, BLOCK_SIZE, lectura);
            should_int(memcmp(bloque0, lectura, BLOCK_SIZE)) be equal to(0);

            leer_bloque_swap(archivo, 1, BLOCK_SIZE, lectura);
            should_int(memcmp(bloque1, lectura, BLOCK_SIZE)) be equal to(0);

            leer_bloque_swap(archivo, 2, BLOCK_SIZE, lectura);
            should_int(memcmp(bloque2, lectura, BLOCK_SIZE)) be equal to(0);
        } end
    } end
}
