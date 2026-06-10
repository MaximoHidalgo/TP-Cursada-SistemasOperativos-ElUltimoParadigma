#ifndef ESTRUCTURAS_CPU_H
#define ESTRUCTURAS_CPU_H

#include <commons/log.h>
#include <commons/config.h>
#include <utils/dominio/estructuras.h>

typedef struct {
    char* id_cpu;
    t_config* config;
    t_log* logger;
    t_conexiones_cpu* conexiones;
    int tam_max_segmento;
} t_cpu_context;

#endif // ESTRUCTURAS_CPU_H
