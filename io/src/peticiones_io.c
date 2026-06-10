#include "peticiones_io.h"

void ejecutar_stdin(t_io_context* ctx, int pid) {
    int cantidad_a_leer = recibir_int(ctx->socket_scheduler);
    
    log_info(ctx->logger, "## PID: %d - Ingrese %d caracteres:", pid, cantidad_a_leer);
    
    char* leido = readline("> ");
    char* texto_final = malloc(cantidad_a_leer + 1);

    int len_leido = strlen(leido);
    for (int i = 0; i < cantidad_a_leer; i++) {
        if (i < len_leido) {
            texto_final[i] = leido[i];
        } else {
            texto_final[i] = '\0';
        }
    }
    texto_final[cantidad_a_leer] = '\0';

    log_info(ctx->logger, "## Texto leido: %s", texto_final);

    // Enviar el texto leido al Kernel Scheduler
    enviar_mensaje(texto_final, ctx->socket_scheduler);

    free(leido);
    free(texto_final);
}

void ejecutar_stdout(t_io_context* ctx, int pid) {
    char* texto = recibir_string(ctx->socket_scheduler);
    
    log_info(ctx->logger, "## PID: %d - %s", pid, texto);
    
    free(texto);
}

void ejecutar_sleep(t_io_context* ctx, int pid) {
    int milisegundos = recibir_int(ctx->socket_scheduler); 

    log_info(ctx->logger, "## PID: %d - Haciendo sleep por %d milisegundos.", pid, milisegundos);
    
    usleep(milisegundos * 1000);
}

void procesar_peticion_io(t_io_context* ctx) {
    int pid = recibir_int(ctx->socket_scheduler); 
    
    log_info(ctx->logger, "## PID: %d - Inicio de IO", pid);

    if (string_equals_ignore_case(ctx->io_name, "SLEEP")) {
        ejecutar_sleep(ctx, pid);
        enviar_mensaje(MSG_OK, ctx->socket_scheduler);
    } 
    else if (string_equals_ignore_case(ctx->io_name, "STDOUT")) {
        ejecutar_stdout(ctx, pid);
        enviar_mensaje(MSG_OK, ctx->socket_scheduler);
    } 
    else if (string_equals_ignore_case(ctx->io_name, "STDIN")) {
        ejecutar_stdin(ctx, pid);
        // STDIN ya envía el texto leido en su funcion
    }

    log_info(ctx->logger, "## PID: %d - Fin de IO", pid);
}