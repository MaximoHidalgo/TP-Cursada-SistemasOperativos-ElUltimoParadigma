#ifndef PETICIONES_IO_H
#define PETICIONES_IO_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include <commons/string.h>
#include <readline/readline.h>

#include "estructuras_io.h"

void ejecutar_stdin(t_io_context* ctx, int pid);
void ejecutar_stdout(t_io_context* ctx, int pid);
void ejecutar_sleep(t_io_context* ctx, int pid);
void procesar_peticion_io(t_io_context* ctx);

#endif