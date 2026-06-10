#include "peticiones_swap.h"

void procesar_peticion_lectura(t_swap_context* ctx) {
    int numero_bloque = recibir_operacion(ctx->socket_kernel_memory);
    
    log_info(ctx->logger, "## Lectura del bloque: %d", numero_bloque);
    
    void* buffer = malloc(ctx->block_size);
    
    fseek(ctx->swap_file, numero_bloque * ctx->block_size, SEEK_SET);
    fread(buffer, 1, ctx->block_size, ctx->swap_file);
    
    t_paquete* paq = crear_paquete();
    paq->codigo_operacion = LECTURA;
    agregar_a_paquete(paq, buffer, ctx->block_size);
    enviar_paquete(paq, ctx->socket_kernel_memory);
    eliminar_paquete(paq);
    
    free(buffer);
}

void procesar_peticion_escritura(t_swap_context* ctx) {
    int numero_bloque = recibir_operacion(ctx->socket_kernel_memory);
    
    int size;
    void* buffer = recibir_buffer(&size, ctx->socket_kernel_memory);
    
    log_info(ctx->logger, "## Escritura del bloque: %d", numero_bloque);
    
    fseek(ctx->swap_file, numero_bloque * ctx->block_size, SEEK_SET);
    fwrite(buffer, 1, ctx->block_size, ctx->swap_file);
    
    // Es importante hacer flush o fclose no, pero un flush asegura que se guarde en disco
    fflush(ctx->swap_file);
    
    enviar_mensaje("OK", ctx->socket_kernel_memory);
    free(buffer);
}
