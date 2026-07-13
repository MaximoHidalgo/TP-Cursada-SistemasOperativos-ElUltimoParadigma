#include <stdio.h>
#include <cspecs/cspec.h>

void correr_tests_legacy(void);

context(tests_viejos_io) {
    describe("Ejecutando suite de tests viejos de IO") {
        it("Deberian pasar todos los asserts de la suite antigua") {
            correr_tests_legacy();
        } end
    } end
}
