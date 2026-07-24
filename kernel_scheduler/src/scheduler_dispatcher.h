#ifndef SCHEDULER_DISPATCHER_H
#define SCHEDULER_DISPATCHER_H
#include "scheduler_syscalls.h"
#include "utils_scheduler.h"

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

int atenderRespuesta(int cliente_fd, t_scheduler_context* ctx, int mensaje_cliente);
void atender_cliente_scheduler(void* arg);

#endif