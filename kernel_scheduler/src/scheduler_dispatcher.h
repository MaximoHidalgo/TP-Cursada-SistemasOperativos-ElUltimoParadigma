#ifndef SCHEDULER_DISPATCHER_H
#define SCHEDULER_DISPATCHER_H
#include "scheduler_syscalls.h"
#include "atender_cliente_scheduler.h"

// Typedef para punteros a funciones manejadoras
typedef int (*handler_t)(int cliente_fd, t_scheduler_context* ctx);
// Creamos un array estático de tamaño 256 (suficiente para cubrir todos los opcodes).
// Los índices que no declaremos se inicializarán automáticamente en NULL.

int handler_desalojo_compactacion(int fd, t_scheduler_context* ctx);
int handler_interrupcion_desalojo(int fd, t_scheduler_context* ctx);
int handler_seg_fault(int cliente_fd, t_scheduler_context* ctx);

int handler_respuesta_compactacion(int fd, t_scheduler_context* ctx);
int handler_mem_expandida(int cliente_fd, t_scheduler_context* ctx);
int handler_mem_corrupta(int cliente_fd, t_scheduler_context* ctx);

int handler_mensaje(int fd, t_scheduler_context* ctx);
int handler_exit(int fd, t_scheduler_context* ctx);
int handler_init_proc(int fd, t_scheduler_context* ctx);
int handler_fin_quantum(int fd, t_scheduler_context* ctx);

// I/O
int handler_sleep(int fd, t_scheduler_context* ctx);
int handler_stdout(int fd, t_scheduler_context* ctx);
int handler_stdin(int fd, t_scheduler_context* ctx);

// Mutex
int handler_mutex_create(int fd, t_scheduler_context* ctx);
int handler_mutex_lock(int fd, t_scheduler_context* ctx);
int handler_mutex_unlock(int fd, t_scheduler_context* ctx);

// Memoria
int handler_mem_alloc(int fd, t_scheduler_context* ctx);
int handler_mem_free(int fd, t_scheduler_context* ctx);


static const handler_t tabla_despacho[256] = {
    [MENSAJE]                    = handler_mensaje,
    [SYSCALL_EXIT]               = handler_exit,
    [SYSCALL_SLEEP]              = handler_sleep,
    [SYSCALL_STDOUT]             = handler_stdout,
    [SYSCALL_STDIN]              = handler_stdin,
    [SYSCALL_MUTEX_CREATE]       = handler_mutex_create,
    [SYSCALL_MUTEX_LOCK]         = handler_mutex_lock,
    [SYSCALL_MUTEX_UNLOCK]       = handler_mutex_unlock,
    [SYSCALL_SEG_FAULT]          = handler_seg_fault,
    [SYSCALL_MEM_ALLOC]          = handler_mem_alloc,
    [SYSCALL_MEM_FREE]           = handler_mem_free,
    [SYSCALL_INIT_PROC]          = handler_init_proc,
    [INTERRUPCION_QUANTUM]       = handler_fin_quantum,
    [MEMORIA_EXPANDIDA]          = handler_mem_expandida, 
	[CORRUPCION_MEMORIA]         = handler_mem_corrupta,
    [RESPUESTA_COMPACTACION]     = handler_respuesta_compactacion,
    [INTERRUPCION_DESALOJO]      = handler_interrupcion_desalojo,
    [DESALOJO_COMPACTACION]      = handler_desalojo_compactacion
};

#endif