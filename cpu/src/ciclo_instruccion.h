#ifndef CICLO_INSTRUCCION_H_
#define CICLO_INSTRUCCION_H_

#include <utils/utils_estructuras.h>

void ejecutar_ciclo_de_instruccion(t_contexto_ejecucion* contexto, t_conexiones_cpu* conexiones, t_log* logger);
void notificar_desalojo_a_scheduler(uint32_t pid, int motivo_desalojo, int conexion_scheduler, t_log* logger);
void notificar_syscall_sleep_a_scheduler(uint32_t pid, uint32_t tiempo_sleep, int conexion_scheduler);

#endif //CICLO_INSTRUCCION_H_