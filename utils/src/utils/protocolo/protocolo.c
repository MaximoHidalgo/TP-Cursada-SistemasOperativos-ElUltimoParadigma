#include "protocolo.h"

// --- MANEJO DE PAQUETES ---

void crear_buffer(t_paquete* paquete) {
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void) {
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

void eliminar_paquete(t_paquete* paquete) {
    if (paquete != NULL) {
        if (paquete->buffer != NULL) {
            if (paquete->buffer->stream != NULL) {
                free(paquete->buffer->stream);
            }
            free(paquete->buffer);
        }
        free(paquete);
    }
}

void* serializar_paquete(t_paquete* paquete, int bytes) {
	void* magic = malloc(bytes);
	int offset = 0;

	memcpy(magic + offset, &(paquete->codigo_operacion), sizeof(int));
	offset += sizeof(int);
	
	memcpy(magic + offset, &(paquete->buffer->size), sizeof(int));
	offset += sizeof(int);
	
    if (paquete->buffer->size > 0 && paquete->buffer->stream != NULL) {
	    memcpy(magic + offset, paquete->buffer->stream, paquete->buffer->size);
    }

	return magic;
}

void enviar_paquete(t_paquete* paquete, int socket_cliente) {
	int bytes = paquete->buffer->size + 2 * sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio) {
    buffer_write_int(paquete->buffer, tamanio);
    buffer_write(paquete->buffer, valor, tamanio);
}

void enviar_mensaje(char* mensaje, int socket_cliente) {
	t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = MENSAJE;
    buffer_write(paquete->buffer, mensaje, strlen(mensaje) + 1);
	enviar_paquete(paquete, socket_cliente);
	eliminar_paquete(paquete);
}

void enviar_int(int socket, int numero) {
    send(socket, &numero, sizeof(int), 0);
}

void enviar_string(int socket, char* texto) {
    int size = strlen(texto) + 1;
    send(socket, &size, sizeof(int), 0);
    send(socket, texto, size, 0);
}

void enviar_operacion_con_pid(op_code codigo, uint32_t pid, int socket_cliente) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = codigo;
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    enviar_paquete(paquete, socket_cliente);
    eliminar_paquete(paquete);
}

void enviar_operacion_con_pid_y_parametro(op_code codigo, uint32_t pid, uint32_t parametro, int socket_cliente) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = codigo;
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &parametro, sizeof(uint32_t));
    enviar_paquete(paquete, socket_cliente);
    eliminar_paquete(paquete);
}

// --- STREAM ABSTRACTION (WRITE) ---

void buffer_write(t_buffer* buffer, void* data, int size) {
	buffer->stream = realloc(buffer->stream, buffer->size + size);
	memcpy(buffer->stream + buffer->size, data, size);
	buffer->size += size;
}

void buffer_write_uint32(t_buffer* buffer, uint32_t data) {
    buffer_write(buffer, &data, sizeof(uint32_t));
}

void buffer_write_int(t_buffer* buffer, int data) {
    buffer_write(buffer, &data, sizeof(int));
}

void buffer_write_string(t_buffer* buffer, char* data) {
    int size = strlen(data) + 1;
    buffer_write_int(buffer, size); // Guardamos la longitud primero
    buffer_write(buffer, data, size);
}

// --- STREAM ABSTRACTION (READ) ---

t_buffer_reader* buffer_reader_create(void* stream, int size) {
    t_buffer_reader* reader = malloc(sizeof(t_buffer_reader));
    reader->stream = stream;
    reader->size = size;
    reader->offset = 0;
    return reader;
}

void buffer_reader_destroy(t_buffer_reader* reader) {
    if (reader) free(reader);
}

void buffer_read(t_buffer_reader* reader, void* dest, int size) {
    if (reader->offset + size > reader->size) {
        // Fallback or error handling for over-reading buffer (could add logging or assertions)
        return; 
    }
    memcpy(dest, reader->stream + reader->offset, size);
    reader->offset += size;
}

uint32_t buffer_read_uint32(t_buffer_reader* reader) {
    uint32_t val = 0;
    buffer_read(reader, &val, sizeof(uint32_t));
    return val;
}

int buffer_read_int(t_buffer_reader* reader) {
    int val = 0;
    buffer_read(reader, &val, sizeof(int));
    return val;
}

char* buffer_read_string(t_buffer_reader* reader) {
    int len = buffer_read_int(reader);
    if (len <= 0) return NULL;
    
    char* str = malloc(len);
    buffer_read(reader, str, len);
    return str;
}
