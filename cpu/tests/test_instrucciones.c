#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include "../src/instrucciones.h"
#include "../src/syscalls.h"
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
            t_segmento* s = malloc(sizeof(t_segmento));
            s->id = 0; s->base = 1000; s->tamanio = 500;
            list_add(ctx->tabla_segmentos, s);
            
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

            t_memory_stick_info* stick = malloc(sizeof(t_memory_stick_info));
            stick->id = 1;
            stick->base_address = 1000;
            stick->tamanio = 500;
            stick->socket_fd = sv_mem[0];
            list_add(cpu_ctx->memory_sticks, stick);

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

        it("Instruccion MOV_IN lee de memoria y guarda en registro") {
            ctx->registros.PC = 0;
            ctx->registros.SI = 10; // dir logica
            instr->identificador = strdup("MOV_IN");
            instr->parametro1 = strdup("EAX"); // Registro destino (4 bytes)

            // Simular respuesta de LECTURA_MEMORIA_DISTRIBUIDA
            int op = LECTURA;
            int size_payload = 4;
            uint32_t val = 2024;
            send(sv_mem[1], &op, sizeof(int), 0);
            send(sv_mem[1], &size_payload, sizeof(int), 0);
            send(sv_mem[1], &val, sizeof(uint32_t), 0);

            int desalojar = instr_mov_in(instr, ctx, cpu_ctx);
            
            should_int(desalojar) be equal to(0);
            should_int(ctx->registros.EAX) be equal to(2024);
        } end

        it("Instruccion MOV_IN desaloja por Segmentation Fault") {
            ctx->registros.SI = 999; // Provocará fallo en mmu
            instr->identificador = strdup("MOV_IN");
            instr->parametro1 = strdup("EAX");
            
            int desalojar = instr_mov_in(instr, ctx, cpu_ctx);
            should_int(desalojar) be equal to(1); // Desaloja por SegFault
        } end

        it("Instruccion MOV_OUT lee registro y guarda en memoria") {
            ctx->registros.PC = 0;
            ctx->registros.DI = 10; // dir logica
            ctx->registros.EBX = 500; // valor a guardar
            instr->identificador = strdup("MOV_OUT");
            instr->parametro1 = strdup("EBX"); // Registro valor (4 bytes)

            // Simular respuesta de ESCRITURA_MEMORIA_DISTRIBUIDA
            int op = MENSAJE;
            int size_payload = 3;
            char msj[] = "OK";
            send(sv_mem[1], &op, sizeof(int), 0);
            send(sv_mem[1], &size_payload, sizeof(int), 0);
            send(sv_mem[1], msj, 3, 0);

            int desalojar = instr_mov_out(instr, ctx, cpu_ctx);
            
            should_int(desalojar) be equal to(0);
        } end

        it("Instruccion COPY_MEM lee de memoria y escribe en memoria") {
            ctx->registros.PC = 0;
            ctx->registros.SI = 10; // dir logica origen
            ctx->registros.DI = 20; // dir logica destino
            ctx->registros.ECX = 5; // tamaño en ECX
            instr->identificador = strdup("COPY_MEM");
            instr->parametro1 = strdup("ECX"); // tamaño a copiar

            // 1. Simular respuesta de LECTURA
            int op_lectura = LECTURA;
            int size_payload = 5;
            char string_val[] = "HOLA!";
            send(sv_mem[1], &op_lectura, sizeof(int), 0);
            send(sv_mem[1], &size_payload, sizeof(int), 0);
            send(sv_mem[1], string_val, 5, 0);

            // 2. Simular respuesta de ESCRITURA
            int op_escritura = MENSAJE;
            int size_payload_esc = 3;
            char msj[] = "OK";
            send(sv_mem[1], &op_escritura, sizeof(int), 0);
            send(sv_mem[1], &size_payload_esc, sizeof(int), 0);
            send(sv_mem[1], msj, 3, 0);

            int desalojar = instr_copy_mem(instr, ctx, cpu_ctx);
            
            should_int(desalojar) be equal to(0);
        } end
    } end
}
