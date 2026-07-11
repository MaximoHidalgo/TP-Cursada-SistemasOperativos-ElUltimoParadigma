#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include "../src/ciclo_instruccion.h"
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

context(test_cpu_ciclo_instruccion) {
    describe("Probando el ciclo de instruccion de CPU") {
        
        t_contexto_ejecucion* ctx;
        t_cpu_context* cpu_ctx;
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

            socketpair(AF_UNIX, SOCK_STREAM, 0, sv_mem);
            socketpair(AF_UNIX, SOCK_STREAM, 0, sv_sched);
            cpu_ctx->conexiones->memoria = sv_mem[0];
            cpu_ctx->conexiones->scheduler = sv_sched[0];
        } end

        after {
            list_destroy_and_destroy_elements(ctx->tabla_segmentos, free);
            free(ctx);
            
            list_destroy_and_destroy_elements(cpu_ctx->memory_sticks, free);
            log_destroy(cpu_ctx->logger);
            free(cpu_ctx->conexiones);
            free(cpu_ctx);

            close(sv_mem[0]);
            close(sv_mem[1]);
            close(sv_sched[0]);
            close(sv_sched[1]);
        } end

        it("Ciclo ejecuta correctamente una instruccion y termina con EXIT") {
            ctx->registros.PC = 0;

            // Preparamos respuestas de memoria simuladas en el socket
            t_paquete* paq1 = crear_paquete();
            paq1->codigo_operacion = ENVIO_INSTRUCCION;
            char* instr1 = "SET AX 5";
            agregar_a_paquete(paq1, instr1, strlen(instr1) + 1);
            enviar_paquete(paq1, sv_mem[1]);
            eliminar_paquete(paq1);

            t_paquete* paq2 = crear_paquete();
            paq2->codigo_operacion = ENVIO_INSTRUCCION;
            char* instr2 = "EXIT";
            agregar_a_paquete(paq2, instr2, strlen(instr2) + 1);
            enviar_paquete(paq2, sv_mem[1]);
            eliminar_paquete(paq2);

            // Ejecutamos el ciclo (leerá SET, luego EXIT, y finalizará el bucle)
            ejecutar_ciclo_de_instruccion(ctx, cpu_ctx);

            // Validamos efectos
            should_int(ctx->registros.AX) be equal to(5);
            // SET incrementa PC (0 -> 1)
            // EXIT (Syscall) incrementa PC (1 -> 2)
            should_int(ctx->registros.PC) be equal to(2);
        } end

        it("Ciclo desalojado por interrupcion de fin de quantum") {
            ctx->registros.PC = 0;

            t_paquete* paq1 = crear_paquete();
            paq1->codigo_operacion = ENVIO_INSTRUCCION;
            char* instr1 = "SET BX 10";
            agregar_a_paquete(paq1, instr1, strlen(instr1) + 1);
            enviar_paquete(paq1, sv_mem[1]);
            eliminar_paquete(paq1);

            // Simulamos una interrupcion enviada por el scheduler
            t_paquete* paq_int = crear_paquete();
            paq_int->codigo_operacion = INTERRUPCION_QUANTUM;
            uint32_t payload = 1; // Debe coincidir con ctx->pid
            agregar_a_paquete(paq_int, &payload, sizeof(uint32_t));
            enviar_paquete(paq_int, sv_sched[1]);
            eliminar_paquete(paq_int);

            // Ejecutamos el ciclo
            ejecutar_ciclo_de_instruccion(ctx, cpu_ctx);

            // Validamos efectos
            should_int(ctx->registros.BX) be equal to(10);
            
            // SET incrementa PC (0 -> 1).
            // Luego checkea interrupción -> hay interrupción -> desaloja.
            // PC debe quedar en 1, ya que rompe el ciclo en ese punto.
            should_int(ctx->registros.PC) be equal to(1);
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
}
