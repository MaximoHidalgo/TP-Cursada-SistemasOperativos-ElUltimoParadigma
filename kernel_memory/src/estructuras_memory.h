#ifndef ESTRUCTURAS_MEMORY_H
#define ESTRUCTURAS_MEMORY_H

#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <pthread.h>

typedef struct {
    t_config* config;
    t_log* logger;
    int socket_servidor;
    t_list* programas;
    int instruction_delay;
    char* base_path;
    pthread_mutex_t mutex_programas;
} t_memory_context;

#endif // ESTRUCTURAS_MEMORY_H
