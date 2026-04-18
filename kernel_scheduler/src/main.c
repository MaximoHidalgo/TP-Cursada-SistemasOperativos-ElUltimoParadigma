#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <utils/utils_server.h>
#include <utils/utils_client.h>

int main(int argc, char* argv[]) {
    // 1. Iniciar Logger
    logger = log_create("Scheduler.log", "Scheduler", true, LOG_LEVEL_DEBUG);

    // 2. Levantar Configuración
    t_config* config = config_create("scheduler.config");
    if (config == NULL) {
        log_error(logger, "No se pudo leer la configuración del scheduler");
        return EXIT_FAILURE;
    }

    char* ip_memory = config_get_string_value(config, "IP_MEMORY");
    char* puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");
    char* puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    // 3. Conectarse a Memoria (El Kernel actúa como cliente primero)

    int socket_memoria = crear_conexion(ip_memory, puerto_memory);
    if(socket_memoria != -1) {
        log_info(logger, "Conectado a Kernel Memory!");
    } else {
        log_error(logger, "Fallo conexion a memoria");
    }

    // 4. Iniciar el Servidor del Kernel
    int server_fd = iniciar_servidor(puerto_escucha);
    log_info(logger, "Kernel Scheduler listo para recibir clientes en el puerto %s", puerto_escucha);

    // 5. Bucle principal para atender clientes
    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        
        int mensaje_cliente = recibir_operacion(cliente_fd);
        
        if (mensaje_cliente == MENSAJE) {
            recibir_mensaje(cliente_fd);
        } 
        else if (mensaje_cliente == -1) {
            log_warning(logger, "El cliente se desconectó.");
        }
        else {
            log_warning(logger, "No entiendo el mensaje %d", mensaje_cliente);
        }
    }

    config_destroy(config);
    return 0;
}