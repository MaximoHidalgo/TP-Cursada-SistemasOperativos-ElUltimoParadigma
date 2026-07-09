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

#include "../src/cpu_utils.c"
#undef main

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

context(test_cpu_utils) {
    describe("Probando utils de CPU") {
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

        it("procesar_tabla_de_sticks deserializa correctamente los memory sticks") {
            t_cpu_context* ctx = malloc(sizeof(t_cpu_context));
            ctx->id_cpu = strdup("TEST_STICKS");
            ctx->logger = log_create("test_sticks.log", "TEST", false, LOG_LEVEL_INFO);
            ctx->memory_sticks = list_create();
            ctx->conexiones = NULL; // para evitar crash en limpiar
            
            // Construimos un buffer serializado manualmente tal como lo envía el Kernel Memory
            t_paquete* paq = crear_paquete();
            
            int cant = 1;
            agregar_a_paquete(paq, &cant, sizeof(int));
            
            int id = 42;
            agregar_a_paquete(paq, &id, sizeof(int));
            uint32_t tamanio = 2048;
            agregar_a_paquete(paq, &tamanio, sizeof(uint32_t));
            uint32_t base = 100;
            agregar_a_paquete(paq, &base, sizeof(uint32_t));
            int puerto = 8080;
            agregar_a_paquete(paq, &puerto, sizeof(int));
            
            char* ip = "127.0.0.1";
            int ip_len = strlen(ip) + 1;
            agregar_a_paquete(paq, &ip_len, sizeof(int)); // Envía la longitud de la IP primero
            agregar_a_paquete(paq, ip, ip_len);           // Envía la IP
            
            // Ejecutamos la función a testear
            procesar_tabla_de_sticks(ctx, paq->buffer->stream, paq->buffer->size);
            
            // Validamos la correcta deserialización
            should_int(list_size(ctx->memory_sticks)) be equal to(1);
            
            t_memory_stick_info* stick = list_get(ctx->memory_sticks, 0);
            should_int(stick->id) be equal to(42);
            should_int(stick->tamanio) be equal to(2048);
            should_int(stick->base_address) be equal to(100);
            should_int(stick->puerto) be equal to(8080);
            should_string(stick->ip) be equal to("127.0.0.1");
            
            // Cleanup
            eliminar_paquete(paq);
            remove("test_sticks.log"); // borrar log file temporal
            
            list_destroy_and_destroy_elements(ctx->memory_sticks, free);
            log_destroy(ctx->logger);
            free(ctx->id_cpu);
            free(ctx);
        } end
    } end
}
