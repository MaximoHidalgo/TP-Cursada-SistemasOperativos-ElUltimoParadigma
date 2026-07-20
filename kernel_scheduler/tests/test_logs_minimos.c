#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <commons/config.h>
#include <commons/log.h>

#include "../src/planificador.h"
#include "../src/rr.h"


typedef struct
{
    const char* path;
    t_log* logger;
    t_config* config;
    t_estratega_planificacion* estratega;
} t_log_test_context;


static void verificar(const char* nombre,bool condicion)
{
    printf("[TEST] %s... ", nombre);
    assert(condicion);
    printf("OK\n");
}

static t_proceso_planificable* crear_proceso_test(uint32_t pid,uint32_t prioridad)
{
    t_proceso_planificable* proceso = calloc(1, sizeof(*proceso));
    assert(proceso != NULL);
    proceso->pid = pid;
    proceso->cola_prioridad = prioridad;

    return proceso;
}

static char* leer_log_completo(
    const char* path)
{
    FILE* archivo = fopen(path, "rb");

    assert(archivo != NULL);

    fseek(archivo, 0L, SEEK_END);
    long size = ftell(archivo);
    
    rewind(archivo);
    char* contenido = calloc(size + 1, sizeof(char));
    assert(contenido != NULL);
    size_t leidos = fread(contenido, 1, size, archivo);
    assert(leidos == (size_t)size);
    fclose(archivo);

    return contenido;
}

static char* obtener_logs(const char* path)
{
    usleep(5000);
    return leer_log_completo(path);
}

static void verificar_log(const char* descripcion,const char* contenido,const char* esperado)
{
    printf("[LOG ] %s... ", descripcion);
    assert(strstr(contenido, esperado) != NULL);
    printf("OK\n");
}

static void verificar_log_archivo(const char* descripcion,const char* path,const char* esperado)
{
    char* contenido = obtener_logs(path);
    verificar_log(descripcion,contenido,esperado);
    free(contenido);
}

static t_log_test_context*
crear_contexto_logs_test(
    const char* path)
{
    t_log_test_context* ctx =
        calloc(1, sizeof(*ctx));

    assert(ctx != NULL);

    ctx->path = path;

    remove(path);

    ctx->logger =
        log_create(
            (char*)path,
            "TEST",
            false,
            LOG_LEVEL_INFO);

    assert(ctx->logger != NULL);

    ctx->config =
        config_create(
            "configs/scheduler.config");

    assert(ctx->config != NULL);

    ctx->estratega =
        crear_estratega_planificacion(
            ctx->config,
            ctx->logger);

    assert(ctx->estratega != NULL);
    /*
    * Reducimos el timeout para que los tests
    * no tengan que esperar demasiado.
    */
    ctx->estratega->suspension_timeout = 1;

    return ctx;
}

static void destruir_contexto_logs_test(
    t_log_test_context* ctx)
{
    if (ctx == NULL)
        return;

    if (ctx->estratega != NULL)
        destruir_estratega_planificacion(
            ctx->estratega);

    if (ctx->logger != NULL)
        log_destroy(ctx->logger);

    if (ctx->config != NULL)
        config_destroy(ctx->config);

    free(ctx);
}

void test_log_creacion_proceso()
{
   
    printf("test_log_creacion_proceso\n");


    t_log_test_context* ctx =
        crear_contexto_logs_test(
            "/tmp/kernel_scheduler_log_creacion.log");

    t_proceso_planificable* proceso =
        crear_proceso_test(
            42,
            0);

    verificar(
        "Encolar proceso NEW",
        planificador_encolar_new(
            ctx->estratega,
            proceso));

    planificador_admitir_a_ready(ctx->estratega);
    verificar("Admitir proceso READY", true);

    verificar_log_archivo(
        "Creación del proceso",
        ctx->path,
        "## (42) Se crea el proceso - Estado: NEW");

    verificar_log_archivo(
        "Cambio NEW -> READY",
        ctx->path,
        "## (42) Pasa del estado NEW al estado READY");

    free(proceso);
    destruir_contexto_logs_test(ctx);

    printf("✅ test_log_creacion_proceso: PASO\n");
}