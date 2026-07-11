#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "../src/mmu.h"
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

context(test_cpu_mmu) {
    describe("Probando MMU Traducir") {
        t_contexto_ejecucion* ctx;
        t_cpu_context* cpu_ctx;

        before {
            ctx = malloc(sizeof(t_contexto_ejecucion));
            ctx->tabla_segmentos = list_create();
            
            t_segmento* seg0 = malloc(sizeof(t_segmento));
            seg0->id = 0;
            seg0->base = 100;
            seg0->tamanio = 50;
            
            t_segmento* seg1 = malloc(sizeof(t_segmento));
            seg1->id = 1;
            seg1->base = 200;
            seg1->tamanio = 30;

            list_add(ctx->tabla_segmentos, seg0);
            list_add(ctx->tabla_segmentos, seg1);

            cpu_ctx = malloc(sizeof(t_cpu_context));
            cpu_ctx->tam_max_segmento = 64; // Ejemplo
        } end

        after {
            list_destroy_and_destroy_elements(ctx->tabla_segmentos, free);
            free(ctx);
            free(cpu_ctx);
        } end

        it("Traducir direccion exitosamente (dentro del segmento 0)") {
            // Segmento 0 va de dir_logica 0 a 63 (por tam_max_segmento=64)
            // Dir_logica = 10 -> Segmento 0, desplazamiento 10.
            // dir_fisica = base(100) + 10 = 110.
            // Tamaño a leer = 4. 10 + 4 <= 50 (tamaño del segmento). Correcto.
            uint32_t dir_fisica = 0;
            int res = mmu_traducir(cpu_ctx, ctx, 10, 4, cpu_ctx->tam_max_segmento, &dir_fisica);
            
            should_int(res) be equal to(0);
            should_int(dir_fisica) be equal to(110);
        } end

        it("Traducir direccion exitosamente (dentro del segmento 1)") {
            // Segmento 1 va de dir_logica 64 a 127
            // Dir_logica = 74 -> Segmento 1 (floor(74/64) = 1), desplazamiento = 10.
            // dir_fisica = base(200) + 10 = 210.
            // Tamaño = 4. 10 + 4 <= 30 (tamaño seg). Correcto.
            uint32_t dir_fisica = 0;
            int res = mmu_traducir(cpu_ctx, ctx, 74, 4, cpu_ctx->tam_max_segmento, &dir_fisica);
            
            should_int(res) be equal to(0);
            should_int(dir_fisica) be equal to(210);
        } end

        it("Fallo por Segmentation Fault (excede el tamanio del segmento)") {
            // Dir_logica = 10. Segmento 0, desplazamiento 10.
            // Tamaño a leer = 45. 10 + 45 = 55 > 50 (tamaño del segmento 0). -> SEG FAULT
            uint32_t dir_fisica = 0;
            int res = mmu_traducir(cpu_ctx, ctx, 10, 45, cpu_ctx->tam_max_segmento, &dir_fisica);
            
            should_int(res) be equal to(-1);
        } end

        it("Fallo por Segmentation Fault (segmento inexistente)") {
            // Dir_logica = 150 -> Segmento 2 (floor(150/64) = 2). 
            // Segmento 2 no existe en la tabla -> SEG FAULT
            uint32_t dir_fisica = 0;
            int res = mmu_traducir(cpu_ctx, ctx, 150, 4, cpu_ctx->tam_max_segmento, &dir_fisica);
            
            should_int(res) be equal to(-1);
        } end
    } end
}
