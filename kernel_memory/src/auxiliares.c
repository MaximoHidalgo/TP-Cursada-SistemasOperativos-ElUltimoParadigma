#include "auxiliares.h"

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
        log_error(ctx->logger, "No se pudo abrir el archivo %s", path);
        return;
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

    log_info(ctx->logger, "## PID: %d - Proceso Creado con %d instrucciones", pid, prog->cantidad_instrucciones);
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