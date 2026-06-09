#include <utils/utils_server.h>
#include <utils/utils_client.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    int pid;
    char** instrucciones;
    int cantidad_instrucciones;
    t_contexto_ejecucion* contexto; 
} t_programa;

typedef struct {
    t_list* programas;
    int instruction_delay;
    char* base_path;
    pthread_mutex_t mutex_programas;
} t_memory_context;

typedef struct {
    int cliente_fd;
    t_memory_context* ctx;
} t_args_hilo_cliente;

t_programa* buscar_programa(t_memory_context* ctx, int pid) {
    pthread_mutex_lock(&ctx->mutex_programas);
    for (int i = 0; i < list_size(ctx->programas); i++) {
        t_programa* prog = list_get(ctx->programas, i);
        if (prog->pid == pid) {
            pthread_mutex_unlock(&ctx->mutex_programas);
            return prog;
        }
    }
    pthread_mutex_unlock(&ctx->mutex_programas);
    return NULL;
}

char** leer_lineas(FILE* f, int* cantidad) {
    char** lineas = NULL;
    char buffer[256];
    int count = 0;

    while (fgets(buffer, sizeof(buffer), f)) {
        lineas = realloc(lineas, sizeof(char*) * (count + 1));
        char* linea = strdup(buffer);
        string_trim(&linea);
        lineas[count] = linea;
        count++;
    }
    *cantidad = count;
    return lineas;
}

void crear_proceso(t_memory_context* ctx, int pid, char* path) {
    FILE* f = fopen(path, "r");

    if (!f) {
        log_error(logger, "No se pudo abrir el archivo %s", path);
        return;
    }

    t_programa* prog = malloc(sizeof(t_programa));
    prog->pid = pid;
    prog->instrucciones = leer_lineas(f, &prog->cantidad_instrucciones);
    fclose(f);

    prog->contexto = malloc(sizeof(t_contexto_ejecucion));
    prog->contexto->pid = pid;
    memset(&(prog->contexto->registros), 0, sizeof(t_registros));

    pthread_mutex_lock(&ctx->mutex_programas);
    list_add(ctx->programas, prog);
    pthread_mutex_unlock(&ctx->mutex_programas);

    log_info(logger, "## PID: %d - Proceso Creado con %d instrucciones", pid, prog->cantidad_instrucciones);
}

char* obtener_instruccion(t_memory_context* ctx, int pid, int pc) {
    t_programa* prog = buscar_programa(ctx, pid);

    if (!prog || pc >= prog->cantidad_instrucciones)
        return strdup("EXIT");

    usleep(ctx->instruction_delay * 1000);

    char* instruccion = strdup(prog->instrucciones[pc]);

    log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s",
             pid, pc, instruccion);

    return instruccion;
}

// =======================================================
// HANDLERS
// =======================================================

void procesar_mensaje_handshake(int cliente_fd, t_memory_context* ctx) {
    char *mensaje = recibir_handshake(cliente_fd);
    
    if (string_starts_with(mensaje, "STICK|")) {
        char **partes = string_split(mensaje, "|");
        log_info(logger, "## Memory Stick de %s bytes Conectada", partes[1]);
        string_array_destroy(partes);
    }
    else if (string_equals_ignore_case(mensaje, HANDSHAKE_SCHEDULER)) {
        log_info(logger, "## Kernel Scheduler Conectado - FD del socket: %d", cliente_fd);
    }
    else if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        log_info(logger, "## CPU %s Conectada", mensaje);
    }
    else { 
        log_info(logger, "## Módulo Conectado: %s", mensaje);
    }
    free(mensaje);
}

void procesar_crear_proceso_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    t_peticion_crear_proceso* peticion = deserializar_peticion_crear_proceso(buffer);
    free(buffer);
    
    char *path_completo = string_new();
    string_append(&path_completo, ctx->base_path);
    string_append(&path_completo, "/");
    string_append(&path_completo, peticion->path);
    
    crear_proceso(ctx, peticion->pid, path_completo);
    enviar_mensaje(MSG_OK, cliente_fd);
    
    free(path_completo);
    destruir_peticion_crear_proceso(peticion);
}

void procesar_solicitud_contexto_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    int desplazamiento = 0;
    int tam_pid, pid;
    memcpy(&tam_pid, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(&pid, buffer + desplazamiento, sizeof(int));
    free(buffer);
    
    t_programa* prog = buscar_programa(ctx, pid);
    if (prog != NULL) {
        enviar_contexto(prog->contexto, cliente_fd);
    } else {
        log_error(logger, "Error: Contexto solicitado para PID %d inexistente", pid);
    }
}

