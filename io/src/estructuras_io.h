#ifndef ESTRUCTURAS_IO_H
#define ESTRUCTURAS_IO_H

#include <commons/log.h>
#include <commons/config.h>

typedef struct {
    char* io_name;
    t_config* config;
    t_log* logger;
    int socket_scheduler;
} t_io_context;

#endif // ESTRUCTURAS_IO_H
