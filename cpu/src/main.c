#include "cpu_utils.h"

int main(int argc, char* argv[]) {
    t_cpu_context* ctx = iniciar_contexto(argc, argv);
    if (!ctx) return EXIT_FAILURE;
    
    atender_kernel_scheduler(ctx);
    limpiar_contexto(ctx);
    return EXIT_SUCCESS;
}