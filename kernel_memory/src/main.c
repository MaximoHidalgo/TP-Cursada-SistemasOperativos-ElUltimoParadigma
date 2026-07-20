#include "main.h"

int main(int argc, char *argv[]) {
    t_memory_context* ctx = iniciar_contexto(argc, argv);
    if (ctx == NULL) {
        return EXIT_FAILURE;
    }

    if (iniciar_servidor_memoria(ctx) == -1) {
        limpiar_contexto(ctx);
        return EXIT_FAILURE;
    }

    iniciar_escucha_servidor(ctx);
    
    limpiar_contexto(ctx);
    return EXIT_SUCCESS;
}