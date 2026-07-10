#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include "../src/instrucciones.h"
#include "../src/syscalls.h"
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

context(test_cpu_instrucciones) {
    describe("Probando handlers de instrucciones de CPU") {
        
        t_contexto_ejecucion* ctx;
        t_cpu_context* cpu_ctx;
        t_instruccion* instr;
        int sv_mem[2];
        int sv_sched[2];

        before {
            ctx = malloc(sizeof(t_contexto_ejecucion));
            ctx->pid = 1;
            memset(&(ctx->registros), 0, sizeof(t_registros));
            ctx->tabla_segmentos = list_create();
            
            cpu_ctx = malloc(sizeof(t_cpu_context));
            cpu_ctx->conexiones = malloc(sizeof(t_conexiones_cpu));
            cpu_ctx->memory_sticks = list_create();
            cpu_ctx->logger = log_create("test.log", "TEST", false, LOG_LEVEL_DEBUG);
            cpu_ctx->tam_max_segmento = 64;

            // Creamos socketpairs para simular conexiones sin red real
            socketpair(AF_UNIX, SOCK_STREAM, 0, sv_mem);
            socketpair(AF_UNIX, SOCK_STREAM, 0, sv_sched);
            cpu_ctx->conexiones->memoria = sv_mem[0];
            cpu_ctx->conexiones->scheduler = sv_sched[0];

            instr = malloc(sizeof(t_instruccion));
            memset(instr, 0, sizeof(t_instruccion));
        } end

        after {
            list_destroy_and_destroy_elements(ctx->tabla_segmentos, free);
            free(ctx);
            
            list_destroy_and_destroy_elements(cpu_ctx->memory_sticks, free);
            log_destroy(cpu_ctx->logger);
            free(cpu_ctx->conexiones);
            free(cpu_ctx);

            if (instr->identificador) free(instr->identificador);
            if (instr->parametro1) free(instr->parametro1);
            if (instr->parametro2) free(instr->parametro2);
            if (instr->parametro3) free(instr->parametro3);
            free(instr);

            close(sv_mem[0]);
            close(sv_mem[1]);
            close(sv_sched[0]);
            close(sv_sched[1]);
        } end

        it("Instruccion SET") {
            instr->identificador = strdup("SET");
            instr->parametro1 = strdup("AX");
            instr->parametro2 = strdup("10");

            int desalojar = instr_set(instr, ctx, cpu_ctx);
            
            should_int(desalojar) be equal to(0);
            should_int(ctx->registros.AX) be equal to(10);
        } end

        it("Instruccion SUM y SUB") {
            ctx->registros.AX = 15;
            ctx->registros.BX = 5;

            instr->identificador = strdup("SUM");
            instr->parametro1 = strdup("AX");
            instr->parametro2 = strdup("BX");

            int desalojar = instr_sum(instr, ctx, cpu_ctx);
            should_int(desalojar) be equal to(0);
            should_int(ctx->registros.AX) be equal to(20);

            // Ahora probamos SUB
            instr->identificador = strdup("SUB");
            desalojar = instr_sub(instr, ctx, cpu_ctx);
            should_int(desalojar) be equal to(0);
            should_int(ctx->registros.AX) be equal to(15);
        } end

        it("Instruccion JNZ - Salta si registro no es cero") {
            ctx->registros.CX = 1;
            ctx->registros.PC = 10;

            instr->identificador = strdup("JNZ");
            instr->parametro1 = strdup("CX");
            instr->parametro2 = strdup("25");

            int desalojar = instr_jnz(instr, ctx, cpu_ctx);
            should_int(desalojar) be equal to(0);
            should_int(ctx->registros.PC) be equal to(25);
        } end

        it("Instruccion JNZ - No salta si registro es cero") {
            ctx->registros.CX = 0;
            ctx->registros.PC = 10;

            instr->identificador = strdup("JNZ");
            instr->parametro1 = strdup("CX");
            instr->parametro2 = strdup("25");

            int desalojar = instr_jnz(instr, ctx, cpu_ctx);
            should_int(desalojar) be equal to(0);
            should_int(ctx->registros.PC) be equal to(10); // El incremento se realiza en el ciclo de instruccion general
        } end

        it("Instruccion EXIT (Syscall)") {
            ctx->registros.PC = 5;
            instr->identificador = strdup("EXIT");

            int desalojar = syscall_exit(instr, ctx, cpu_ctx);
            
            should_int(desalojar) be equal to(1); // Todas las syscalls deben desalojar
            should_int(ctx->registros.PC) be equal to(6); // La syscall incrementa PC antes de enviar contexto
        } end
        
        it("Instruccion SLEEP (Syscall)") {
            ctx->registros.PC = 5;
            instr->identificador = strdup("SLEEP");
            instr->parametro1 = strdup("1000");

            int desalojar = syscall_sleep(instr, ctx, cpu_ctx);
            
            should_int(desalojar) be equal to(1);
            should_int(ctx->registros.PC) be equal to(6); // Deberia haber sumado 1
        } end
    } end
}
