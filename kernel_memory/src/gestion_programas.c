#include "gestion_programas.h"
#include "gestion_segmentos.h"
#include <commons/string.h>
#include <commons/log.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

t_programa* buscar_programa(t_memory_context* ctx, int pid) {
    pthread_mutex_lock(&ctx->mutex_programas);
    for (int i = 0; i < list_size(ctx->programas); i++) {
        t_programa* prog = list_get(ctx->programas, i);
        if (prog->pid == pid) {
            pthread_mutex_unlock(&ctx->mutex_programas);
            return prog;
        }
    }
    pthread_mutex_unlock(&ctx->mutex_programas);
    return NULL;
}

char** leer_lineas(FILE* f, int* cantidad) {
    char** lineas = NULL;
    char buffer[256];
    int count = 0;

    while (fgets(buffer, sizeof(buffer), f)) {
        lineas = realloc(lineas, sizeof(char*) * (count + 1));
        char* linea = strdup(buffer);
        string_trim(&linea);
        lineas[count] = linea;
        count++;
    }
    *cantidad = count;
    return lineas;
}

void crear_proceso(t_memory_context* ctx, int pid, char* path) {
    FILE* f = fopen(path, "r");

    if (!f) {
        // Fallback for tests
        char fallback_path[512];
        snprintf(fallback_path, sizeof(fallback_path), "../utils/scripts/%s", path);
        f = fopen(fallback_path, "r");
        
        if (!f) {
            log_error(ctx->logger, "No se pudo abrir el archivo %s ni %s", path, fallback_path);
            return;
        }
    }

    t_programa* prog = malloc(sizeof(t_programa));
    prog->pid = pid;
    prog->instrucciones = leer_lineas(f, &prog->cantidad_instrucciones);
    fclose(f);

    prog->contexto = malloc(sizeof(t_contexto_ejecucion));
    prog->contexto->pid = pid;
    memset(&(prog->contexto->registros), 0, sizeof(t_registros));
    prog->contexto->tabla_segmentos = list_create();

    pthread_mutex_lock(&ctx->mutex_programas);
    list_add(ctx->programas, prog);
    pthread_mutex_unlock(&ctx->mutex_programas);

    // Log obligatorio del TP (strict format)
    log_info(ctx->logger, "## PID: %d - Proceso Creado", pid);
    log_info(ctx->logger, "Se leyeron %d instrucciones para el proceso %d", prog->cantidad_instrucciones, pid);
}

void destruir_programa(t_memory_context* ctx, t_programa* prog) {
    if (prog->contexto->tabla_segmentos != NULL) {
        pthread_mutex_lock(&ctx->mutex_programas);
        for (int i = 0; i < list_size(prog->contexto->tabla_segmentos); i++) {
            t_segmento* seg = list_get(prog->contexto->tabla_segmentos, i);
            eliminar_segmento(ctx, prog->pid, seg);
        }
        list_clean(prog->contexto->tabla_segmentos);
        pthread_mutex_unlock(&ctx->mutex_programas);
    }

    pthread_mutex_lock(&ctx->mutex_programas);
    for (int i = 0; i < list_size(ctx->programas); i++) {
        if (list_get(ctx->programas, i) == prog) {
            list_remove(ctx->programas, i);
            break;
        }
    }
    pthread_mutex_unlock(&ctx->mutex_programas);

    if (prog->instrucciones != NULL) {
        for (int i = 0; i < prog->cantidad_instrucciones; i++) {
            free(prog->instrucciones[i]);
        }
        free(prog->instrucciones);
    }

    if (prog->contexto != NULL) {
        if (prog->contexto->tabla_segmentos != NULL) {
            list_destroy(prog->contexto->tabla_segmentos);
        }
        free(prog->contexto);
    }

    free(prog);
}

char* obtener_instruccion(t_memory_context* ctx, int pid, int pc) {
    t_programa* prog = buscar_programa(ctx, pid);

    if (!prog || pc >= prog->cantidad_instrucciones)
        return strdup("EXIT");

    usleep(ctx->instruction_delay * 1000);

    char* instruccion = strdup(prog->instrucciones[pc]);

    log_info(ctx->logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s",
             pid, pc, instruccion);

    return instruccion;
}
