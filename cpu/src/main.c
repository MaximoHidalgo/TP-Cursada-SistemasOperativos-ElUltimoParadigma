#include "main.h"

void manejar_ejecucion_proceso(int pid_recibido, t_cpu_context* ctx) {
    log_info(ctx->logger, "## PID recibido para ejecutar: %d", pid_recibido);

    t_contexto_ejecucion* contexto = pedir_contexto_a_memoria(pid_recibido, ctx);

    if (contexto != NULL) {
        ejecutar_ciclo_de_instruccion(contexto, ctx);
        free(contexto);
    } else {
        log_error(ctx->logger, "Memoria devolvió un contexto nulo para el PID %d", pid_recibido);
    }
}

t_cpu_context* iniciar_contexto(int argc, char* argv[]) {
    validar_argumentos(argc);

    t_cpu_context* ctx = malloc(sizeof(t_cpu_context));
    if (!ctx) return NULL;

    ctx->id_cpu = strdup(argv[2]);
    char* path_config = argv[1];

    printf("Se arranco el CPU correctamente con el id: %s y el path del config: %s\n", ctx->id_cpu, path_config);

    char* nombre_log = string_from_format("%s.log", ctx->id_cpu);
    ctx->logger = log_create(nombre_log, "CPU", true, LOG_LEVEL_INFO);
    free(nombre_log);

    ctx->config = config_create(path_config);
    if (ctx->config == NULL) {
        log_error(ctx->logger, "No se pudo leer el archivo de configuracion");
        log_destroy(ctx->logger);
        free(ctx->id_cpu);
        free(ctx);
        return NULL;
    }

    char* ip_scheduler = config_get_string_value(ctx->config, "IP_SCHEDULER");
    char* puerto_scheduler = config_get_string_value(ctx->config, "PUERTO_SCHEDULER");
    char* ip_memory = config_get_string_value(ctx->config, "IP_MEMORY");
    char* puerto_memory = config_get_string_value(ctx->config, "PUERTO_MEMORY");
    
    ctx->conexiones = malloc(sizeof(t_conexiones_cpu));
    ctx->conexiones->sticks = -1;

    log_info(ctx->logger, "Intentando conectar Kernel Memory...");
    ctx->conexiones->memoria = crear_conexion(ip_memory, puerto_memory);
    ctx->memory_sticks = list_create();
    if (ctx->conexiones->memoria != -1) {
        log_info(ctx->logger, "## Conectado a Kernel Memory");
        enviar_mensaje(ctx->id_cpu, ctx->conexiones->memoria);

        int op = SOLICITUD_TAM_MAX_SEGMENTO;
        send(ctx->conexiones->memoria, &op, sizeof(int), 0);
        
        // El Kernel Memory va a responder con ENVIO_TABLA_STICKS y TAM_MAX_SEGMENTO
        int recibidos = 0;
        while (recibidos < 2) {
            int resp = recibir_operacion(ctx->conexiones->memoria);
            if (resp == TAM_MAX_SEGMENTO) {
                int size;
                void* buf = recibir_buffer(&size, ctx->conexiones->memoria);
                memcpy(&ctx->tam_max_segmento, buf, sizeof(int));
                free(buf);
                log_info(ctx->logger, "Tamaño maximo de segmento recibido: %d", ctx->tam_max_segmento);
                recibidos++;
            } else if (resp == ENVIO_TABLA_STICKS) {
                int size;
                void* buf = recibir_buffer(&size, ctx->conexiones->memoria);
                int desp = 0;
                int cant;
                desp += sizeof(int); // skip size of cant
                memcpy(&cant, buf + desp, sizeof(int)); desp += sizeof(int);
                
                for (int i = 0; i < cant; i++) {
                    t_memory_stick_info* stick = malloc(sizeof(t_memory_stick_info));
                    
                    desp += sizeof(int); // skip size
                    memcpy(&(stick->id), buf + desp, sizeof(int)); desp += sizeof(int);
                    
                    desp += sizeof(int); // skip size
                    memcpy(&(stick->tamanio), buf + desp, sizeof(uint32_t)); desp += sizeof(uint32_t);
                    
                    desp += sizeof(int); // skip size
                    memcpy(&(stick->base_address), buf + desp, sizeof(uint32_t)); desp += sizeof(uint32_t);
                    
                    desp += sizeof(int); // skip size
                    memcpy(&(stick->puerto), buf + desp, sizeof(int)); desp += sizeof(int);
                    
                    int ip_len;
                    desp += sizeof(int); // skip size
                    memcpy(&ip_len, buf + desp, sizeof(int)); desp += sizeof(int);
                    
                    desp += sizeof(int); // skip size
                    stick->ip = malloc(ip_len);
                    memcpy(stick->ip, buf + desp, ip_len); desp += ip_len;
                    
                    stick->socket_fd = crear_conexion(stick->ip, string_itoa(stick->puerto)); // TODO free itoa
                    if (stick->socket_fd != -1) {
                        enviar_mensaje(ctx->id_cpu, stick->socket_fd);
                        list_add(ctx->memory_sticks, stick);
                        log_info(ctx->logger, "Conectado a Memory Stick ID %d en %s:%d", stick->id, stick->ip, stick->puerto);
                    } else {
                        log_error(ctx->logger, "Error al conectar con Memory Stick ID %d", stick->id);
                    }
                }
                free(buf);
                recibidos++;
            } else {
                log_error(ctx->logger, "Operacion no esperada en handshake de memory: %d", resp);
                break;
            }
        }
    }

    log_info(ctx->logger, "Intentando conectar Kernel Scheduler...");
    ctx->conexiones->scheduler = crear_conexion(ip_scheduler, puerto_scheduler);
    if (ctx->conexiones->scheduler != -1) {
        log_info(ctx->logger, "## Conectado a Kernel Scheduler");
        enviar_mensaje(ctx->id_cpu, ctx->conexiones->scheduler);
    }

    return ctx;
}

