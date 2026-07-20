#ifndef UTILS_SCHEDULER_H
#define UTILS_SCHEDULER_H

#include <commons/log.h>
#include <commons/config.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "estructuras_scheduler.h"
#include <utils/sockets/sockets.h>

//bloques utils scheduler
//================================================================
typedef void (*t_funcion)(void *);
t_config* crear_configuracion(char* path_configuracion);
t_log* crear_logger_configurado(t_config* configuracion);
void funcionMutex(t_funcion, void *, pthread_mutex_t*);
//================================================================

//bloques para utils scheduler
//====================================================
int preparar_socket_servidor(char*,t_log*,t_scheduler_context*);
void atender_socket_invalido(char*,t_log*, t_scheduler_context*);
int preparar_servidor(char*, t_log*, t_scheduler_context*);
void limpiar_contexto(t_scheduler_context*);
//====================================================

#endif //UTILS_SCHEDULER_H