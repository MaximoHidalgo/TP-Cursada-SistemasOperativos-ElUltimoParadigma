#ifndef ESTRUCTURAS_IO_H
#define ESTRUCTURAS_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>

typedef struct {
    char* io_name;
    t_config* config;
    t_log* logger;
    int socket_scheduler;
} t_io_context;

#endif // ESTRUCTURAS_IO_H