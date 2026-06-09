#include <utils/utils_client.h>
#include <utils/utils_server.h>

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

    enviar_mensaje(texto_final, socket_scheduler);

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

int main(int argc, char* argv[]) {
    if (argc != 3){
        printf("Error en parametros. Uso: ./io [path] [io]\n");
        return EXIT_FAILURE;
    }
    char* ConfigPath = argv[1];
    char* IoName = argv[2]; 

    t_config* config = config_create(ConfigPath);
    if (config == NULL){
        printf("No se pudo leer el archivo de config del modulo IO\n");
        return EXIT_FAILURE;
    }

    char* ipKernel = config_get_string_value(config, "IP_SCHEDULER");
    char* puertoKernel = config_get_string_value(config, "PUERTO_SCHEDULER");
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    
    t_log_level nivelLog = log_level_from_string(log_level_str);
    t_log* logger = log_create("io.log", IoName, true, nivelLog);

    if (logger == NULL){
        printf("No se pudo crear el logger en modulo IO\n");
        config_destroy(config);
        return EXIT_FAILURE;
    }

    int socket_scheduler = crear_conexion(ipKernel, puertoKernel);

    if(socket_scheduler == -1){
        log_error(logger, "No se pudo establecer conexion con el Kernel Scheduler\n");
        log_destroy(logger);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    log_info(logger, "## Conectado a Kernel Scheduler");
    enviar_mensaje(IoName, socket_scheduler);
    log_info(logger, "Interfaz %s lista. Presione ENTER para cerrar la conexión", IoName);

    while (1) {
        int cod_op = recibir_operacion(socket_scheduler);
        
        switch(cod_op) {
            case PETICION_IO:
                procesar_peticion_io(socket_scheduler, IoName, logger);
                break;
            case -1:
                log_error(logger, "Se desconectó el Kernel Scheduler.");
                liberar_conexion(socket_scheduler);
                log_destroy(logger);
                config_destroy(config);
                return EXIT_FAILURE;
            default:
                log_warning(logger, "Operación desconocida: %d", cod_op);
                break;
        }
    }

    liberar_conexion(socket_scheduler);
    log_destroy(logger);
    config_destroy(config);
    return 0;
}
