#include <commons/log.h>
#include <commons/string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../src/peticiones_io.h"
#include <utils/protocolo/protocolo.h>
#include <utils/sockets/sockets.h>

// Importante incluir CSpecs AL FINAL para que el macro 'end' no rompa otras firmas
#include <cspecs/cspec.h>

// Referencias a variables definidas en test_peticiones_io.c
extern char* mock_readline_return;
extern bool mock_readline_retornar_null;

// Helpers duplicados localmente para independizar el archivo
static t_io_context crear_contexto_cspecs(int socket, char* name)
{
    t_io_context ctx;
    ctx.io_name = name;
    ctx.config = NULL;
    ctx.logger = log_create("test_cspecs.log", "TEST_CSPECS_IO", false, LOG_LEVEL_DEBUG);
    ctx.socket_scheduler = socket;
    return ctx;
}

static void crear_socket_cspecs(int s[2])
{
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, s) == 0);
}

static void cerrar_socket_cspecs(int s[2])
{
    close(s[0]);
    close(s[1]);
}

static void recibir_mensaje_cspecs(int socket, int *op, int *size, char *buffer)
{
    recv(socket, op, sizeof(int), MSG_WAITALL);
    recv(socket, size, sizeof(int), MSG_WAITALL);
    recv(socket, buffer, *size, MSG_WAITALL);
}

context(peticiones_io_edge_cases) {

    describe("Edge Cases STDIN") {
        
        before {
            // Resetear estado del mock antes de cada test
            mock_readline_return = NULL;
            mock_readline_retornar_null = false;
        } end
        
        after {
            mock_readline_return = NULL;
            mock_readline_retornar_null = false;
        } end


        it("Padding: Debería rellenar con \\0 si el input es más corto que lo solicitado") {
            int s[2];
            crear_socket_cspecs(s);
            t_io_context ctx = crear_contexto_cspecs(s[0], "STDIN");
            
            mock_readline_return = "hola";
            enviar_int(s[1], 10); // pide 10 bytes
            
            ejecutar_stdin(&ctx, 1);
            
            int op, size;
            char buffer[64];
            memset(buffer, 'X', 64); // Ensucia el buffer para asegurar que los ceros vengan de la red
            recibir_mensaje_cspecs(s[1], &op, &size, buffer);
            
            should_int(op) be equal to(MENSAJE);
            should_int(size) be equal to(10);
            
            // Los primeros 4 bytes son 'h', 'o', 'l', 'a'
            should_bool(strncmp(buffer, "hola", 4) == 0) be equal to(true);
            
            // Los siguientes bytes hasta el 10 deben ser \0
            should_int(buffer[4]) be equal to(0);
            should_int(buffer[9]) be equal to(0);
            
            log_destroy(ctx.logger);
            cerrar_socket_cspecs(s);
        } end

        it("Truncamiento: Debería cortar el string si el input es más largo que lo solicitado") {
            int s[2];
            crear_socket_cspecs(s);
            t_io_context ctx = crear_contexto_cspecs(s[0], "STDIN");
            
            mock_readline_return = "hola mundo"; // 10 caracteres
            enviar_int(s[1], 4); // pero pide 4 bytes
            
            ejecutar_stdin(&ctx, 2);
            
            int op, size;
            char buffer[64];
            memset(buffer, 'X', 64);
            recibir_mensaje_cspecs(s[1], &op, &size, buffer);
            
            should_int(op) be equal to(MENSAJE);
            should_int(size) be equal to(4); // Debe haber enviado solo 4 bytes
            
            // Los 4 bytes deben ser 'h', 'o', 'l', 'a'
            should_bool(strncmp(buffer, "hola", 4) == 0) be equal to(true);
            
            log_destroy(ctx.logger);
            cerrar_socket_cspecs(s);
        } end

        it("EOF/NULL: Debería reemplazar un input NULL (Ctrl+D) por un string vacío y mandar ceros") {
            int s[2];
            crear_socket_cspecs(s);
            t_io_context ctx = crear_contexto_cspecs(s[0], "STDIN");
            
            mock_readline_retornar_null = true; // Simula EOF
            enviar_int(s[1], 5); // pide 5 bytes
            
            ejecutar_stdin(&ctx, 3);
            
            int op, size;
            char buffer[64];
            memset(buffer, 'X', 64);
            recibir_mensaje_cspecs(s[1], &op, &size, buffer);
            
            should_int(op) be equal to(MENSAJE);
            should_int(size) be equal to(5);
            
            // Todos los bytes deben ser ceros
            should_int(buffer[0]) be equal to(0);
            should_int(buffer[4]) be equal to(0);
            
            log_destroy(ctx.logger);
            cerrar_socket_cspecs(s);
        } end
    } end

    describe("Edge Cases SLEEP") {
        it("Debería soportar un sleep de 0 milisegundos sin errores") {
            int s[2];
            crear_socket_cspecs(s);
            t_io_context ctx = crear_contexto_cspecs(s[0], "SLEEP");
            
            enviar_int(s[1], 0); // 0 ms
            
            ejecutar_sleep(&ctx, 4);
            
            int op, size;
            char buffer[64];
            recibir_mensaje_cspecs(s[1], &op, &size, buffer);
            
            should_int(op) be equal to(MENSAJE);
            should_string(buffer) be equal to(MSG_OK);
            
            log_destroy(ctx.logger);
            cerrar_socket_cspecs(s);
        } end
    } end
}
