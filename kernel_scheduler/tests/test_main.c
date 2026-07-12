#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../src/colas.h"
#include "../src/pcb.h"
#include "../src/atender_cliente_scheduler.h"
#include "../src/main.h"

#define main kernel_scheduler_main
#include "../src/main.c"
#undef main

static t_scheduler_context* crear_contexto_test(void) {
    t_scheduler_context* ctx = calloc(1, sizeof(t_scheduler_context));
    assert(ctx != NULL);

    ctx->logger = log_create("/tmp/kernel_scheduler_test.log", "TEST", false, LOG_LEVEL_INFO);
    assert(ctx->logger != NULL);

    ctx->config = config_create("configs/scheduler.config");
    assert(ctx->config != NULL);

    ctx->estratega = calloc(1, sizeof(t_estratega_planificacion));
    assert(ctx->estratega != NULL);

    ctx->estratega->logger = ctx->logger;
    ctx->estratega->cola_new = queue_create();
    ctx->estratega->colas = list_create();
    ctx->estratega->quantum_rr = 3;
    ctx->estratega->socket_cpu = -1;
    ctx->estratega->socket_memoria = -1;
    pthread_mutex_init(&ctx->estratega->mutex_colas, NULL);

    return ctx;
}

static void destruir_contexto_test(t_scheduler_context* ctx, int socket_memoria_cliente, int socket_memoria_servidor) {
    if (ctx == NULL) {
        return;
    }

    if (socket_memoria_cliente >= 0) {
        close(socket_memoria_cliente);
    }
    if (socket_memoria_servidor >= 0) {
        close(socket_memoria_servidor);
    }

    if (ctx->estratega != NULL) {
        if (ctx->estratega->cola_new != NULL) {
            queue_destroy(ctx->estratega->cola_new);
        }
        if (ctx->estratega->colas != NULL) {
            list_destroy(ctx->estratega->colas);
        }
        pthread_mutex_destroy(&ctx->estratega->mutex_colas);
        free(ctx->estratega);
    }

    if (ctx->config != NULL) {
        config_destroy(ctx->config);
    }

    if (ctx->logger != NULL) {
        log_destroy(ctx->logger);
    }

    free(ctx);
}

static void agregar_cola_ready(t_estratega_planificacion* estratega, int nivel, t_algoritmo_cola algoritmo, int quantum) {
    t_cola_planificacion* cola = calloc(1, sizeof(t_cola_planificacion));
    cola->nivel = nivel;
    cola->algoritmo = algoritmo;
    cola->quantum = quantum;
    cola->procesos = queue_create();
    list_add(estratega->colas, cola);
}

void test_inicializacion_de_planificador(void) {
    t_scheduler_context* ctx = crear_contexto_test();
    assert(ctx != NULL);

    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    ctx->path_script_inicial = "./scripts/PLANI_PRE_0.prc";
    ctx->socket_memoria = sockets[0];
    agregar_cola_ready(ctx->estratega, 0, ALGORITMO_COLA_FIFO, 0);

    iniciar_planificacion(ctx);

    assert(queue_is_empty(ctx->estratega->cola_new));

    t_cola_planificacion* cola_ready = list_get(ctx->estratega->colas, 0);
    assert(cola_ready != NULL);
    assert(queue_size(cola_ready->procesos) == 1);

    t_proceso_planificable* proceso_ready = queue_pop(cola_ready->procesos);
    assert(proceso_ready != NULL);
    assert(proceso_ready->pid == 0);
    assert(proceso_ready->estado == ESTADO_PROCESO_READY);

    free(proceso_ready);
    destruir_contexto_test(ctx, sockets[0], sockets[1]);

    printf("✅ test_inicializacion_de_planificador: PASO\n");
}