#include <utils_memory_stick.h>


int iniciar_servidor(char* puerto) {

    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, puerto, &hints, &servinfo);

    int socket_servidor = socket(
        servinfo->ai_family,
        servinfo->ai_socktype,
        servinfo->ai_protocol
    );

    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    listen(socket_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);

    log_info(logger, "Servidor levantado");

    return socket_servidor;
}

int esperar_cliente(int socket_servidor) {
    int cliente = accept(socket_servidor, NULL, NULL);

    log_info(logger, "cpu conectado");

    return cliente;
}

void iniciar_config_memory_stick() {

    config = config_create("memory_stick.config");
    puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

}