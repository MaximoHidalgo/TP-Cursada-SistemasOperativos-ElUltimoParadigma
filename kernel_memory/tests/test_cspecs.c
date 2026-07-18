#include <string.h>

// Renombramos main para evitar colisión con el test runner
#define main main_del_kernel
#include "../src/main.c"
#undef main

// IMPORTANTE: cspec.h DEBE ser siempre el último include de tu archivo.
// Esto es porque cspec define palabras clave como "end" que rompen
// los archivos de las commons (como string.h) si se incluyen después.
#include <cspecs/cspec.h>

// --- HACK PARA EL LINTER DE VSCODE ---
// CSpecs usa extensiones de GNU C que el analizador de VSCode no entiende,
// marcando errores rojos aunque compile bien. Con esto engañamos al editor:
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
// -------------------------------------

context(test_kernel_memory) {
    describe("Iniciando contexto de memoria") {
        
        t_memory_context* ctx_prueba;

        before {
            // Setup que corre antes de cada test (it)
            int cant_args = 2;
            char* lista_args[2] = {"./kernel_memory_test", "configs/kernelMemory.config"};
            ctx_prueba = iniciar_contexto(cant_args, lista_args);
        } end

        after {
            // Teardown: limpiamos memoria luego de que termine el test
            if (ctx_prueba != NULL) {
                // limpiar_contexto(ctx_prueba); // Opcional, si está implementado
            }
        } end

        it("El contexto y logger deben inicializarse correctamente") {
            should_ptr(ctx_prueba) not be null;
            should_ptr(ctx_prueba->logger) not be null;
        } end

        it("Los valores del archivo config se cargan bien en el contexto") {
            should_int(ctx_prueba->instruction_delay) be equal to(500);
            should_int(ctx_prueba->segment_max_size) be equal to(128);
            should_int(ctx_prueba->memoria_total) be equal to(0);
            should_string(ctx_prueba->allocation_strategy) be equal to("BEST");
            should_string(ctx_prueba->base_path) be equal to("../utils/scripts");
        } end
        
        it("Las listas internas de administracion arrancan vacias") {
            should_bool(list_is_empty(ctx_prueba->programas)) be equal to(true);
            should_bool(list_is_empty(ctx_prueba->huecos_libres)) be equal to(true);
            should_bool(list_is_empty(ctx_prueba->segmentos_globales)) be equal to(true);
            should_bool(list_is_empty(ctx_prueba->memory_sticks)) be equal to(true);
        } end
        
        it("Las estructuras de paginacion/swap inician deshabilitadas o nulas") {
            should_int(ctx_prueba->swap_socket) be equal to(-1);
            should_ptr(ctx_prueba->swap_bitmap) be null;
            should_ptr(ctx_prueba->bitarray_data) be null;
            should_ptr(ctx_prueba->espacio_memoria) be null;
        } end
    } end
}
