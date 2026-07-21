#ifndef ESTRUCTURAS_MEMORY_H
#define ESTRUCTURAS_MEMORY_H

#include <stdint.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/bitarray.h>
#include <pthread.h>
#include "utils/dominio/estructuras.h"

typedef struct {
    uint32_t base;
    uint32_t limite;
} t_hueco_libre;

typedef struct {
    int pid;
    char** instrucciones;
    int cantidad_instrucciones;
    t_contexto_ejecucion* contexto; 
} t_programa;

typedef struct {
    int id;
    int socket_fd;
    uint32_t tamanio;
    uint32_t base_address;
    char* ip;
    int puerto;
} t_memory_stick_info;

typedef struct {
    t_config* config;
    t_log* logger;
    int socket_servidor;
    t_list* programas;
    int instruction_delay;
    int compaction_delay;
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

    int scheduler_socket;
    pthread_mutex_t mutex_scheduler_socket;
    
    t_list* memory_sticks;
    
    int swap_socket;
    uint32_t swap_file_size;
    uint32_t swap_block_size;
    t_bitarray* swap_bitmap;
    void* bitarray_data;
} t_memory_context;

void cleanup_memory_context(t_memory_context* ctx);

#endif // ESTRUCTURAS_MEMORY_H