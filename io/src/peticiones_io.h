#ifndef PETICIONES_IO_H
#define PETICIONES_IO_H

#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>
#include <commons/string.h>
#include <readline/readline.h>

void ejecutar_stdin(int pid, int socket_scheduler, t_log* logger);
void ejecutar_stdout(int pid, int socket_scheduler, t_log* logger);
void ejecutar_sleep(int pid, int socket_scheduler, t_log* logger);
void procesar_peticion_io(int socket_scheduler, char* IoName, t_log* logger);

#endif