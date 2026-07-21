#ifndef AUXILIARES_H
#define AUXILIARES_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include <pthread.h>
#include <string.h>
#include <commons/string.h>
#include <commons/log.h>
#include "estructuras_memory.h"
#include "procesador_mensajes.h"
#include "gestion_huecos_y_compactacion.h"
#include "gestion_segmentos.h"
#include "memoria_distribuida.h"
#include "swap_y_suspension.h"
#include "comunicacion_scheduler.h"
#include "atender_cliente_kernel_memory.h"

t_programa* buscar_programa(t_memory_context* ctx, int pid);
char** leer_lineas(FILE* f, int* cantidad);
void crear_proceso(t_memory_context* ctx, int pid, char* path);
void destruir_programa(t_memory_context* ctx, t_programa* prog);
char* obtener_instruccion(t_memory_context* ctx, int pid, int pc);

bool procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx);

t_memory_context* iniciar_contexto(int argc, char* argv[]);
void inicializar_entorno(t_memory_context* ctx, int argc, char* argv[]);
int iniciar_servidor_memoria(t_memory_context* ctx);
void iniciar_escucha_servidor(t_memory_context* ctx);
void limpiar_contexto(t_memory_context* ctx);

#endif // AUXILIARES_H