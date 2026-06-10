#include "estructuras.h"

void validar_argumentos(int argc) {
    // Para no matar el programa con exit, idealmente esto debería retornar false,
    // pero por retrocompatibilidad con el TP actual mantendremos el printf y exit, 
    // sabiendo que es un area de mejora si se reestructura el main().
    if(argc != 3){
        printf("Cantidad de argumentos incorrecta. Uso correcto: ./bin/modulo [Archivo Config] [Id]\n");
        exit(EXIT_FAILURE);
    }
}

void limpieza_programa(t_conexiones_cpu* conexiones, t_log* logger, t_config* config) {
	if (logger != NULL) {
		log_destroy(logger);
    }
	if (config != NULL) {
		config_destroy(config);
    }
    if (conexiones != NULL) {
	    liberar_conexion(conexiones->scheduler);
        liberar_conexion(conexiones->memoria);
        liberar_conexion(conexiones->sticks);
        free(conexiones);
    }
}

// --- CONTEXTO ---

void serializar_contexto(t_paquete* paquete, t_contexto_ejecucion* contexto) {
    buffer_write_uint32(paquete->buffer, contexto->pid);
    buffer_write(paquete->buffer, &(contexto->registros), sizeof(t_registros));
    
    // Serializar tabla de segmentos
    int cant_segmentos = contexto->tabla_segmentos != NULL ? list_size(contexto->tabla_segmentos) : 0;
    buffer_write_uint32(paquete->buffer, cant_segmentos);
    
    for (int i = 0; i < cant_segmentos; i++) {
        t_segmento* seg = list_get(contexto->tabla_segmentos, i);
        buffer_write_uint32(paquete->buffer, seg->id);
        buffer_write_uint32(paquete->buffer, seg->base);
        buffer_write_uint32(paquete->buffer, seg->tamanio);
        buffer_write_uint32(paquete->buffer, seg->bloque_swap);
    }
}

void deserializar_contexto(void* stream, int size, t_contexto_ejecucion* contexto) {
    // Usar el tamaño provisto en vez de intentar adivinarlo
    t_buffer_reader* reader = buffer_reader_create(stream, size);
    contexto->pid = buffer_read_uint32(reader);
    buffer_read(reader, &(contexto->registros), sizeof(t_registros));
    
    // Deserializar tabla de segmentos
    uint32_t cant_segmentos = buffer_read_uint32(reader);
    // Expandimos el reader para leer los segmentos (ya tiene el size correcto)
    
    contexto->tabla_segmentos = list_create();
    for (uint32_t i = 0; i < cant_segmentos; i++) {
        t_segmento* seg = malloc(sizeof(t_segmento));
        seg->id = buffer_read_uint32(reader);
        seg->base = buffer_read_uint32(reader);
        seg->tamanio = buffer_read_uint32(reader);
        seg->bloque_swap = buffer_read_uint32(reader);
        list_add(contexto->tabla_segmentos, seg);
    }
    
    buffer_reader_destroy(reader);
}

void enviar_contexto(t_contexto_ejecucion* contexto, int socket_cliente) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ENVIO_CONTEXTO;
    serializar_contexto(paquete, contexto);
	enviar_paquete(paquete, socket_cliente);
	eliminar_paquete(paquete);
}

// --- CREAR PROCESO ---

void enviar_peticion_crear_proceso(uint32_t pid, char* path, int socket) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = CREAR_PROCESO;
    buffer_write_uint32(paquete->buffer, pid);
    buffer_write_string(paquete->buffer, path);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

t_peticion_crear_proceso* deserializar_peticion_crear_proceso(void* stream) {
    // Obtenemos el stream en bruto recibido del socket. Como no sabemos el tamaño a priori, 
    // lo ideal sería recibir el tamaño total del paquete y pasarlo al reader, pero como 
    // la estructura original no lo provee, asumimos un tamaño seguro o pasamos INT_MAX.
    t_buffer_reader* reader = buffer_reader_create(stream, 1000000); 
    
    t_peticion_crear_proceso* peticion = malloc(sizeof(t_peticion_crear_proceso));
    peticion->pid = buffer_read_uint32(reader);
    peticion->path = buffer_read_string(reader);
    
    buffer_reader_destroy(reader);
    return peticion;
}

void destruir_peticion_crear_proceso(t_peticion_crear_proceso* peticion) {
    if (peticion) {
        if (peticion->path) free(peticion->path);
        free(peticion);
    }
}

// --- FETCH ---

void enviar_peticion_fetch_instruccion(uint32_t pid, uint32_t pc, int socket) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = FETCH_INSTRUCCION;
    buffer_write_uint32(paquete->buffer, pid);
    buffer_write_uint32(paquete->buffer, pc);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

t_peticion_fetch_instruccion* deserializar_peticion_fetch_instruccion(void* stream) {
    t_buffer_reader* reader = buffer_reader_create(stream, 2 * sizeof(uint32_t));
    t_peticion_fetch_instruccion* peticion = malloc(sizeof(t_peticion_fetch_instruccion));
    
    peticion->pid = buffer_read_uint32(reader);
    peticion->pc = buffer_read_uint32(reader);
    
    buffer_reader_destroy(reader);
    return peticion;
}

void destruir_peticion_fetch_instruccion(t_peticion_fetch_instruccion* peticion) {
    if (peticion) free(peticion);
}

// --- LECTURA ---

void enviar_peticion_lectura(uint32_t direccion_fisica, uint32_t tamanio, int socket) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = LEER_MEMORIA;
    buffer_write_uint32(paquete->buffer, direccion_fisica);
    buffer_write_uint32(paquete->buffer, tamanio);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

t_peticion_lectura* deserializar_peticion_lectura(void* stream) {
    t_buffer_reader* reader = buffer_reader_create(stream, 2 * sizeof(uint32_t));
    t_peticion_lectura* peticion = malloc(sizeof(t_peticion_lectura));
    
    peticion->direccion_fisica = buffer_read_uint32(reader);
    peticion->tamanio = buffer_read_uint32(reader);
    
    buffer_reader_destroy(reader);
    return peticion;
}

void destruir_peticion_lectura(t_peticion_lectura* peticion) {
    if (peticion) free(peticion);
}

// --- ESCRITURA ---

void enviar_peticion_escritura(uint32_t direccion_fisica, uint32_t tamanio, void* contenido, int socket) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ESCRIBIR_MEMORIA;
    buffer_write_uint32(paquete->buffer, direccion_fisica);
    buffer_write_uint32(paquete->buffer, tamanio);
    // Para enviar memoria binaria cruda (void*) usamos buffer_write
    buffer_write_int(paquete->buffer, tamanio); // Opcional, pero para mantener paridad en deserialización, si se envía dinámico
    buffer_write(paquete->buffer, contenido, tamanio);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

t_peticion_escritura* deserializar_peticion_escritura(void* stream) {
    t_buffer_reader* reader = buffer_reader_create(stream, 1000000); 
    t_peticion_escritura* peticion = malloc(sizeof(t_peticion_escritura));
    
    peticion->direccion_fisica = buffer_read_uint32(reader);
    peticion->tamanio = buffer_read_uint32(reader);
    
    int content_size = buffer_read_int(reader); // Si lo guardamos antes
    peticion->contenido = malloc(content_size);
    buffer_read(reader, peticion->contenido, content_size);
    
    buffer_reader_destroy(reader);
    return peticion;
}

void destruir_peticion_escritura(t_peticion_escritura* peticion) {
    if (peticion) {
        if (peticion->contenido) free(peticion->contenido);
        free(peticion);
    }
}
