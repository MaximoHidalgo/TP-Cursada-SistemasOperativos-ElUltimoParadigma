#ifndef SYSCALLS_H_
#define SYSCALLS_H_

#include "estructuras_cpu.h"
#include "cpu_utils.h"
#include "cpu_memoria.h"
#include "mmu.h"
#include "registros.h"

void preparar_syscall(t_contexto_ejecucion* contexto, t_cpu_context* ctx);
int syscall_mutex_create(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_mutex_lock(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_mutex_unlock(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_mem_alloc(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_mem_free(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_sleep(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_stdout(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_stdin(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_init_proc(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);
int syscall_exit(t_instruccion* instr, t_contexto_ejecucion* ctx_ej, t_cpu_context* ctx_cpu);

#endif // SYSCALLS_H_