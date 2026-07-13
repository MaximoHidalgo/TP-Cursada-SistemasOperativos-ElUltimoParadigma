#include "../src/io_utils.h"
#include <commons/config.h>
#include <stdlib.h>
#include <string.h>
#include <cspecs/cspec.h>

context(io_utils_tests) {
    describe("Iniciación de Contexto de IO") {
        it("Falla al no pasar la cantidad correcta de argumentos") {
            char* args[] = {"./io"};
            t_io_context* ctx = iniciar_contexto(1, args);
            should_ptr(ctx) be equal to(NULL);
        } end
        
        it("Falla al pasar un config que no existe") {
            char* args[] = {"./io", "config_falso.config", "STDOUT"};
            t_io_context* ctx = iniciar_contexto(3, args);
            should_ptr(ctx) be equal to(NULL);
        } end

        it("Falla al pasar un tipo de interfaz invalido") {
            char* args[] = {"./io", "configs/io.config", "TIPO_FALSO"};
            t_io_context* ctx = iniciar_contexto(3, args);
            should_ptr(ctx) be equal to(NULL);
        } end
    } end
}