void procesar_actualizacion_contexto_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    t_contexto_ejecucion contexto_actualizado;
    deserializar_contexto(buffer, &contexto_actualizado);
    free(buffer);
    
    t_programa* prog = buscar_programa(ctx, contexto_actualizado.pid);
    if (prog != NULL) {
        pthread_mutex_lock(&ctx->mutex_programas);
        *(prog->contexto) = contexto_actualizado;
        pthread_mutex_unlock(&ctx->mutex_programas);
        log_info(logger, "Contexto actualizado correctamente para PID %d", prog->contexto->pid);
    } else {
        log_error(logger, "Error: Intento de actualizar contexto de PID %d inexistente", contexto_actualizado.pid);
    }
}

void procesar_fetch_instruccion_handler(int cliente_fd, t_memory_context* ctx) {
    int size;
    void* buffer = recibir_buffer(&size, cliente_fd);
    
    t_peticion_fetch_instruccion* peticion = deserializar_peticion_fetch_instruccion(buffer);
    free(buffer);
    
    char *instr = obtener_instruccion(ctx, peticion->pid, peticion->pc);
    
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ENVIO_INSTRUCCION;
    agregar_a_paquete(paquete, instr, strlen(instr) + 1);
    enviar_paquete(paquete, cliente_fd);
    eliminar_paquete(paquete);
    
    free(instr);
    destruir_peticion_fetch_instruccion(peticion);
}

void atender_cliente(void *arg) {
    t_args_hilo_cliente* args = (t_args_hilo_cliente*)arg;
    int cliente_fd = args->cliente_fd;
    t_memory_context* ctx = args->ctx;
    free(args);

    while (1) {
        int cod_op = recibir_operacion(cliente_fd);

        switch(cod_op) {
            case MENSAJE:
                procesar_mensaje_handshake(cliente_fd, ctx);
                break;
            case CREAR_PROCESO:
                procesar_crear_proceso_handler(cliente_fd, ctx);
                break;
            case SOLICITUD_CONTEXTO:
                procesar_solicitud_contexto_handler(cliente_fd, ctx);
                break;
            case ACTUALIZACION_CONTEXTO:
                procesar_actualizacion_contexto_handler(cliente_fd, ctx);
                break;
            case FETCH_INSTRUCCION:
                procesar_fetch_instruccion_handler(cliente_fd, ctx);
                break;
            case LEER_MEMORIA:
            case ESCRIBIR_MEMORIA:
                enviar_mensaje(MSG_OK, cliente_fd);
                break;
            case CONSULTAR_ESPACIO_LIBRE:
            {
                int espacio_fijo_mock = 999999; 
                send(cliente_fd, &espacio_fijo_mock, sizeof(int), 0); 
                break;
            }
            case -1:
                log_warning(logger, "Cliente desconectado");
                close(cliente_fd);
                return;
            default:
                log_warning(logger, "Operación desconocida: %d", cod_op);
                close(cliente_fd);
                return;
        }
    }
}

int main(int argc, char *argv[]) {
    logger = log_create("KernelMemory.log", "KernelMemory", true, LOG_LEVEL_DEBUG);

    t_config *config = config_create("kernelMemory.config");
    if (config == NULL) {
        log_error(logger, "No se pudo leer la configuración de la memoria");
        return EXIT_FAILURE;
    }

    t_memory_context* ctx = malloc(sizeof(t_memory_context));
    ctx->programas = list_create();
    ctx->instruction_delay = config_get_int_value(config, "INSTRUCTION_DELAY");
    ctx->base_path = strdup(config_get_string_value(config, "SCRIPTS_BASEPATH"));
    pthread_mutex_init(&ctx->mutex_programas, NULL);

    char *puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");

    int server_fd = iniciar_servidor(puerto_memory);
    if(server_fd == -1) {
        return EXIT_FAILURE;
    }
    
    log_info(logger, "Kernel Memory listo para recibir clientes en el puerto %s", puerto_memory);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        if(cliente_fd == -1) continue;

        t_args_hilo_cliente* args = malloc(sizeof(t_args_hilo_cliente));
        args->cliente_fd = cliente_fd;
        args->ctx = ctx;

        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, (void*)atender_cliente, args);
        pthread_detach(hilo_cliente); 
    }
    
    // Unreachable, but for completeness
    config_destroy(config);
    return 0;
}