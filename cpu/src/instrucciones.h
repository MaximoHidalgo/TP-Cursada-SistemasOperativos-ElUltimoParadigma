#ifndef INSTRUCCIONES_H_
#define INSTRUCCIONES_H_

#include <commons/string.h>
#include "estructuras_cpu.h"
#include "ciclo_instruccion.h"
#include "cpu_utils.h"
#include "registros.h"
#include "mmu.h"
#include <math.h>

// 1. Definimos el tipo de puntero a función:
typedef int (*t_instr_handler)(t_instruccion*, t_contexto_ejecucion*, t_cpu_context*);

// 2. Definimos el struct
typedef struct {
    char* name;
    t_instr_handler instr; // El puntero a la función que la ejecuta
} t_instruction;

// 3. Declaramos todas las funciones de ejecución
#include "syscalls.h"

t_instruction *instruction_get_by_name(char *name);
int instr_noop(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_set(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_sum(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_sub(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_jnz(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mov_in(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mov_out(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_copy_mem(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);

#endif // INSTRUCCIONES_H_