#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/estructuras.h"

#include "../src/swap_utils.c"
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

context(test_swap_utils) {
    describe("Probando utilidades del SWAP") {
        it("crear_archivo_swap crea correctamente un archivo del tamanio indicado") {
             t_swap_context ctx;
             ctx.swap_file_path = "swap_test.bin";
             ctx.swap_file_size = 1024;
             ctx.swap_file = NULL;
             ctx.logger = NULL;
             ctx.config = NULL;
             ctx.socket_kernel_memory = -1;
             
             crear_archivo_swap(&ctx);
             
             should_ptr(ctx.swap_file) not be null;
             
             fseek(ctx.swap_file, 0, SEEK_END);
             long tamanio = ftell(ctx.swap_file);
             
             should_int(tamanio) be equal to(1024);
             
             fclose(ctx.swap_file);
             remove("swap_test.bin");
        } end
    } end
}
