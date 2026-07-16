#include "../src/memory_stick_utils.h"
#include <cspecs/cspec.h>

context(test_memory_stick_utils) {
    describe("Prueba de inicializacion de entorno de memory stick") {
        it("Debe inicializar el contexto correctamente con argumentos validos") {
            char* args[] = {"./memory_stick", "configs/memory_stick.config", "1024"};
            t_memory_stick_context* ctx = inicializar_entorno(3, args);

            should_bool(ctx != NULL) be equal to(true);
            should_bool(ctx->config != NULL) be equal to(true);
            should_bool(ctx->logger != NULL) be equal to(true);
            should_bool(ctx->espacio_memoria != NULL) be equal to(true);
            should_int(ctx->tamanio_total) be equal to(1024);
            should_string(ctx->puerto_escucha) be equal to("8002");
            should_string(ctx->ip_memory) be equal to("127.0.0.1");

            limpiar_contexto(ctx);
        } end
    } end
}
