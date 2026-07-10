#ifndef CPU_MEMORIA_H_
#define CPU_MEMORIA_H_

#include "estructuras_cpu.h"
#include <utils/dominio/estructuras.h>
#include <utils/protocolo/protocolo.h>
#include <commons/log.h>

t_contexto_ejecucion* pedir_contexto_a_memoria(int pid_recibido, t_cpu_context* ctx);
void enviar_contexto_a_memoria(t_contexto_ejecucion* contexto, int conexion_memory);

#endif // CPU_MEMORIA_H_