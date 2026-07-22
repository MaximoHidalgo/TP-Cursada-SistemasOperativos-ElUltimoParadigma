#ifndef PLANIFICADOR_IO_H
#define PLANIFICADOR_IO_H

#include "planificador_io.h"
#include "planificador.h"
#include <unistd.h>
#include <sys/time.h>
#include "estructuras_scheduler.h"

t_interfaz_io* buscar_interfaz_por_socket(t_estratega_planificacion* estratega, int socket_fd);
t_interfaz_io* buscar_interfaz_por_nombre(t_estratega_planificacion* estratega, char* nombre);
void planificador_registrar_io(t_estratega_planificacion* estratega, char* nombre_interfaz, int socket_fd);
bool planificador_es_io_registrado(t_estratega_planificacion* estratega, int socket_fd);
void planificador_atender_peticion_io(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* interfaz_nombre);
void planificador_io_terminada(t_estratega_planificacion* estratega, int socket_fd);

#endif