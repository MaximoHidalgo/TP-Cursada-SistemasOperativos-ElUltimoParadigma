#ifndef INICIALIZACION_H
#define INICIALIZACION_H
#include "estructuras_scheduler.h"
#include "./commons/log.h"
#include "./commons/config.h"
#include "planificador.h"
#include "pcb.h"
#include "atender_cliente_scheduler.h"
#include <utils/dominio/opcodes.h>
#include <utils/dominio/estructuras.h>
#include <utils/sockets/sockets.h>
#include <pthread.h>
#include <string.h>
#include "utils_scheduler.h"

typedef struct {
    int pid;
    char *path;
    int socket_memoria;
    t_estratega_planificacion *estratega;
} t_args_creacion_proceso;


t_scheduler_context* iniciar_contexto(int, char**);

/**
 *FUNCION iniciar_contexto_scheduler()
 * inicializa la estructura del contexto del scheduler
 * parametros:
 * @param   ruta_script_inicial: ruta al script inicial que ejecutará el scheduler
 * @param   ip_servicio_memoria: IP del servicio de memoria/KM
 * @param   puerto_servicio_memoria: puerto del servicio de memoria/KM
 * @param   puerto_escucha_scheduler: puerto en el que el scheduler escucha conexiones entrantes
 * @param   socket_memoria_fd: descriptor de socket conectado al servicio de memoria/KM
 * @param   socket_escucha_fd: descriptor de socket de escucha del scheduler
 * @param   logger_preconfigurado: logger ya creado para el scheduler
 * @param   config_preconfigurada: configuración ya cargada en memoria
 * @param   estratega_planificacion: unidad de planificación ya creada
*/
t_scheduler_context* iniciar_contexto_scheduler(char*, char*, char*, char*, int, int, t_log*, t_config*, t_estratega_planificacion*);

void iniciar_planificacion(t_scheduler_context* ctx); 


void solicitar_creacion_proceso(void *);
t_proceso_planificable* crear_pcb_inicial();

void conectar_kernel_memory(t_scheduler_context* );

#endif