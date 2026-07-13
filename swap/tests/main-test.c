#include <stdio.h>
#include "assert.h"

// Eliminamos la definición de main porque ahora se usa swap_utils
#include "../src/swap_utils.h"

void test_crear_archivo_swap_old() {
     t_swap_context ctx;

    // Inicializamos solo los campos necesarios
    ctx.swap_file_path = "swap_test_old.bin";
    ctx.swap_file_size = 1024;
    ctx.swap_file = NULL;

    // No se usan en este test, pero los dejamos en NULL
    ctx.logger = NULL;
    ctx.config = NULL;
    ctx.socket_kernel_memory = -1;

    crear_archivo_swap(&ctx);

    // Verificamos que el archivo se abrió correctamente
    assert(ctx.swap_file != NULL);

    // Verificamos el tamaño del archivo
    fseek(ctx.swap_file, 0, SEEK_END);
    long tamanio = ftell(ctx.swap_file);

    assert(tamanio == 1024);

    fclose(ctx.swap_file);
    remove("swap_test_old.bin");

    printf("test_crear_archivo_swap PASO\n");
}
