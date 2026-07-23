#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "../src/scheduler_syscalls.h"
#include "../src/scheduler_dispatcher.h"

static t_scheduler_context* crear_contexto_test(void)
{
    t_scheduler_context* ctx = calloc(1, sizeof(*ctx));
    assert(ctx != NULL);
    ctx->logger = log_create("/tmp/kernel_scheduler_test.log","TEST",false,LOG_LEVEL_INFO);
    assert(ctx->logger != NULL);
    return ctx;
}

static void destruir_contexto_test(t_scheduler_context* ctx)
{
    log_destroy(ctx->logger);
    free(ctx);
}

static void verificar(const char* nombre, int resultado, int esperado)
{
    printf("[TEST] %s... ", nombre);
    assert(resultado == esperado);
    printf("OK\n");
}

void test_atender_cliente() {
    t_scheduler_context* ctx = crear_contexto_test();
    assert(ctx != NULL);


    // La nueva lógica centraliza el despacho en atenderRespuesta.
    // Para un cliente desconectado o un opcode inválido, debe terminar el procesamiento.
    verificar("Cliente desconectado", atenderRespuesta(-1, ctx, -1),-1);
    verificar("Opcode fuera de rango (256)", atenderRespuesta(-1, ctx, 256),-1);
    verificar("Opcode muy fuera de rango (999)", atenderRespuesta(-1, ctx, 999), -1);
    verificar("Opcode no mapeado (254)", atenderRespuesta(-1, ctx, 254), -1);

    destruir_contexto_test(ctx);
    printf("✅ test_atender_cliente: PASO\n");

}