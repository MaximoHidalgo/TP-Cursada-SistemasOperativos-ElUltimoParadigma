#include <utils/utils_server.h>
#include <utils/utils_client.h>

int main(int argc, char *argv[]){

    logger = log_create("memory_stick.log", "MEMORY_STICK", true, LOG_LEVEL_INFO);
    int socket_cpu;

    if (argc != 3){
        log_error(logger, "Uso: ./bin/memory_stick [Config] [Tamaño]");
        return EXIT_FAILURE;
    }
    char *tamanio = argv[2];

    // iniciar_config_memory_stick(config, puerto_escucha, PUERTO_MEMORY, IP_MEMORY);
    // CARGAR EL CONFIG
    t_config *config = config_create("memory_stick.config");

    if (config == NULL){
        log_info(logger, "No se pudo cargar el config");
        return EXIT_FAILURE;
    }

    char *puerto_escucha = config_get_string_value(config, "PUERTO_STICK");
    char *IP_MEMORY = config_get_string_value(config, "IP_MEMORY");
    char *PUERTO_MEMORY = config_get_string_value(config, "PUERTO_MEMORY");

    // CONECTO CON EL KERNEL MEMORY
    int socket_kernel_memory = crear_conexion(IP_MEMORY, PUERTO_MEMORY);

    if (socket_kernel_memory == -1){
        log_info(logger, "No se pudo conectar a Kernel Memory");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    log_info(logger, "## Conectado a Kernel Memory");

    char *msj_handshake = string_from_format("STICK|%s", tamanio);
    enviar_mensaje(msj_handshake, socket_kernel_memory);
    free(msj_handshake);
    log_info(logger, "Mensaje enviado a kernel Memory correctamente");

    // RECIBO CONEXION DE CPU
    int socket_escucha = iniciar_servidor(puerto_escucha);
    log_info(logger, "Memory Stick listo para recibir conexiones en el puerto %s", puerto_escucha);

    // El servidor se queda abierto a que entre una conexion
    while (1){
        socket_cpu = esperar_cliente(socket_escucha);
        int cod_op = recibir_operacion(socket_cpu);

        switch (cod_op)
        {

        case MENSAJE:
            char *id_cpu = recibir_handshake(socket_cpu);
            log_info(logger, "## CPU %s Conectada", id_cpu);
            free(id_cpu);
            break;
        case -1:
            log_info(logger, "Cliente desconectado");
            close(socket_cpu);
            break;

        default:
            log_info(logger, "Operacion desconocida");
            break;
        }
    }

    // LIMPIEZA
    close(socket_cpu);
    close(socket_escucha);
    close(socket_kernel_memory);
    config_destroy(config);
    log_destroy(logger);

    return 0;
}
