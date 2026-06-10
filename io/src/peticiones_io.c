#include "peticiones_io.h"

void ejecutar_stdin(int pid, int socket_scheduler, t_log* logger) {
    int cantidad_a_leer = recibir_int(socket_scheduler);
    
    log_info(logger, "## PID: %d - Ingrese %d caracteres:", pid, cantidad_a_leer);
    
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

    // Para el Checkpoint 2, NO se requiere mandar el texto_final de vuelta al Scheduler.
    // Solo validamos que la I/O termine su espera (el MSG_OK de procesar_peticion_io).
    log_info(logger, "## Texto leido: %s", texto_final);

    free(leido);
    free(texto_final);
}

void ejecutar_stdout(int pid, int socket_scheduler, t_log* logger) {
    char* texto = recibir_string(socket_scheduler);
    
    log_info(logger, "## PID: %d - %s", pid, texto);
    
    free(texto);
}

void ejecutar_sleep(int pid, int socket_scheduler, t_log* logger) {
    int milisegundos = recibir_int(socket_scheduler); 

    log_info(logger, "## PID: %d - Haciendo sleep por %d milisegundos.", pid, milisegundos);
    
    usleep(milisegundos * 1000);
}

void procesar_peticion_io(int socket_scheduler, char* IoName, t_log* logger) {
    int pid = recibir_int(socket_scheduler); 
    
    log_info(logger, "## PID: %d - Inicio de IO", pid);

    if (string_equals_ignore_case(IoName, "SLEEP")) {
        ejecutar_sleep(pid, socket_scheduler, logger);
    } 
    else if (string_equals_ignore_case(IoName, "STDOUT")) {
        ejecutar_stdout(pid, socket_scheduler, logger);
    } 
    else if (string_equals_ignore_case(IoName, "STDIN")) {
        ejecutar_stdin(pid, socket_scheduler, logger);
    }

    log_info(logger, "## PID: %d - Fin de IO", pid);
    
    enviar_mensaje(MSG_OK, socket_scheduler);
}