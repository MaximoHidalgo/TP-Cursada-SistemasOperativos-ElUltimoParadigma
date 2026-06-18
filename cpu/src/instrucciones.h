#ifndef INSTRUCCIONES_H_
#define INSTRUCCIONES_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/string.h>
#include "estructuras.h"

// 1. Definimos el tipo de puntero a función con la firma de este TP:
typedef int (*t_instr_handler)(t_instruccion*, t_contexto_ejecucion*, t_cpu_context*);

// 2. Definimos el struct (igual al de tu otro TP)
typedef struct {
    char* name;
    t_instr_handler instr; // El puntero a la función que la ejecuta
} t_instruction;

// 3. Declaramos todas las funciones de ejecución
int instr_set(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_sum(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_sub(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_jnz(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mov_in(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mov_out(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_copy_mem(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_sleep(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_stdout(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_stdin(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mutex_create(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mutex_lock(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mutex_unlock(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mem_alloc(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_mem_free(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_init_proc(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_exit(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int instr_noop(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);

// 4. Declaración de la función de búsqueda
t_instruction *instruction_get_by_name(char *name);

#endif // INSTRUCCIONES_H_