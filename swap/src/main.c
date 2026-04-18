#include <utils_swap.h>

t_log* logger;
t_config* config;
char* puerto_kernel_memory;
char* ip_kernel_memory;
int socket_kernel_memory;

int main(int argc, char* argv[]) {
    
logger = log_create("mainSwap.log","SWAP", true, LOG_LEVEL_INFO );

config = config_create("swap.config");

if (config == NULL) {
        log_error(logger, "No se pudo leer el config");
        log_destroy(logger);
        return EXIT_FAILURE;
    }



ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

socket_kernel_memory = crear_conexion(ip_kernel_memory,puerto_kernel_memory,logger);

if (socket_kernel_memory == -1) {
        log_error(logger, "No se pudo conectar a Memory Stick");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

enviar_mensaje("HOLA SOY SWAP", socket_kernel_memory);

    log_info(logger, "Handshake enviado");

    // 🔹 LIMPIEZA
    close(socket_kernel_memory);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;

    return 0;
}
