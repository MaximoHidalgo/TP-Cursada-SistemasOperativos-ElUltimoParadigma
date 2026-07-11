#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "../src/registros.h"
#include <commons/collections/list.h>
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

context(test_cpu_registros) {
    describe("Probando funciones de registros de CPU") {
        
        t_contexto_ejecucion* ctx;

        before {
            ctx = malloc(sizeof(t_contexto_ejecucion));
            ctx->pid = 1;
            memset(&(ctx->registros), 0, sizeof(t_registros));
            ctx->tabla_segmentos = list_create();
        } end

        after {
            list_destroy(ctx->tabla_segmentos);
            free(ctx);
        } end

        it("Asignar y leer un registro de 1 byte (uint8_t)") {
            asignar_valor_a_registro(ctx, "AX", 255); // 0xFF
            uint32_t val = leer_registro(ctx, "AX");
            should_int(val) be equal to(255);
            
            // Si intentamos asignar un valor más grande, debe truncar a 1 byte (debido a cast uint8_t)
            asignar_valor_a_registro(ctx, "BX", 257); // 257 en binario es 0000 0001 0000 0001 -> 1 en 8 bits
            val = leer_registro(ctx, "BX");
            should_int(val) be equal to(1);
        } end

        it("Asignar y leer un registro de 4 bytes (uint32_t)") {
            asignar_valor_a_registro(ctx, "EAX", 4000000000);
            uint32_t val = leer_registro(ctx, "EAX");
            should_int(val) be equal to(4000000000);
        } end

        it("Obtener tamanio de registro") {
            should_int(obtener_tamanio_registro("AX")) be equal to(1);
            should_int(obtener_tamanio_registro("BX")) be equal to(1);
            should_int(obtener_tamanio_registro("EAX")) be equal to(4);
            should_int(obtener_tamanio_registro("SI")) be equal to(4);
            should_int(obtener_tamanio_registro("DI")) be equal to(4);
        } end
    } end
}
