#ifndef ESTRUCTURAS_SCHEDULER_H_
#define ESTRUCTURAS_SCHEDULER_H_

#include "utils/dominio/estructuras.h"
#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <commons/log.h>
#include <commons/config.h>
#include "planificador.h"

typedef struct {
    char* path_script_inicial;
    char* ip_memory;
    char* puerto_memory;
    char* puerto_escucha;
    int socket_memoria;
    int socket_escucha;
    t_log* logger;
    t_config* config;
    t_estratega_planificacion* estratega;
} t_scheduler_context;

typedef struct {
    int cliente_fd;
    t_scheduler_context* ctx;
} t_args_hilo_scheduler;

#endif // ESTRUCTURAS_SCHEDULER_H_
