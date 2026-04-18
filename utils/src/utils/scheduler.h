#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <commons/collections/list.h>
#include "proceso.h"

typedef enum
{
	MENSAJE,
	PAQUETE
}op_code;

/*
// Colas
extern t_list* cola_new;
extern t_list* cola_ready;
extern t_list* cola_exec;
extern t_list* cola_block;

// Funciones
void iniciar_colas();
void pasar_a_ready(proceso_t* p);
proceso_t* obtener_siguiente();
void planificar();
void* servidor_cpu(void* arg);
void* conexion_memory(void* arg);
void ejecutar_proceso(proceso_t* p);
*/

#endif