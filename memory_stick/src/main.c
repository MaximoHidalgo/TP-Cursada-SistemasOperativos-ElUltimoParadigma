#include <utils_memory_stick.h>

t_log* logger;
t_config* config;
char* puerto_escucha;
char* puerto_a_kernel_memory;
char* ip_kernel_memory;
int socket_escucha;
int socket_cpu;

int main(int argc, char* argv[]) {
    
logger = log_create("main.log","MEMORY_STICK", true, LOG_LEVEL_INFO );
iniciar_config_memory_stick();

socket_escucha= iniciar_servidor(puerto_escucha);
log_info(logger, "Esperando conexiones...");
socket_cpu = esperar_cliente(socket_escucha);









    return 0;
}
