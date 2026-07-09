#ifndef ESTRUCTURAS_CPU_H
#define ESTRUCTURAS_CPU_H

#include <commons/log.h>
#include <commons/config.h>
#include <utils/dominio/estructuras.h>

#include <commons/collections/list.h>

typedef struct {
    int id;
    int socket_fd;
    uint32_t tamanio;
    uint32_t base_address;
    char* ip;
    int puerto;
} t_memory_stick_info;

typedef struct {
    char* id_cpu;
    t_config* config;
    t_log* logger;
    t_conexiones_cpu* conexiones;
    int tam_max_segmento;
    t_list* memory_sticks;
} t_cpu_context;

typedef struct {
    char* identificador;
    char* parametro1;//Ej: "AX", "MUTEX_1", "25000"
    char* parametro2;//Ej: "6", "BX" 
    char* parametro3;//Por las dudas
} t_instruccion;

#endif // ESTRUCTURAS_CPU_H
