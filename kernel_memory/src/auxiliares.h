#ifndef AUXILIARES_H
#define AUXILIARES_H

#include <utils/utils_client.h>
#include <utils/utils_server.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    int pid;
    char** instrucciones;
    int cantidad_instrucciones;
    t_contexto_ejecucion* contexto; 
} t_programa;

typedef struct {
    t_list* programas;
    int instruction_delay;
    char* base_path;
    pthread_mutex_t mutex_programas;
} t_memory_context;

t_programa* buscar_programa(t_memory_context* ctx, int pid);
char** leer_lineas(FILE* f, int* cantidad);
void crear_proceso(t_memory_context* ctx, int pid, char* path);
char* obtener_instruccion(t_memory_context* ctx, int pid, int pc);

#endif // AUXILIARES_H