#ifndef ESTRUCTURAS_MEMORY_H
#define ESTRUCTURAS_MEMORY_H

#include <stdint.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <pthread.h>

typedef struct {
    uint32_t base;
    uint32_t limite;
} t_hueco_libre;

typedef struct {
    t_config* config;
    t_log* logger;
    int socket_servidor;
    t_list* programas;
    int instruction_delay;
    char* base_path;
    pthread_mutex_t mutex_programas;
    
    // Segmentación
    void* espacio_memoria;
    uint32_t memoria_total;
    int segment_max_size;
    char* allocation_strategy;
    t_list* huecos_libres;
    t_list* segmentos_globales;
    pthread_mutex_t mutex_memoria;
} t_memory_context;

#endif // ESTRUCTURAS_MEMORY_H
