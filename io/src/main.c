#include <utils/utils_client.h>

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}


int main(int argc, char* argv[]) {
    if (argc != 3){
        printf("Error en parametros. Uso: ./io [path] [io]\n");
        return EXIT_FAILURE;
    }
    char* ConfigPath = argv[1]; //Guardamos el path en una variable
    char* IoName = argv[2]; //Guardamos si es STDIN, STOUT, SLEEP en una variable

    t_config* config = config_create(ConfigPath);
    if (config == NULL){
        printf("No se pudo leer el archivo de config del modulo IO\n");
        return EXIT_FAILURE;
    }

    //Si existe el archivo config y lo pudimos leer, guardamos la ip y puerto del kernel
    char* ipKernel = config_get_string_value(config, "IP_KERNEL");
    char* puertoKernel = config_get_string_value(config, "PUERTO_KERNEL");
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    
    //Inicializamos el log
    t_log_level nivelLog = log_level_from_string(log_level_str);
    t_log* logger = log_create("io.log", IoName, true, nivelLog);

    if (logger == NULL){
        printf("No se pudo crear el logger en modulo IO\n");
        config_destroy(config);
        return EXIT_FAILURE;
    }

    //Ahora si todo salio bien, nos conectamos al kernel
    int socketKernel = crear_conexion(ipKernel, puertoKernel);

    if(socketKernel == -1){
        log_error(logger, "No se pudo establecer conexion con el Kernel Scheduler\n");
        log_destroy(logger);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    //Si se conecto, lo avisamos
    log_info(logger, "Conectado al kernel scheduler");

    //Hacemos handshake
    enviar_mensaje(IoName, socketKernel);
    log_info(logger, "Interfaz %s lista. Presione ENTER para cerrar la conexión", IoName);

    //Limpiamos recursos
    liberar_conexion(socketKernel);
    log_destroy(logger);
    config_destroy(config);

    
    return 0;
}

