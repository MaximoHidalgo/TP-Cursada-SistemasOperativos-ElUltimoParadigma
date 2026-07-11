#ifndef CPU_H
#define CPU_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/opcodes.h>
#include "estructuras_cpu.h"
#include "ciclo_instruccion.h"
#include "notificaciones_scheduler.h"

void manejar_ejecucion_proceso(int pid_recibido, t_cpu_context* ctx);
int desalojar_proceso(t_contexto_ejecucion* contexto, t_cpu_context* ctx, op_code motivo);
void procesar_tabla_de_sticks(t_cpu_context* ctx, void* buffer, int size);
void actualizar_tabla_sticks(t_cpu_context* ctx);

#endif //CPU_H