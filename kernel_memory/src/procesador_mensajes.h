#ifndef PROCESADOR_MENSAJES_H
#define PROCESADOR_MENSAJES_H

#include "auxiliares.h"

typedef struct {
    int cliente_fd;
    t_memory_context* ctx;
} t_args_hilo_cliente;

void procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx);
void procesar_crear_proceso_handler(int cliente_fd, t_memory_context* ctx);
void procesar_solicitud_contexto_handler(int cliente_fd, t_memory_context* ctx);
void procesar_actualizacion_contexto_handler(int cliente_fd, t_memory_context* ctx);
void procesar_fetch_instruccion_handler(int cliente_fd, t_memory_context* ctx);
void atender_cliente(void *arg);

#endif