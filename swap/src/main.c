#include <utils/utils_client.h>

int main(void){
    
    t_log* logger = log_create("mainSwap.log","SWAP", true, LOG_LEVEL_INFO);

    t_config* config = config_create("swap.config");

    if (config == NULL) {
        log_error(logger, "No se pudo leer el config");
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    char* ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    int socket_kernel_memory = crear_conexion(ip_kernel_memory,puerto_kernel_memory);

    if (socket_kernel_memory == -1) {
        log_error(logger, "No se pudo conectar a Kernel Memory");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    log_info(logger, "Conectado a Kernel Memory");
    enviar_mensaje("HOLA SOY SWAP", socket_kernel_memory);
    log_info(logger, "Mensaje enviado a kernel Memory correctamente");

    //LIMPIEZA
    close(socket_kernel_memory);
    config_destroy(config);
    log_destroy(logger);

    return 0;
}
