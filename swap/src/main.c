#include "swap_utils.h"

int main(int argc, char* argv[]) {
    t_swap_context* ctx = iniciar_contexto(argc, argv);
    
    crear_archivo_swap(ctx);
    conectar_kernel_memory(ctx);
    atender_kernel_memory(ctx);
    limpiar_contexto(ctx);
    return 0;
}
