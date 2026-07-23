#ifndef PLANIFICADOR_MEMORIA_H
#define PLANIFICADOR_MEMORIA_H

#include "estructuras_scheduler.h"

void planificador_finalizar_todo_por_bsod(t_estratega_planificacion* estratega);
bool comparador_procesos_suspendidos(void* a, void* b);
void planificador_intentar_desuspender(t_estratega_planificacion* estratega);
int planificador_orquestar_compactacion_sincronica(t_estratega_planificacion* estratega);
void* hilo_desuspender_memoria(void* arg);
int esperar_respuesta_memoria(int socket_memoria, t_estratega_planificacion* estratega);

#endif
