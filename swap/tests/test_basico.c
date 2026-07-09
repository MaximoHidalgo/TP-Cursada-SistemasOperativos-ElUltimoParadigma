#include <cspecs/cspec.h>

context(test_basico_swap) {
    describe("Prueba basica para verificar que cspecs compila correctamente") {
        it("Debe pasar siempre") {
            should_bool(true) be equal to(true);
        } end
    } end
}
