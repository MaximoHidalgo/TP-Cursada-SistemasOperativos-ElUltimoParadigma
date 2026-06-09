#include "utils_client.h"

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

int crear_conexion(char *ip, char* puerto){
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(ip, puerto, &hints, &server_info) != 0) {
        return -1;
    }
	
	// Ahora vamos a crear el socket.
	int socket_cliente = socket(server_info->ai_family,
								server_info->ai_socktype,
								server_info->ai_protocol);

	// Ahora que tenemos el socket, vamos a conectarlo
	int error = connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);
	
	freeaddrinfo(server_info);

	if (error == -1) {
		return -1;
	}

	return socket_cliente;
}

void crear_buffer(t_paquete* paquete){
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio){
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente){
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete){
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente){
	close(socket_cliente);
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

void validar_argumentos(int argc){
    if(argc != 3){
        printf("Cantidad de argumentos incorrecta. Uso correcto: ./bin/cpu [Archivo Config] [Id]\n");
        exit(EXIT_FAILURE);
    }
}

void limpieza_programa(t_conexiones_cpu* conexiones, t_log* logger, t_config* config){
	if (logger != NULL){
		log_destroy(logger);}

	if (config != NULL){
		config_destroy(config);}

    if (conexiones != NULL) {
	    liberar_conexion(conexiones->scheduler);
        liberar_conexion(conexiones->memoria);
        liberar_conexion(conexiones->sticks);
        free(conexiones);
    }
}

void enviar_int(int socket, int numero) {
    send(socket, &numero, sizeof(int), 0);
}

void enviar_string(int socket, char* texto) {

    int size = strlen(texto) + 1;

    send(socket, &size, sizeof(int), 0);
    send(socket, texto, size, 0);
}

void serializar_contexto(t_paquete* paquete, t_contexto_ejecucion* contexto)
{
    paquete->buffer->size = sizeof(uint32_t) + sizeof(t_registros);

    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;

    memcpy(paquete->buffer->stream + offset, &(contexto->pid), sizeof(uint32_t));

    offset += sizeof(uint32_t);

    memcpy(paquete->buffer->stream + offset, &(contexto->registros), sizeof(t_registros));
}

void deserializar_contexto(void* stream, t_contexto_ejecucion* contexto)
{
    int offset = 0;

    memcpy(&(contexto->pid), stream + offset, sizeof(uint32_t));

    offset += sizeof(uint32_t);

    memcpy(&(contexto->registros), stream + offset, sizeof(t_registros));
}

void enviar_contexto(t_contexto_ejecucion* contexto, int socket_cliente)
{
    t_paquete* paquete = crear_paquete();

    paquete->codigo_operacion = ENVIO_CONTEXTO;

    serializar_contexto(paquete, contexto);
    
	enviar_paquete(paquete, socket_cliente);
    
	eliminar_paquete(paquete);
}

void enviar_peticion_crear_proceso(uint32_t pid, char* path, int socket) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = CREAR_PROCESO;
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, path, strlen(path) + 1);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

t_peticion_crear_proceso* deserializar_peticion_crear_proceso(void* buffer) {
    t_peticion_crear_proceso* peticion = malloc(sizeof(t_peticion_crear_proceso));
    int desplazamiento = 0;
    int tam_pid;
    memcpy(&tam_pid, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&(peticion->pid), buffer + desplazamiento, sizeof(uint32_t));
    desplazamiento += sizeof(int); // El tamaño real del payload de pid, que es 4
    
    int tam_path;
    memcpy(&tam_path, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    
    peticion->path = malloc(tam_path);
    memcpy(peticion->path, buffer + desplazamiento, tam_path);
    
    return peticion;
}

void destruir_peticion_crear_proceso(t_peticion_crear_proceso* peticion) {
    if (peticion) {
        if (peticion->path) free(peticion->path);
        free(peticion);
    }
}

void enviar_peticion_fetch_instruccion(uint32_t pid, uint32_t pc, int socket) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = FETCH_INSTRUCCION;
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &pc, sizeof(uint32_t));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

t_peticion_fetch_instruccion* deserializar_peticion_fetch_instruccion(void* buffer) {
    t_peticion_fetch_instruccion* peticion = malloc(sizeof(t_peticion_fetch_instruccion));
    int desplazamiento = 0;
    int tam_pid, tam_pc;
    
    memcpy(&tam_pid, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&(peticion->pid), buffer + desplazamiento, sizeof(uint32_t));
    desplazamiento += sizeof(int);
    
    memcpy(&tam_pc, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&(peticion->pc), buffer + desplazamiento, sizeof(uint32_t));
    
    return peticion;
}

void destruir_peticion_fetch_instruccion(t_peticion_fetch_instruccion* peticion) {
    if (peticion) free(peticion);
}