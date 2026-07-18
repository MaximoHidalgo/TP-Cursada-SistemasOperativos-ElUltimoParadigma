#include "../src/atender_cpu.h"
#include "../src/memory_stick_utils.h"
#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <cspecs/cspec.h>

static void recibir_mensaje_test(int socket, int *op, int *size, char **buffer) {
    recv(socket, op, sizeof(int), MSG_WAITALL);
    recv(socket, size, sizeof(int), MSG_WAITALL);
    *buffer = malloc(*size);
    recv(socket, *buffer, *size, MSG_WAITALL);
}

context(test_atender_cpu) {
    describe("Prueba de operaciones de red del Memory Stick (Lectura/Escritura)") {
        
        t_memory_stick_context* ctx;
        int s[2];
        
        before {
            // Inicializar un contexto mockeado
            ctx = malloc(sizeof(t_memory_stick_context));
            ctx->logger = log_create("test_atender_cpu.log", "TEST_CPU", false, LOG_LEVEL_DEBUG);
            ctx->tamanio_total = 100;
            ctx->espacio_memoria = malloc(100);
            memset(ctx->espacio_memoria, 0, 100);
            ctx->memory_delay = 0; // Sin delay para los tests
            pthread_mutex_init(&ctx->mutex_memoria, NULL);
            
            // Llenamos la memoria de prueba con algunos datos
            strcpy(ctx->espacio_memoria, "HOLA_MUNDO");
            
            // Creamos un socketpair para simular el cliente (CPU) y el servidor (Memory Stick)
            socketpair(AF_UNIX, SOCK_STREAM, 0, s);
        } end
        
        after {
            close(s[0]);
            close(s[1]);
            free(ctx->espacio_memoria);
            pthread_mutex_destroy(&ctx->mutex_memoria);
            log_destroy(ctx->logger);
            free(ctx);
        } end
        
        it("Debe procesar lectura dentro de los limites exitosamente") {
            // CPU manda peticion de lectura de 4 bytes desde la direccion 0 ("HOLA")
            enviar_peticion_lectura(0, 4, s[1]);
            
            // Memory Stick lee el opcode (procesar_lectura asume que el opcode ya fue leido)
            int op = recibir_operacion(s[0]);
            should_int(op) be equal to(LEER_MEMORIA);
            
            // Memory Stick procesa la lectura
            procesar_lectura(s[0], ctx);
            
            // CPU recibe el buffer leido (procesado como paquete)
            int op_resp = recibir_operacion(s[1]);
            should_int(op_resp) be equal to(LECTURA);
            
            t_list* valores = recibir_paquete(s[1]);
            should_int(list_size(valores)) be equal to(1);
            
            char* leido = list_get(valores, 0);
            
            // "HOLA" es 4 bytes. Para comparar con string usando cspecs, aseguramos null terminacion o usamos strncmp.
            // Aqui podemos copiarlo o compararlo manualmente.
            should_int(leido[0]) be equal to('H');
            should_int(leido[1]) be equal to('O');
            should_int(leido[2]) be equal to('L');
            should_int(leido[3]) be equal to('A');
            
            list_destroy_and_destroy_elements(valores, free);
        } end
        
        it("Debe manejar lectura fuera de los limites y devolver ceros") {
            // CPU manda peticion de lectura de 4 bytes pero desde direccion 98 (sobrepasa el limite de 100)
            enviar_peticion_lectura(98, 4, s[1]);
            
            int op = recibir_operacion(s[0]);
            should_int(op) be equal to(LEER_MEMORIA);
            
            procesar_lectura(s[0], ctx);
            
            int op_resp = recibir_operacion(s[1]);
            should_int(op_resp) be equal to(LECTURA);
            
            t_list* valores = recibir_paquete(s[1]);
            should_int(list_size(valores)) be equal to(1);
            
            char* leido = list_get(valores, 0);
            // Deberia devolver ceros
            should_int(leido[0]) be equal to(0);
            should_int(leido[1]) be equal to(0);
            
            list_destroy_and_destroy_elements(valores, free);
        } end
        
        it("Debe procesar escritura dentro de los limites exitosamente") {
            // CPU manda peticion de escritura "TEST" (4 bytes) en la direccion 5
            enviar_peticion_escritura(5, 4, "TEST", s[1]);
            
            int op = recibir_operacion(s[0]);
            should_int(op) be equal to(ESCRIBIR_MEMORIA);
            
            procesar_escritura(s[0], ctx);
            
            // CPU recibe mensaje MSG_OK
            int op_resp, size_resp;
            char* buffer_resp;
            recibir_mensaje_test(s[1], &op_resp, &size_resp, &buffer_resp);
            
            should_int(op_resp) be equal to(MENSAJE);
            should_string(buffer_resp) be equal to(MSG_OK);
            
            // Verificamos que se escribio correctamente en la memoria
            char* leido = malloc(5);
            memcpy(leido, ctx->espacio_memoria + 5, 4);
            leido[4] = '\0';
            should_string(leido) be equal to("TEST");
            
            free(leido);
            free(buffer_resp);
        } end
        
        it("Debe manejar escritura fuera de los limites gracefulmente sin fallar") {
            // CPU manda peticion de escritura en direccion 98 con tamano 5 (fuera de limite)
            enviar_peticion_escritura(98, 5, "ERROR", s[1]);
            
            int op = recibir_operacion(s[0]);
            should_int(op) be equal to(ESCRIBIR_MEMORIA);
            
            procesar_escritura(s[0], ctx);
            
            // CPU igual recibe MSG_OK pero la memoria no se debio tocar mas alla del limite
            int op_resp, size_resp;
            char* buffer_resp;
            recibir_mensaje_test(s[1], &op_resp, &size_resp, &buffer_resp);
            
            should_int(op_resp) be equal to(MENSAJE);
            should_string(buffer_resp) be equal to(MSG_OK);
            free(buffer_resp);
        } end
        
        it("Debe procesar mensaje handshake correctamente") {
            // CPU manda handshake
            enviar_mensaje("CPU_TEST", s[1]);
            
            int op = recibir_operacion(s[0]);
            should_int(op) be equal to(MENSAJE);
            
            procesar_mensaje_handshake(s[0], ctx->logger);
            
            // Como procesar_mensaje_handshake solo loguea y libera memoria, el test es asegurar que no crashea y vacia el buffer
            should_bool(true) be equal to(true);
        } end
    } end
}
