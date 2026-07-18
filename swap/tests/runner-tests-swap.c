#include <stdio.h>
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

void test_escribir_primer_bloque_old();
void test_escribir_bloque_intermedio_old();
void test_sobrescribir_bloque_old();
void test_leer_bloque_old();
void test_leer_varios_bloques_old();
void test_crear_archivo_swap_old();

context(tests_viejos_swap) {
    describe("Ejecutando suite de tests viejos de SWAP") {
        it("Deberian pasar todos los asserts de la suite antigua") {
            printf("\n=================================================\n");
            printf("💿 Iniciando Tests Unitarios Antiguos de SWAP\n");
            printf("=================================================\n\n");

            test_escribir_primer_bloque_old();
            test_escribir_bloque_intermedio_old();
            test_sobrescribir_bloque_old();
            test_leer_bloque_old();
            test_leer_varios_bloques_old();
            test_crear_archivo_swap_old();
            
            printf("\n=================================================\n");
            printf(">:) Todos los tests antiguos de SWAP se ejecutaron.\n");
            printf("=================================================\n");
        } end
    } end
}
