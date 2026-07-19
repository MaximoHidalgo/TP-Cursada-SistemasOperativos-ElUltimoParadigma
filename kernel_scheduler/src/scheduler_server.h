#ifndef SCHEDULER_SERVER_H
#define SCHEDULER_SERVER_H

#include "estructuras_scheduler.h"
#include <pthread.h>
#include "atender_cliente_scheduler.h"
//cabezera temporal:
#include "atender_cliente_Y_scheduler_server.h"
//========
 
//bloques para utils scheduler
//====================================================
int preparar_servidor(char*,t_log*,t_scheduler_context*);
//====================================================
int esperar_cliente_scheduler(int, t_log* );
t_args_hilo_scheduler* crear_argumento_planificador(int,t_scheduler_context*);
void iniciar_servidor_clientes(t_scheduler_context*);

#endif