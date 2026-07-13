#include "peticiones_swap.h"

void procesar_peticion_lectura(t_swap_context* ctx) {
    int numero_bloque = recibir_operacion(ctx->socket_kernel_memory);
    
    log_info(ctx->logger, "## Lectura del bloque: %d", numero_bloque);
    
    void* buffer = malloc(ctx->block_size);
    
    leer_bloque_swap(ctx->swap_file, numero_bloque, ctx->block_size, buffer);
    
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
    
    escribir_bloque_swap(ctx->swap_file, numero_bloque, ctx->block_size, buffer);
    
    enviar_mensaje("OK", ctx->socket_kernel_memory);
    free(buffer);
}

void escribir_bloque_swap(FILE* archivo,int numero_bloque,int block_size,void* buffer)
{
    fseek(archivo, numero_bloque * block_size, SEEK_SET);

    fwrite(buffer, 1, block_size, archivo);

    fflush(archivo);
}

void leer_bloque_swap(FILE* archivo, int numero_bloque, int block_size, void* destino)
{
    fseek(archivo, numero_bloque * block_size, SEEK_SET);

    fread(destino,1,block_size,archivo);
}