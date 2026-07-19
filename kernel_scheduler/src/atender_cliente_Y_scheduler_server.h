#ifndef ATENDER_CLIENTE_Y_SCHEDULER_SERVER_H
#define ATENDER_CLIENTE_Y_SCHEDULER_SERVER_H

#include "estructuras_scheduler.h"

typedef struct {
    int cliente_fd;
    t_scheduler_context* ctx;
} t_args_hilo_scheduler;

#endif
