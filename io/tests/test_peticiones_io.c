#include "test_peticiones_io.h"

/*------------------------------------------------*/
/* HELPERS                                        */
/*------------------------------------------------*/

static t_io_context crear_contexto(int socket)
{
    t_io_context ctx;

    ctx.io_name = NULL;
    ctx.config = NULL;
    ctx.logger = NULL;
    ctx.socket_scheduler = socket;

    return ctx;
}

static void crear_socket(int s[2])
{
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, s) == 0);
}

static void cerrar_socket(int s[2])
{
    close(s[0]);
    close(s[1]);
}

/* helper RENOMBRADO (IMPORTANTE) */
static void recibir_mensaje_test(int socket, int *op, int *size, char *buffer)
{
    recv(socket, op, sizeof(int), MSG_WAITALL);
    recv(socket, size, sizeof(int), MSG_WAITALL);
    recv(socket, buffer, *size, MSG_WAITALL);
}

/* mock readline determinista */
char* io_readline(const char* prompt)
{
    (void)prompt;
    return strdup("hola");
}

/*------------------------------------------------*/
/* TEST SLEEP                                     */
/*------------------------------------------------*/
void test_ejecutar_sleep(void)
{
    int s[2];
    crear_socket(s);

    t_io_context ctx = crear_contexto(s[0]);
    ctx.io_name = "SLEEP";

    int pid = 1;
    int ms = 1;

    enviar_int(s[1], pid);
    enviar_int(s[1], ms);

    ejecutar_sleep(&ctx, pid);

    cerrar_socket(s);

    printf(";) test_ejecutar_sleep: PASO\n");
}

/*------------------------------------------------*/
/* TEST STDOUT                                    */
/*------------------------------------------------*/
void test_ejecutar_stdout(void)
{
    int s[2];
    crear_socket(s);

    t_io_context ctx = crear_contexto(s[0]);
    ctx.io_name = "STDOUT";

    int pid = 10;
    char texto[] = "Hola";

    enviar_int(s[1], pid);
    enviar_string(s[1], texto);

    ejecutar_stdout(&ctx, pid);

    cerrar_socket(s);

    printf(";) test_ejecutar_stdout: PASO\n");
}

/*------------------------------------------------*/
/* TEST STDIN                                     */
/*------------------------------------------------*/
void test_ejecutar_stdin(void)
{
    int s[2];
    crear_socket(s);

    t_io_context ctx = crear_contexto(s[0]);
    ctx.io_name = "STDIN";

    int pid = 1;
    int len = 5;

    enviar_int(s[1], len);

    ejecutar_stdin(&ctx, pid);

    int op, size;
    char buffer[64];

    recibir_mensaje_test(s[1], &op, &size, buffer);

    assert(op == MENSAJE);
    assert(strcmp(buffer, "hola") == 0);

    cerrar_socket(s);

    printf(";) test_ejecutar_stdin: PASO\n");
}

/*------------------------------------------------*/
/* TEST PROCESS SLEEP                             */
/*------------------------------------------------*/
void test_procesar_peticion_io_sleep(void)
{
    int s[2];
    crear_socket(s);

    t_io_context ctx = crear_contexto(s[0]);
    ctx.io_name = "SLEEP";

    int pid = 7;
    int ms = 1;

    enviar_int(s[1], pid);
    enviar_int(s[1], ms);

    procesar_peticion_io(&ctx);

    int op, size;
    char buffer[64];

    recibir_mensaje_test(s[1], &op, &size, buffer);

    assert(op == MENSAJE);
    assert(strcmp(buffer, MSG_OK) == 0);

    cerrar_socket(s);

    printf(";) test_procesar_peticion_io_sleep: PASO\n");
}

/*------------------------------------------------*/
/* TEST PROCESS STDOUT                            */
/*------------------------------------------------*/
void test_procesar_peticion_io_stdout(void)
{
    int s[2];
    crear_socket(s);

    t_io_context ctx = crear_contexto(s[0]);
    ctx.io_name = "STDOUT";

    int pid = 3;
    char texto[] = "Test";

    enviar_int(s[1], pid);
    enviar_string(s[1], texto);

    procesar_peticion_io(&ctx);

    int op, size;
    char buffer[64];

    recibir_mensaje_test(s[1], &op, &size, buffer);

    assert(op == MENSAJE);
    assert(strcmp(buffer, MSG_OK) == 0);

    cerrar_socket(s);

    printf(";) test_procesar_peticion_io_stdout: PASO\n");
}