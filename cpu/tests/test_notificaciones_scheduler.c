#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <commons/collections/list.h>
#include <utils/protocolo/protocolo.h>
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

context(test_notificaciones) {
    describe("Probando notificaciones al scheduler") {
        t_cpu_context* ctx;
        int sv_sched[2];

        before {
            ctx = malloc(sizeof(t_cpu_context));
            ctx->conexiones = malloc(sizeof(t_conexiones_cpu));
            ctx->logger = log_create("test_notif.log", "TEST", false, LOG_LEVEL_DEBUG);
            
            socketpair(AF_UNIX, SOCK_STREAM, 0, sv_sched);
            ctx->conexiones->scheduler = sv_sched[0];
        } end

        after {
            log_destroy(ctx->logger);
            free(ctx->conexiones);
            free(ctx);
            close(sv_sched[0]);
            close(sv_sched[1]);
            remove("test_notif.log");
        } end

        it("notificar_desalojo_a_scheduler envia codigo y pid") {
            notificar_desalojo_a_scheduler(42, INTERRUPCION_QUANTUM, ctx);

            int op = recibir_operacion(sv_sched[1]);
            should_int(op) be equal to(INTERRUPCION_QUANTUM);

            t_list* paq = recibir_paquete(sv_sched[1]);
            should_int(list_size(paq)) be equal to(1);
            should_int(*(uint32_t*)list_get(paq, 0)) be equal to(42);

            list_destroy_and_destroy_elements(paq, free);
        } end

        it("notificar_syscall_sleep_a_scheduler envia sleep y pid") {
            notificar_syscall_sleep_a_scheduler(10, 500, ctx);

            int op = recibir_operacion(sv_sched[1]);
            should_int(op) be equal to(SYSCALL_SLEEP);

            t_list* paq = recibir_paquete(sv_sched[1]);
            should_int(list_size(paq)) be equal to(2);
            should_int(*(uint32_t*)list_get(paq, 0)) be equal to(10);
            should_int(*(uint32_t*)list_get(paq, 1)) be equal to(500);

            list_destroy_and_destroy_elements(paq, free);
        } end

        it("notificar_syscall_recurso_a_scheduler envia op_code, pid y recurso") {
            notificar_syscall_recurso_a_scheduler(SYSCALL_WAIT, 99, "MUTEX", ctx);

            int op = recibir_operacion(sv_sched[1]);
            should_int(op) be equal to(SYSCALL_WAIT);

            t_list* paq = recibir_paquete(sv_sched[1]);
            should_int(list_size(paq)) be equal to(2);
            should_int(*(uint32_t*)list_get(paq, 0)) be equal to(99);
            should_string((char*)list_get(paq, 1)) be equal to("MUTEX");

            list_destroy_and_destroy_elements(paq, free);
        } end

        it("notificar_syscall_memoria_a_scheduler envia op_code, pid, y 2 params") {
            notificar_syscall_memoria_a_scheduler(SYSCALL_MEM_ALLOC, 55, 1024, 2048, ctx);

            int op = recibir_operacion(sv_sched[1]);
            should_int(op) be equal to(SYSCALL_MEM_ALLOC);

            t_list* paq = recibir_paquete(sv_sched[1]);
            should_int(list_size(paq)) be equal to(3);
            should_int(*(uint32_t*)list_get(paq, 0)) be equal to(55);
            should_int(*(uint32_t*)list_get(paq, 1)) be equal to(1024);
            should_int(*(uint32_t*)list_get(paq, 2)) be equal to(2048);

            list_destroy_and_destroy_elements(paq, free);
        } end

        it("notificar_syscall_init_proc_a_scheduler envia op_code, pid, string y param") {
            notificar_syscall_init_proc_a_scheduler(77, "/scripts/test.txt", 3, ctx);

            int op = recibir_operacion(sv_sched[1]);
            should_int(op) be equal to(SYSCALL_INIT_PROC);

            t_list* paq = recibir_paquete(sv_sched[1]);
            should_int(list_size(paq)) be equal to(3);
            should_int(*(uint32_t*)list_get(paq, 0)) be equal to(77);
            should_string((char*)list_get(paq, 1)) be equal to("/scripts/test.txt");
            should_int(*(uint32_t*)list_get(paq, 2)) be equal to(3);

            list_destroy_and_destroy_elements(paq, free);
        } end
    } end
}
