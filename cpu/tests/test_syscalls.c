#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <commons/collections/list.h>
#include <utils/protocolo/protocolo.h>
#include "../src/syscalls.h"
#include "../src/notificaciones_scheduler.h"
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

context(test_syscalls) {
    describe("Probando handlers de syscalls en CPU") {
        t_cpu_context* ctx;
        t_contexto_ejecucion* ctx_ejec;
        t_instruccion instr;
        int sv_sched[2];
        int sv_mem[2];

        before {
            ctx = malloc(sizeof(t_cpu_context));
            ctx->logger = log_create("test_sys.log", "TEST", false, LOG_LEVEL_DEBUG);
            ctx->conexiones = malloc(sizeof(t_conexiones_cpu));
            
            socketpair(AF_UNIX, SOCK_STREAM, 0, sv_sched);
            socketpair(AF_UNIX, SOCK_STREAM, 0, sv_mem);
            ctx->conexiones->scheduler = sv_sched[0];
            ctx->conexiones->memoria = sv_mem[0];
            ctx->tam_max_segmento = 64;
            ctx->memory_sticks = list_create();

            ctx_ejec = malloc(sizeof(t_contexto_ejecucion));
            ctx_ejec->pid = 1;
            ctx_ejec->registros.PC = 0;
            ctx_ejec->tabla_segmentos = list_create();
            t_segmento* s = malloc(sizeof(t_segmento));
            s->id = 0; s->base = 1000; s->tamanio = 500;
            list_add(ctx_ejec->tabla_segmentos, s);

            instr.parametro1 = NULL;
            instr.parametro2 = NULL;
            instr.parametro3 = NULL;
        } end

        after {
            log_destroy(ctx->logger);
            free(ctx->conexiones);
            list_destroy_and_destroy_elements(ctx->memory_sticks, free);
            free(ctx);
            list_destroy_and_destroy_elements(ctx_ejec->tabla_segmentos, free);
            free(ctx_ejec);
            close(sv_sched[0]); close(sv_sched[1]);
            close(sv_mem[0]); close(sv_mem[1]);
            remove("test_sys.log");
        } end

        it("syscall_mutex_create invoca notificar_recurso") {
            instr.parametro1 = "MUT1";
            syscall_mutex_create(&instr, ctx_ejec, ctx);
            
            int op = recibir_operacion(sv_sched[1]);
            should_int(op) be equal to(SYSCALL_MUTEX_CREATE);
            
            t_list* paq = recibir_paquete(sv_sched[1]);
            should_string((char*)list_get(paq, 1)) be equal to("MUT1");
            list_destroy_and_destroy_elements(paq, free);
            
            should_int(ctx_ejec->registros.PC) be equal to(1); // PC++
        } end

        it("syscall_mutex_lock invoca notificar_recurso") {
            instr.parametro1 = "MUT2";
            syscall_mutex_lock(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_MUTEX_LOCK);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_mutex_unlock invoca notificar_recurso") {
            instr.parametro1 = "MUT3";
            syscall_mutex_unlock(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_MUTEX_UNLOCK);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_mem_alloc invoca notificar_memoria con atoi") {
            instr.parametro1 = "5";
            instr.parametro2 = "1024";
            syscall_mem_alloc(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_MEM_ALLOC);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_mem_free notifica liberacion") {
            instr.parametro1 = "7";
            syscall_mem_free(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_MEM_FREE);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_sleep notifica sleep") {
            instr.parametro1 = "250";
            syscall_sleep(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_SLEEP);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_stdin traduce y notifica si hay exito") {
            ctx_ejec->registros.EAX = 10;
            ctx_ejec->registros.EBX = 20;
            instr.parametro1 = "EAX";
            instr.parametro2 = "EBX";
            syscall_stdin(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_STDIN);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_stdin desaloja si falla mmu_traducir (Segmentation Fault)") {
            ctx_ejec->registros.EAX = 999; // Error: Segmentation Fault 
            ctx_ejec->registros.EBX = 20;
            instr.parametro1 = "EAX";
            instr.parametro2 = "EBX";
            syscall_stdin(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_SEG_FAULT);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_stdout traduce y notifica si hay exito") {
            ctx_ejec->registros.ECX = 15;
            ctx_ejec->registros.EDX = 30;
            instr.parametro1 = "ECX";
            instr.parametro2 = "EDX";
            syscall_stdout(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_STDOUT);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_init_proc notifica path y prioridad") {
            instr.parametro1 = "script.txt";
            instr.parametro2 = "5";
            syscall_init_proc(&instr, ctx_ejec, ctx);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_INIT_PROC);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end

        it("syscall_exit desaloja y hace PC++") {
            syscall_exit(&instr, ctx_ejec, ctx);
            should_int(ctx_ejec->registros.PC) be equal to(1);
            should_int(recibir_operacion(sv_sched[1])) be equal to(SYSCALL_EXIT);
            list_destroy_and_destroy_elements(recibir_paquete(sv_sched[1]), free);
        } end
    } end
}
