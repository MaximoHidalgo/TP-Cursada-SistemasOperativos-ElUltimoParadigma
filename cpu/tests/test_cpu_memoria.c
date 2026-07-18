#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <commons/collections/list.h>
#include <utils/protocolo/protocolo.h>

#define main main_test_cpu_memoria
#define recibir_operacion mock_mem_recibir_operacion
#define recibir_buffer mock_mem_recibir_buffer
#define enviar_paquete mock_mem_enviar_paquete

int mock_mem_recibir_operacion(int socket_cliente) {
    return ENVIO_CONTEXTO;
}

static void* buffer_simulado = NULL;
static int size_buffer_simulado = 0;

void* mock_mem_recibir_buffer(int* size, int socket_cliente) {
    *size = size_buffer_simulado;
    void* copia = malloc(*size);
    memcpy(copia, buffer_simulado, *size);
    return copia;
}

void mock_mem_enviar_paquete(t_paquete* paquete, int socket_cliente) {
    // No hacemos nada, solo simulamos que se envió
}

#include "../src/cpu_memoria.c"
#undef main

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

context(test_cpu_memoria) {
    describe("Probando interaccion entre CPU y Memoria (Contexto)") {
        
        t_cpu_context* cpu_ctx;

        before {
            cpu_ctx = malloc(sizeof(t_cpu_context));
            cpu_ctx->conexiones = malloc(sizeof(t_conexiones_cpu));
            cpu_ctx->conexiones->memoria = 100; // Socket dummy
            cpu_ctx->logger = log_create("test_memoria.log", "TEST", false, LOG_LEVEL_DEBUG);
        } end

        after {
            log_destroy(cpu_ctx->logger);
            free(cpu_ctx->conexiones);
            free(cpu_ctx);
            remove("test_memoria.log");
            if (buffer_simulado) {
                free(buffer_simulado);
                buffer_simulado = NULL;
            }
        } end

        it("pedir_contexto_a_memoria deserializa correctamente un contexto enviado por Kernel Memory") {
            // Preparamos un contexto de prueba y lo serializamos para simular la respuesta de Memoria
            t_contexto_ejecucion* ctx_dummy = malloc(sizeof(t_contexto_ejecucion));
            ctx_dummy->pid = 42;
            memset(&(ctx_dummy->registros), 0, sizeof(t_registros));
            ctx_dummy->registros.PC = 10;
            ctx_dummy->registros.AX = 5;
            
            ctx_dummy->tabla_segmentos = list_create();
            t_segmento* seg = malloc(sizeof(t_segmento));
            seg->id = 0;
            seg->base = 100;
            seg->tamanio = 50;
            list_add(ctx_dummy->tabla_segmentos, seg);

            // Serializamos el contexto como si fueramos la Memoria
            t_paquete* paq = crear_paquete();
            serializar_contexto(paq, ctx_dummy);
            
            // Extraemos el payload (sin el op_code ni el tamanio del payload)
            // paq->buffer->stream tiene el tamaño al principio? No, el buffer de t_paquete tiene el stream crudo.
            size_buffer_simulado = paq->buffer->size;
            buffer_simulado = malloc(size_buffer_simulado);
            memcpy(buffer_simulado, paq->buffer->stream, size_buffer_simulado);
            
            eliminar_paquete(paq);
            list_destroy_and_destroy_elements(ctx_dummy->tabla_segmentos, free);
            free(ctx_dummy);

            // Probamos la función (la cual usará los mocks)
            t_contexto_ejecucion* ctx_recibido = pedir_contexto_a_memoria(42, cpu_ctx);

            should_ptr(ctx_recibido) not be null;
            should_int(ctx_recibido->pid) be equal to(42);
            should_int(ctx_recibido->registros.PC) be equal to(10);
            should_int(ctx_recibido->registros.AX) be equal to(5);
            should_int(list_size(ctx_recibido->tabla_segmentos)) be equal to(1);
            
            t_segmento* seg_rec = list_get(ctx_recibido->tabla_segmentos, 0);
            should_int(seg_rec->id) be equal to(0);
            should_int(seg_rec->base) be equal to(100);
            should_int(seg_rec->tamanio) be equal to(50);

            list_destroy_and_destroy_elements(ctx_recibido->tabla_segmentos, free);
            free(ctx_recibido);
        } end
    } end
}
