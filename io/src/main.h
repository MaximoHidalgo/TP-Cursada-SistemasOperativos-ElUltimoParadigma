#ifndef IO_H
#define IO_H

#include "peticiones_io.h"

t_io_context* iniciar_contexto(int argc, char* argv[]);
void limpiar_contexto(t_io_context* ctx);

#endif //IO_H