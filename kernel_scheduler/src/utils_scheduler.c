#include "utils_scheduler.h"
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

void funcionMutex(t_funcion funcion, void *arg, pthread_mutex_t* mutex)
{
    pthread_mutex_lock(mutex);

    funcion(arg);

    pthread_mutex_unlock(mutex);
}
//====================================================================

 
int preparar_socket_servidor(char*puerto,t_log*logger,t_scheduler_context* ctx){
    int socket_escucha = iniciar_servidor(puerto,logger);
    log_info(logger, "Kernel Scheduler listo para recibir clientes en el puerto %s", ctx->puerto_escucha);
    return socket_escucha;
}

void atender_socket_invalido(char* puerto,t_log* logger, t_scheduler_context* ctx){
    log_error(logger, "No se pudo iniciar el servidor en el puerto %s", puerto);
    limpiar_contexto(ctx);
    exit(EXIT_FAILURE);
}

int preparar_servidor(char* puerto, t_log* logger, t_scheduler_context* ctx){
    int socket_servidor = preparar_socket_servidor(puerto,logger,ctx);
    if(socket_servidor==-1) atender_socket_invalido(puerto,logger,ctx);
    return socket_servidor;
}

void limpiar_contexto(t_scheduler_context* ctx) {
    if (ctx->socket_escucha != -1) close(ctx->socket_escucha);
    if (ctx->socket_memoria != -1) close(ctx->socket_memoria);

    if (ctx->estratega != NULL) destruir_estratega_planificacion(ctx->estratega);
    if (ctx->config != NULL) config_destroy(ctx->config);
    if (ctx->logger != NULL) log_destroy(ctx->logger);

    free(ctx);
}