#include <utils/utils_server.h>
#include <utils/utils_client.h>

int main(int argc, char* argv[]) {
    // Iniciar Logger
    logger = log_create("KernelMemory.log", "KernelMemory", true, LOG_LEVEL_DEBUG);

    //Levantar Configuración
    t_config* config = config_create("kernelMemory.config");
    if (config == NULL) {
        log_error(logger, "No se pudo leer la configuración del scheduler");
        return EXIT_FAILURE;
    }

    char* puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");

    // 4Iniciar el Servidor del Kernel Memory
    int server_fd = iniciar_servidor(puerto_memory);
    log_info(logger, "Kernel Memory listo para recibir clientes en el puerto %s", puerto_memory);

    //Bucle principal para atender clientes
    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        
        int mensaje_cliente = recibir_operacion(cliente_fd);
        
        if (mensaje_cliente == MENSAJE) {
            char* mensaje = recibir_handshake(cliente_fd);
            
            // Verificamos quién se conectó analizando el mensaje
            if (string_starts_with(mensaje, "STICK|")) {
                // Separamos el string "STICK|1024" usando el pipe
                char** partes = string_split(mensaje, "|");
                log_info(logger, "## Memory Stick de %s bytes Conectada", partes[1]);
                string_array_destroy(partes);
            } 
            else if (string_equals_ignore_case(mensaje, "SCHEDULER")) {
                log_info(logger, "## Kernel Scheduler Conectado %d", cliente_fd);
            } 
            else if (string_starts_with(mensaje, "CPU")) {
                log_info(logger, "## CPU %s Conectada", mensaje);
            } 
            else {
                log_info(logger, "## Swap Conectado y %s recibido", mensaje);
            }
            free(mensaje); // Muy importante liberar la memoria
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