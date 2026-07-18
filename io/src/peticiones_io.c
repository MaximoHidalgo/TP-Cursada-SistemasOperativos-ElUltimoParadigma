#include "io_utils.h"

/*------------------------------------------------*/
/* STDIN                                         */
/*------------------------------------------------*/

void ejecutar_stdin(t_io_context* ctx, int pid) {
    int cantidad_a_leer = recibir_int(ctx->socket_scheduler);

    log_info(ctx->logger,
             "## PID: %d - Ingrese %d caracteres:",
             pid,
             cantidad_a_leer);

    char* leido = io_readline("> ");
    if (leido == NULL) {
        leido = strdup("");
    }

    char* texto_final = calloc(cantidad_a_leer + 1, sizeof(char));
    strncpy(texto_final, leido, cantidad_a_leer);

    log_info(ctx->logger, "## Texto leido: %s", texto_final);

    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = MENSAJE;
    buffer_write(paquete->buffer, texto_final, cantidad_a_leer);
    enviar_paquete(paquete, ctx->socket_scheduler);
    eliminar_paquete(paquete);

    free(leido);
    free(texto_final);
}

/*------------------------------------------------*/
/* STDOUT                                        */
/*------------------------------------------------*/

void ejecutar_stdout(t_io_context* ctx, int pid) {
    char* texto = recibir_string(ctx->socket_scheduler);

    log_info(ctx->logger,
             "## PID: %d - %s",
             pid,
             texto);

    free(texto);
    enviar_mensaje(MSG_OK, ctx->socket_scheduler);
}

/*------------------------------------------------*/
/* SLEEP                                         */
/*------------------------------------------------*/

void ejecutar_sleep(t_io_context* ctx, int pid) {
    int milisegundos = recibir_int(ctx->socket_scheduler);

    log_info(ctx->logger,
             "## PID: %d - Haciendo sleep por %d milisegundos.",
             pid,
             milisegundos);

    // Encapsulamos la lógica fea de C en una función de utilidad
    // para mantener este archivo declarativo y limpio.
    dormir_milisegundos(milisegundos);
    
    enviar_mensaje(MSG_OK, ctx->socket_scheduler);
}

/*------------------------------------------------*/
/* DISPATCH GENERAL IO                            */
/*------------------------------------------------*/

void procesar_peticion_io(t_io_context* ctx) {
    int pid = recibir_int(ctx->socket_scheduler);

    log_info(ctx->logger, "## PID: %d - Inicio de IO", pid);

    if (string_equals_ignore_case(ctx->io_name, "SLEEP")) {
        ejecutar_sleep(ctx, pid);
    } else if (string_equals_ignore_case(ctx->io_name, "STDIN")) {
        ejecutar_stdin(ctx, pid);
    } else if (string_equals_ignore_case(ctx->io_name, "STDOUT")) {
        ejecutar_stdout(ctx, pid);
    }

    log_info(ctx->logger, "## PID: %d - Fin de IO", pid);
}