#ifndef MAIN_H_
#define MAIN_H_

#include "estructuras_scheduler.h"
#include "atender_cliente_scheduler.h"

// Inicialización y Orquestación
t_scheduler_context* iniciar_contexto(int argc, char* argv[]);
void* atender_mensajes_memoria(void* arg);
void conectar_kernel_memory(t_scheduler_context* ctx);
void iniciar_planificacion(t_scheduler_context* ctx);
void iniciar_servidor_clientes(t_scheduler_context* ctx);
//void limpiar_contexto(t_scheduler_context* ctx);

#endif /* MAIN_H_ */