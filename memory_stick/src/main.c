#include <utils_memory_stick.h>

t_log* logger;
t_config* config;
char* puerto_escucha;
char* puerto_kernel_memory;
char* ip_kernel_memory;
int socket_escucha;
int socket_cpu;
int socket_kernel_memory;

int main(int argc, char* argv[]) {
    
logger = log_create("main.log","MEMORY_STICK", true, LOG_LEVEL_INFO );

//iniciar_config_memory_stick(config, puerto_escucha, puerto_kernel_memory, ip_kernel_memory);

//CARGO DEL CONFIG
config = config_create("memory_stick.config");

 if (config == NULL) {
        log_info(logger, "No se pudo cargar el config");
        return EXIT_FAILURE;
    }

puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

//CONECTO CON EL KERNEL MEMORY
socket_kernel_memory = crear_conexion(ip_kernel_memory, puerto_kernel_memory, logger);



if (socket_kernel_memory == -1) {
        log_info(logger, "No se pudo conectar a Kernel Memory");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
enviar_mensaje("Soy Memory Stick",socket_kernel_memory);

//RECIBO CONEXION DE CPU
socket_escucha= iniciar_servidor(puerto_escucha, logger);
log_info(logger, "Esperando conexiones...");
socket_cpu = esperar_cliente(socket_escucha, logger);

 int cod_op = recibir_operacion(socket_cpu);

    switch (cod_op) {

        case MENSAJE:
            recibir_mensaje(socket_cpu, logger);
            break;

        case -1:
            log_info(logger, "Cliente desconectado");
            close(socket_cpu);
            break;

        default:
            log_info(logger, "Operacion desconocida");
            break;
    }

    // LIMPIEZA 
    close(socket_cpu);
    close(socket_escucha);
    close(socket_kernel_memory);

    config_destroy(config);
    log_destroy(logger);









    return EXIT_SUCCESS;
}
