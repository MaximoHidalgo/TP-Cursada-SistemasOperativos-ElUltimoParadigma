#include "io_utils.h"

int main(int argc, char* argv[]) {
    t_io_context* ctx = iniciar_contexto(argc, argv);
    
    if (ctx == NULL) return EXIT_FAILURE;

    atender_kernel_scheduler(ctx);
    limpiar_contexto(ctx);
    return EXIT_SUCCESS;
}