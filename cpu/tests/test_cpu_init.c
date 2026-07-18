#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <commons/collections/list.h>
#include <utils/protocolo/protocolo.h>

#define main main_del_cpu
#define crear_conexion mock_crear_conexion
#define enviar_mensaje mock_enviar_mensaje
#define send mock_send
#define recibir_operacion mock_recibir_operacion
#define recibir_paquete mock_recibir_paquete
#define recibir_buffer mock_recibir_buffer

int mock_crear_conexion(char* ip, char* puerto) { return 100; }
void mock_enviar_mensaje(char* msg, int socket) {}
ssize_t mock_send(int sockfd, const void *buf, size_t len, int flags) { return len; }

static int recibir_op_count = 0;

int mock_recibir_operacion(int socket_cliente) {
    if (recibir_op_count == 0) {
        recibir_op_count++;
        return TAM_MAX_SEGMENTO;
    } else if (recibir_op_count == 1) {
        recibir_op_count++;
        return ENVIO_TABLA_STICKS;
    }
    return -1;
}

t_list* mock_recibir_paquete(int socket_cliente) {
    t_list* lista = list_create();
    int* val = malloc(sizeof(int));
    *val = 128;
    list_add(lista, val);
    return lista;
}

void* mock_recibir_buffer(int* size, int socket_cliente) {
    *size = sizeof(int) * 2;
    void* buffer = malloc(*size);
    int desp = 0;
    int payload_size = sizeof(int);
    memcpy(buffer + desp, &payload_size, sizeof(int)); desp += sizeof(int);
    int cant = 0;
    memcpy(buffer + desp, &cant, sizeof(int));
    return buffer;
}

#include "../src/cpu_init.c"
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

context(test_cpu_init) {
    describe("Probando inicializacion de CPU") {
        it("Inicia y limpia el contexto de ejecucion correctamente") {
            char* args[] = {"./cpu", "configs/cpu.config", "CPU_TEST"};
            
            // Reiniciamos contadores para el mock
            recibir_op_count = 0;

            t_cpu_context* ctx = iniciar_contexto(3, args);

            should_ptr(ctx) not be null;
            should_int(ctx->tam_max_segmento) be equal to(128);
            should_string(ctx->id_cpu) be equal to("CPU_TEST");
            should_ptr(ctx->config) not be null;
            should_ptr(ctx->logger) not be null;
            should_ptr(ctx->conexiones) not be null;
            should_ptr(ctx->memory_sticks) not be null;
            should_int(list_size(ctx->memory_sticks)) be equal to(0);

            // Validamos que se pueda limpiar sin leaks/crashes
            limpiar_contexto(ctx);
        } end

        it("inicializar_entorno configura logger y config correctamente") {
            t_cpu_context* ctx = malloc(sizeof(t_cpu_context));
            ctx->id_cpu = strdup("TEST_ENTORNO");
            ctx->conexiones = NULL; // para limpieza
            
            bool ok = inicializar_entorno(ctx, "configs/cpu.config");
            
            should_bool(ok) be equal to(true);
            should_ptr(ctx->config) not be null;
            should_ptr(ctx->logger) not be null;
            
            limpiar_contexto(ctx);
        } end

        it("Fallo por argumentos invalidos en iniciar_contexto") {
            // Test vacio ya que abortaría el proceso con exit()
        } end

        it("Fallo al crear logger por falta de archivo config") {
            t_cpu_context* ctx = malloc(sizeof(t_cpu_context));
            ctx->id_cpu = strdup("TEST_ENTORNO_ERR");
            ctx->conexiones = NULL; // para limpieza
            
            bool ok = inicializar_entorno(ctx, "configs/no_existe.config");
            
            should_bool(ok) be equal to(false);
            should_ptr(ctx->config) be null;
            should_ptr(ctx->logger) not be null; // fallback logger was created
            
            limpiar_contexto(ctx);
        } end
    } end
}
