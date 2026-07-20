#ifndef SCHEDULER_SERVER_H
#define SCHEDULER_SERVER_H

#include "estructuras_scheduler.h"
#include <pthread.h>
#include "atender_cliente_scheduler.h"
//cabezera temporal:
#include "atender_cliente_Y_scheduler_server.h"
#include "utils_scheduler.h"
//========
 

int esperar_cliente_scheduler(int, t_log* );
t_args_hilo_scheduler* crear_argumento_planificador(int,t_scheduler_context*);
void crear_hilo_atencion_cliente(pthread_t*,t_args_hilo_scheduler*, t_log*, int);
void manejar_conexion_cliente_scheduler(int,t_log*, t_scheduler_context*);
void iniciar_servidor_clientes(t_scheduler_context*);

#endif