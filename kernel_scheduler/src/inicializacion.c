#include "inicializacion.h"

t_scheduler_context* iniciar_contexto(int argc, char* argv[]) {
    char* path_config = argv[1];
    char* ruta_script_inicial = argv[2];

    t_config* configuracion = crear_configuracion(path_config);
    t_log* logger = crear_logger_configurado(configuracion);
    t_estratega_planificacion* estratega = crear_estratega_planificacion(configuracion, logger);

    t_scheduler_context* contexto_scheduler = iniciar_contexto_scheduler(
        ruta_script_inicial, 
        config_get_string_value(configuracion, "IP_MEMORY"),
        config_get_string_value(configuracion, "PUERTO_MEMORY"),
        config_get_string_value(configuracion, "PUERTO_SCHEDULER"),
        -1,
        -1,
        logger,
        configuracion,
        estratega
    );

    return contexto_scheduler;
}
//bloques para utils scheduler
//====================================================================
t_config* crear_configuracion(char* path_configuracion){
    t_config* configuracion = config_create(path_configuracion);
    if (configuracion == NULL) {
        printf("Error: No se pudo leer la configuración en %s\n", path_configuracion);
        exit(EXIT_FAILURE);
    }
    return configuracion;
}

t_log* crear_logger_configurado(t_config* configuracion){
    char* log_level_str = config_get_string_value(configuracion,"LOG_LEVEL");
    t_log_level level = log_level_from_string(log_level_str);
    t_log* logger = log_create("Scheduler.log","Scheduler",true,level);
    return logger;
}
//====================================================================

/**
 *FUNCION iniciar_contexto_scheduler()
 * inicializa la estructura del contexto del scheduler
 * parametros:
 * @param   ruta_script_inicial: ruta al script inicial que ejecutará el scheduler
 * @param   ip_servicio_memoria: IP del servicio de memoria/KM
 * @param   puerto_servicio_memoria: puerto del servicio de memoria/KM
 * @param   puerto_escucha_scheduler: puerto en el que el scheduler escucha conexiones entrantes
 * @param   socket_memoria_fd: descriptor de socket conectado al servicio de memoria/KM
 * @param   socket_escucha_fd: descriptor de socket de escucha del scheduler
 * @param   logger_preconfigurado: logger ya creado para el scheduler
 * @param   config_preconfigurada: configuración ya cargada en memoria
 * @param   estratega_planificacion: unidad de planificación ya creada

*/
t_scheduler_context* iniciar_contexto_scheduler
    (char* ruta_script_inicial,
    char* ip_servicio_memoria,
    char* puerto_servicio_memoria,
    char* puerto_escucha_scheduler,
    int socket_memoria_fd,
    int socket_escucha_fd,
    t_log* logger_preconfigurado,
    t_config* config_preconfigurada,
    t_estratega_planificacion* estratega_planificacion
    ) {
    t_scheduler_context* ctx = malloc(sizeof(t_scheduler_context));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->path_script_inicial = ruta_script_inicial;
    ctx->ip_memory = ip_servicio_memoria;
    ctx->puerto_memory = puerto_servicio_memoria;
    ctx->puerto_escucha = puerto_escucha_scheduler;
    ctx->socket_memoria = socket_memoria_fd;
    ctx->socket_escucha = socket_escucha_fd;
    ctx->logger = logger_preconfigurado;
    ctx->config = config_preconfigurada;
    ctx->estratega = estratega_planificacion;
    
    return ctx;
}


//================================================================
//================================================================

//funcion bloque que modificare
void iniciar_planificacion(t_scheduler_context* ctx) {
    log_info(ctx->logger, "Se debe iniciar el proceso con script: %s", ctx->path_script_inicial);
    
    t_proceso_planificable* pcb_inicial = crear_pcb_inicial();
    t_args_creacion_proceso args = {
        .pid = pcb_inicial->pid, .path = ctx->path_script_inicial, .socket_memoria = ctx->socket_memoria, .estratega = ctx->estratega
    };

    funcionMutex(solicitar_creacion_proceso, &args, &ctx->estratega->mutex_socket_memoria);
    planificador_encolar_new(ctx->estratega, pcb_inicial);
    planificador_admitir_a_ready(ctx->estratega);
}


void solicitar_creacion_proceso(void *arg){
    t_args_creacion_proceso *args = arg;

    enviar_peticion_crear_proceso(args->pid, args->path, args->socket_memoria);
    int resp_mem = esperar_respuesta_memoria(args->socket_memoria, args->estratega);

    if (resp_mem == MENSAJE) {
        int size;
        char *mensaje = recibir_buffer(&size, args->socket_memoria);
        if(strcmp(mensaje,"OK")==0) log_info(args->estratega->logger,"## (0) Se crea el proceso inicial del scheduler.");
        free(mensaje);
    }
}


t_proceso_planificable* crear_pcb_inicial() {
    t_proceso_planificable* pcb = malloc(sizeof(t_proceso_planificable));
    pcb->pid = 0; 
    pcb->cola_prioridad = 0;
    pcb->prioridad_original = 0; // Prioridad máxima según el TP [1]
    pcb->bloqueo_id = 0;
    return pcb;
}



void funcionMutex(t_funcion funcion, void *arg, pthread_mutex_t* mutex)
{
    pthread_mutex_lock(mutex);

    funcion(arg);

    pthread_mutex_unlock(mutex);
}

