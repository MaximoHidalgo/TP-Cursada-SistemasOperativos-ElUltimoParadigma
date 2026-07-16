#include "memory_stick_utils.h"

int main(int argc, char *argv[]) {
    t_memory_stick_context* ctx = inicializar_entorno(argc, argv);
    conectar_kernel_memory(ctx);
    iniciar_servidor_cpus(ctx);
    limpiar_contexto(ctx);
    return EXIT_SUCCESS;
}