#ifndef ESTRUCTURAS_STICK_H_
#define ESTRUCTURAS_STICK_H_

#include <commons/log.h>
#include <commons/config.h>
#include <pthread.h>
#include <stdint.h>

typedef struct {
    void* espacio_memoria;
    uint32_t tamanio_total;
    char* puerto_escucha;
    char* ip_memory;
    char* puerto_memory;
    t_log* logger;
    t_config* config;
    int socket_kernel_memory;
    int socket_escucha;
    pthread_mutex_t mutex_memoria;
} t_memory_stick_context;

#endif // ESTRUCTURAS_STICK_H_
