#include <string.h>
#include <stdint.h>
#include "../src/estructuras.h"
#include <commons/collections/list.h>

// IMPORTANTE: cspec.h DEBE ser siempre el último include
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

context(test_cpu_estructuras) {
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

    describe("Probando decode") {
        it("Decodificar instruccion sin parametros") {
            t_instruccion* instr = decode("EXIT");
            should_string(instr->identificador) be equal to("EXIT");
            should_ptr(instr->parametro1) be null;
            should_ptr(instr->parametro2) be null;
            should_ptr(instr->parametro3) be null;
            
            free(instr->identificador);
            free(instr);
        } end

        it("Decodificar instruccion con un parametro") {
            t_instruccion* instr = decode("SLEEP 100");
            should_string(instr->identificador) be equal to("SLEEP");
            should_string(instr->parametro1) be equal to("100");
            should_ptr(instr->parametro2) be null;
            should_ptr(instr->parametro3) be null;

            free(instr->identificador);
            free(instr->parametro1);
            free(instr);
        } end

        it("Decodificar instruccion con dos parametros") {
            t_instruccion* instr = decode("SET AX 5");
            should_string(instr->identificador) be equal to("SET");
            should_string(instr->parametro1) be equal to("AX");
            should_string(instr->parametro2) be equal to("5");
            should_ptr(instr->parametro3) be null;

            free(instr->identificador);
            free(instr->parametro1);
            free(instr->parametro2);
            free(instr);
        } end
    } end

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
