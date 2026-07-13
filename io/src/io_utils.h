#ifndef IO_UTILS_H_
#define IO_UTILS_H_

#include "estructuras_io.h"
#include "peticiones_io.h"

t_io_context* iniciar_contexto(int argc, char* argv[]);
void limpiar_contexto(t_io_context* ctx);
void atender_kernel_scheduler(t_io_context* ctx);

#endif // IO_UTILS_H_
