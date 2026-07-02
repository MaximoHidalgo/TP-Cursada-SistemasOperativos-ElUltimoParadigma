#ifndef TEST_PETICIONES_IO_H
#define TEST_PETICIONES_IO_H

#include <commons/log.h>
#include <commons/string.h>
#include <readline/readline.h>
#include <unistd.h>
#include <sys/socket.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "../src/peticiones_io.h"
#include <utils/protocolo/protocolo.h>
#include <utils/sockets/sockets.h>

/* TESTS */
void test_ejecutar_sleep(void);
void test_ejecutar_stdout(void);
void test_ejecutar_stdin(void);
void test_procesar_peticion_io_sleep(void);
void test_procesar_peticion_io_stdout(void);

#endif