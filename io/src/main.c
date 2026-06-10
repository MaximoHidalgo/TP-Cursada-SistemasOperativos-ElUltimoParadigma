#include "main.h"

t_log* logger;
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
    log_info(logger, "Interfaz %s lista.", IoName);

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
