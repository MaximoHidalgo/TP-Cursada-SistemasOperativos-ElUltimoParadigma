#ifndef COMUNICACION_SCHEDULER_H
#define COMUNICACION_SCHEDULER_H

#include "estructuras_memory.h"
#include <utils/dominio/estructuras.h>

void notificar_memoria_corrupta(int scheduler_socket);
void notificar_memoria_expandida(int scheduler_socket);
bool solicitar_compactacion_al_scheduler(int cliente_fd, t_memory_context* ctx);

#endif
