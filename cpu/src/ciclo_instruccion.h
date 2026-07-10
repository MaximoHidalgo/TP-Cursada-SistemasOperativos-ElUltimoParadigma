#ifndef CICLO_INSTRUCCION_H_
#define CICLO_INSTRUCCION_H_

#include <stdint.h>
#include <stdlib.h>
#include <commons/string.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include <utils/protocolo/protocolo.h>
#include "estructuras_cpu.h"
#include "cpu_memoria.h"
#include "instrucciones.h"
#include <errno.h>
#include "cpu_utils.h"


void ejecutar_ciclo_de_instruccion(t_contexto_ejecucion* contexto, t_cpu_context* ctx);
char* fetch(t_contexto_ejecucion* contexto, t_cpu_context* ctx);
t_instruccion* decode(char* instruccion_texto);
int execute(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx);
int chequear_interrupciones(t_cpu_context* ctx, uint32_t pid_actual);

#endif //CICLO_INSTRUCCION_H_