#ifndef PROCESADOR_MENSAJES_H
#define PROCESADOR_MENSAJES_H

#include "gestion_huecos_y_compactacion.h"
#include "gestion_segmentos.h"
#include "memoria_distribuida.h"
#include "swap_y_suspension.h"
#include "comunicacion_scheduler.h"
#include <unistd.h>
#include <stdlib.h>

typedef struct {
    int cliente_fd;
    t_memory_context* ctx;
} t_args_hilo_cliente;

bool procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx);
void procesar_crear_proceso_handler(int cliente_fd, t_memory_context* ctx);
void procesar_solicitud_contexto_handler(int cliente_fd, t_memory_context* ctx);
void procesar_actualizacion_contexto_handler(int cliente_fd, t_memory_context* ctx);
void procesar_fetch_instruccion_handler(int cliente_fd, t_memory_context* ctx);
void procesar_crear_segmento_handler(int cliente_fd, t_memory_context* ctx);
void procesar_eliminar_segmento_handler(int cliente_fd, t_memory_context* ctx);
void procesar_consultar_espacio_libre_handler(int cliente_fd, t_memory_context* ctx);
void procesar_desconexion_cliente(int cliente_fd, t_memory_context* ctx);
void procesar_suspender_proceso(int cliente_fd, t_memory_context* ctx);
void procesar_desuspender_proceso(int cliente_fd, t_memory_context* ctx);
void procesar_liberar_proceso_handler(int cliente_fd, t_memory_context* ctx);
void procesar_solicitud_tam_max_segmento_handler(int cliente_fd, t_memory_context* ctx);
void procesar_leer_memoria_handler(int cliente_fd, t_memory_context* ctx);
void procesar_escribir_memoria_handler(int cliente_fd, t_memory_context* ctx);
void procesar_solicitud_tabla_sticks_handler(int cliente_fd, t_memory_context* ctx);
void procesar_retroceso_pc_out_of_memory_handler(int cliente_fd, t_memory_context* ctx);

#endif