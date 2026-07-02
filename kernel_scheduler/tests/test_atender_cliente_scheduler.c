#include <stdio.h>
#include <assert.h> //assert
#include <stdlib.h>
#include "../src/colas.h"
#include "../src/pcb.h"
#include "../src/atender_cliente_scheduler.h"

void test_atender_cliente() {
    t_args_hilo_scheduler arg;
    arg.cliente_fd = 42;

    arg.ctx = calloc(1, sizeof(t_scheduler_context));
    assert(arg.ctx != NULL);

    arg.ctx->logger = log_create("/tmp/kernel_scheduler_test.log", "TEST", false, LOG_LEVEL_INFO);
    assert(arg.ctx->logger != NULL);

    arg.ctx->config = config_create("configs/scheduler.config");
    assert(arg.ctx->config != NULL);

    arg.ctx->estratega = calloc(1, sizeof(t_estratega_planificacion));
    assert(arg.ctx->estratega != NULL);

    arg.ctx->estratega->logger = arg.ctx->logger;
    arg.ctx->estratega->cola_new = queue_create();
    arg.ctx->estratega->colas = list_create();
    arg.ctx->estratega->quantum_rr = 3;
    arg.ctx->estratega->socket_cpu = -1;
    arg.ctx->estratega->socket_memoria = -1;
    pthread_mutex_init(&arg.ctx->estratega->mutex_colas, NULL);

    t_cola_planificacion* cola_alta = calloc(1, sizeof(t_cola_planificacion));
    cola_alta->nivel = 0;
    cola_alta->algoritmo = ALGORITMO_COLA_FIFO;
    cola_alta->procesos = queue_create();
    list_add(arg.ctx->estratega->colas, cola_alta);

    t_cola_planificacion* cola_baja = calloc(1, sizeof(t_cola_planificacion));
    cola_baja->nivel = 1;
    cola_baja->algoritmo = ALGORITMO_COLA_RR;
    cola_baja->quantum = 3;
    cola_baja->procesos = queue_create();
    list_add(arg.ctx->estratega->colas, cola_baja);

    assert(arg.cliente_fd == 42);
    assert(arg.ctx->estratega->quantum_rr == 3);

    queue_destroy(cola_alta->procesos);
    free(cola_alta);
    queue_destroy(cola_baja->procesos);
    free(cola_baja);
    list_destroy(arg.ctx->estratega->colas);
    queue_destroy(arg.ctx->estratega->cola_new);
    pthread_mutex_destroy(&arg.ctx->estratega->mutex_colas);
    free(arg.ctx->estratega);
    config_destroy(arg.ctx->config);
    log_destroy(arg.ctx->logger);
    free(arg.ctx);

    printf("✅ test_atender_cliente: PASO\n");
}