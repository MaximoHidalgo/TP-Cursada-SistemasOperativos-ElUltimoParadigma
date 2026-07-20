#include "main.h"

int main(int argc, char* argv[]) {
    t_scheduler_context* ctx = iniciar_contexto(argc, argv);

    conectar_kernel_memory(ctx);
    iniciar_planificacion(ctx);
    iniciar_servidor_clientes(ctx);

    limpiar_contexto(ctx);
    return EXIT_SUCCESS;
}