void limpiar_contexto(t_cpu_context* ctx) {
    if (!ctx) return;
    
    // Si la limpieza vieja existe, la reusamos.
    if (ctx->conexiones) {
        limpieza_programa(ctx->conexiones, ctx->logger, ctx->config);
    } else {
        if (ctx->logger) log_destroy(ctx->logger);
        if (ctx->config) config_destroy(ctx->config);
    }
    
    if (ctx->id_cpu) free(ctx->id_cpu);
    free(ctx);
}

int main(int argc, char* argv[]) {
    t_cpu_context* ctx = iniciar_contexto(argc, argv);
    if (!ctx) return EXIT_FAILURE;

    while (1) {
        log_info(ctx->logger, "Esperando un nuevo PID del Kernel Scheduler...");
        int process_ID = recibir_operacion(ctx->conexiones->scheduler);
        
        switch(process_ID) {
            case PID_A_EJECUTAR:
            {
                int size;
                void* buffer = recibir_buffer(&size, ctx->conexiones->scheduler);
                
                int desplazamiento = 0;
                int size_payload;
                memcpy(&size_payload, buffer + desplazamiento, sizeof(int));
                desplazamiento += sizeof(int);
                
                int pid_recibido;
                memcpy(&pid_recibido, buffer + desplazamiento, sizeof(int));
                free(buffer);

                manejar_ejecucion_proceso(pid_recibido, ctx);
                break;
            }
            case -1:
                log_error(ctx->logger, "El Kernel Scheduler se desconectó inesperadamente.");
                printf("Termino la CPU y se cerro correctamente\n");
                limpiar_contexto(ctx);
                return EXIT_FAILURE;
            default:
                log_warning(ctx->logger, "Operación desconocida recibida del Scheduler: %d", process_ID);
                limpiar_contexto(ctx);
                return EXIT_FAILURE;
        }
    }

    limpiar_contexto(ctx);
    return 0;
}