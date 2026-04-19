#include "main.h"

int main(int argc, char* argv[]) {

    validar_argumentos(argc);

    char* path_config = argv[1];
    char* id_cpu = argv[2];

    printf("Se arranco el CPU correctamente con el id: %s y el path del config: %s\n", id_cpu, path_config);

    //Creo el logger
    char* nombre_log = string_from_format("cpuNumero_%s.log", id_cpu);
    t_log* logger = log_create(nombre_log, "CPU", true, LOG_LEVEL_INFO);
    free(nombre_log);

    //Levanto el archivo config
    t_config* config = config_create(path_config);
    if (config == NULL) {
        log_error(logger, "No se pudo leer el archivo de configuracion");
        exit(EXIT_FAILURE);
    }

    char* ip_scheduler = config_get_string_value(config, "IP_SCHEDULER");
    char* puerto_scheduler = config_get_string_value(config,"PUERTO_SCHEDULER");
    char* ip_memory = config_get_string_value(config, "IP_MEMORY");
	char* puerto_memory = config_get_string_value(config,"PUERTO_MEMORY");
    char* ip_sticks= config_get_string_value(config, "IP_MEMORY_STICK");
	char* puerto_sticks = config_get_string_value(config,"PUERTO_MEMORY_STICK");

    /*log_info(logger, "IP del kernel SCHEDULER: %s", ip_scheduler);
    log_info(logger, "PUERTO del kernel SCHEDULER: %s", puerto_scheduler);
	log_info(logger, "IP del kernel MEMORY: %s", ip_memory);
    log_info(logger, "PUERTO del kernel MEMORY: %s", puerto_memory);
    log_info(logger, "IP de los Memory sticks: %s", ip_sticks);
	log_info(logger, "PUERTO de los Memory sticks: %s", puerto_sticks);
    */
    

	// Creamos una conexión hacia el servidor de kernel Memory
	log_info(logger, "Intentando conectar kernel Memory...");
	int conexion_memory = crear_conexion(ip_memory, puerto_memory);
    if (conexion_memory != -1) {
        log_info(logger, "## Conectado a Kernel Memory");
        // Enviamos la ID del CPU al servidor de kernel Memory
        enviar_mensaje(id_cpu, conexion_memory);
    }

    // Creamos una conexión hacia el servidor de kernel Scheduler
    log_info(logger, "Intentando conectar kernel Scheduler...");
	int conexion_scheduler = crear_conexion(ip_scheduler, puerto_scheduler);
	if (conexion_scheduler != -1) {
        log_info(logger, "## Conectado a Kernel Scheduler");
        // Enviamos la ID del CPU al servidor de kernel Scheduler
        enviar_mensaje(id_cpu, conexion_scheduler);
    }

    // Creamos una conexión hacia el servidor de los Memory sticks
    log_info(logger, "Intentando conectar con los Memory sticks...");
	int conexion_sticks = crear_conexion(ip_sticks, puerto_sticks);
	if (conexion_sticks != -1) {
        log_info(logger, "## Conectado a Memory Stick");
        // Enviamos la ID del CPU al servidor de los Memory sticks
        enviar_mensaje(id_cpu, conexion_sticks);
    }

    // Actualizar la llamada
    limpieza_programa(conexion_scheduler, conexion_memory, conexion_sticks, logger, config);

    printf("Termino la CPU y se cerro correctamente\n");

    return 0;
}

 void validar_argumentos(int argc){
    if(argc != 3){
        printf("Cantidad de argumentos incorrecta. Uso correcto: ./bin/cpu [Archivo Config] [Id]\n");
        exit(EXIT_FAILURE);
    }
 }

 int crear_conexion(char *ip, char* puerto){
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // Usamos IPv4
	hints.ai_socktype = SOCK_STREAM; // Protocolo TCP
	hints.ai_flags = AI_PASSIVE; // Usamos nuestra propia IP

	if (getaddrinfo(ip, puerto, &hints, &server_info) != 0) {
        return -1;
    }

	// Ahora vamos a crear el socket.
	int socket_cliente = socket(server_info->ai_family,
                                server_info->ai_socktype,
                                server_info->ai_protocol);

	// Ahora que tenemos el socket, vamos a conectarlo
	if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        freeaddrinfo(server_info);
        close(socket_cliente); // Cerramos el socket si falló la conexión
        return -1;
    }

	freeaddrinfo(server_info);

	return socket_cliente;
}

void limpieza_programa(int conexion1, int conexion2, int conexion3, t_log* logger, t_config* config){
	if (logger != NULL){
		log_destroy(logger);}

	if (config != NULL){
		config_destroy(config);}

	liberar_conexion(conexion1);
    liberar_conexion(conexion2);
    liberar_conexion(conexion3);
}

void liberar_conexion(int socket_cliente){
	close(socket_cliente);
}

void* serializar_paquete(t_paquete* paquete, int bytes){
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

void enviar_mensaje(char* mensaje, int socket_cliente){
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

void eliminar_paquete(t_paquete* paquete){
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}
