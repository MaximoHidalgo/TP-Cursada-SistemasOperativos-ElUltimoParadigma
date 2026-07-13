#ifndef PETICIONES_IO_H
#define PETICIONES_IO_H

#include "estructuras_io.h"
#include <utils/io/io_wrapper.h>
#include <unistd.h>

void ejecutar_stdin(t_io_context* ctx, int pid);
void ejecutar_stdout(t_io_context* ctx, int pid);
void ejecutar_sleep(t_io_context* ctx, int pid);
void procesar_peticion_io(t_io_context* ctx);

#endif