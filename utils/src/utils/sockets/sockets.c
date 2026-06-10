#include "sockets.h"

// --- CLIENTE ---

int crear_conexion(char *ip, char* puerto) {
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(ip, puerto, &hints, &server_info) != 0) {
        return -1;
    }
	
	int socket_cliente = socket(server_info->ai_family,
								server_info->ai_socktype,
								server_info->ai_protocol);

	int error = connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);
	
	freeaddrinfo(server_info);

	if (error == -1) {
        if (socket_cliente != -1) close(socket_cliente);
		return -1;
	}

	return socket_cliente;
}

void liberar_conexion(int socket_cliente) {
	close(socket_cliente);
}


// --- SERVIDOR ---

int iniciar_servidor(char* puerto, t_log* logger) {
	int socket_servidor;
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	socket_servidor = socket(servinfo->ai_family,
								servinfo->ai_socktype,
								servinfo->ai_protocol);

	if (socket_servidor == -1) {
		log_error(logger, "Error al crear el socket");
		return -1;
	}

	int err = setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &(int){1},sizeof(int));
	if (err == -1) {
		log_error(logger, "Error al configurar el socket (setsockopt)");
		close(socket_servidor);
		return -1;
	}

	err = bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
	if (err == -1) {
		log_error(logger, "Error al hacer bind");
		close(socket_servidor);
		return -1;
	}

	err = listen(socket_servidor, SOMAXCONN);
	if (err == -1) {
		log_error(logger, "Error al escuchar");
		close(socket_servidor);
		return -1;
	}

	freeaddrinfo(servinfo);
	log_trace(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log* logger) {
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	if (socket_cliente == -1) {
		log_error(logger, "Error al intentar conectarse un cliente");
		close(socket_servidor);
		return -1;
	}

	log_info(logger, "Se conectó un cliente!");
	return socket_cliente;
}

// --- RECEPCION GENÉRICA ---

int recibir_operacion(int socket_cliente) {
	int cod_op;
	if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else {
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente) {
	void* buffer;
	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);
	return buffer;
}

void recibir_mensaje(int socket_cliente, t_log* logger) {
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Me llegó el mensaje %s", buffer);
	free(buffer);
}

char* recibir_handshake(int socket_cliente) {
    int size;
    recv(socket_cliente, &size, sizeof(int), MSG_WAITALL);
    
    char* buffer = malloc(size);
    recv(socket_cliente, buffer, size, MSG_WAITALL);
    
    return buffer;
}

t_list* recibir_paquete(int socket_cliente) {
	int size;
	int desplazamiento = 0;
	void* buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size) {
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento += sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer + desplazamiento, tamanio);
		desplazamiento += tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

int recibir_int(int socket) {
    int numero;
    recv(socket, &numero, sizeof(int), MSG_WAITALL);
    return numero;
}

char* recibir_string(int socket) {
    int size;
    recv(socket, &size, sizeof(int), MSG_WAITALL);
    char* buffer = malloc(size);
    recv(socket, buffer, size, MSG_WAITALL);
    return buffer;
}
