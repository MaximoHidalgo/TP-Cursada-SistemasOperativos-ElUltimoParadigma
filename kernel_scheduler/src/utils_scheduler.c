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
    t_log_level level = LOG_LEVEL_INFO;
    bool has_log = config_has_property(configuracion, "LOG_LEVEL");
    if(has_log){
        char* log_level_str = config_get_string_value(configuracion,"LOG_LEVEL");
        level = log_level_from_string(log_level_str);
    }
    t_log* logger = log_create("Scheduler.log","Scheduler",true,level);
    if(!has_log){
        log_warning(logger, "Falta LOG_LEVEL, asumiendo INFO");
    }
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

const char* obtener_nombre_syscall(int tipo_syscall) {
    switch (tipo_syscall) {
        case SYSCALL_SLEEP: return "SLEEP";
        case SYSCALL_STDOUT: return "STDOUT";
        case SYSCALL_STDIN: return "STDIN";
        case SYSCALL_MUTEX_CREATE: return "MUTEX_CREATE";
        case SYSCALL_MUTEX_LOCK: return "MUTEX_LOCK";
        case SYSCALL_MUTEX_UNLOCK: return "MUTEX_UNLOCK";
        case SYSCALL_MEM_ALLOC: return "MEM_ALLOC";
        case SYSCALL_MEM_FREE: return "MEM_FREE";
        default: return "UNKNOWN";
    }
} 
/* 
void procesar_mensaje_handshake(int cliente_fd, t_scheduler_context* ctx) {
    int size;
    char* mensaje = recibir_buffer(&size, cliente_fd);

    if (string_starts_with(mensaje, HANDSHAKE_CPU)) {
        log_info(ctx->logger, "## CPU %s Conectada", mensaje);
        //ctx->estratega->socket_cpu = cliente_fd;
        //despachar_proceso_a_cpu(ctx, cliente_fd);

        // 1. Agregamos el socket a la lista de CPUs
        int* nuevo_socket_cpu = malloc(sizeof(int));
        *nuevo_socket_cpu = cliente_fd;
        list_add(ctx->estratega->sockets_cpu, nuevo_socket_cpu);
        
        // Despachamos a esta CPU que se acaba de conectar
        despachar_proceso_a_cpu(ctx, cliente_fd);
    } else {
        t_interfaz_io* interfaz = buscar_interfaz_por_socket(ctx->estratega, cliente_fd);
        if (interfaz != NULL) {
            // Mensaje de una IO ya registrada
            if (string_equals_ignore_case(mensaje, "OK")) {
                planificador_io_terminada(ctx->estratega, cliente_fd);

                // 2 y 3. Verificamos la lista e intentamos despachar
                if (ctx->estratega->proceso_exec == NULL && !list_is_empty(ctx->estratega->sockets_cpu)) {
                    for(int i = 0; i < list_size(ctx->estratega->sockets_cpu); i++) {
                        int* fd_cpu = list_get(ctx->estratega->sockets_cpu, i);
                        if (ctx->estratega->proceso_exec == NULL) {
                            despachar_proceso_a_cpu(ctx, *fd_cpu);
                        }
                    }
                }
            } else {
                // Es el texto leido por STDIN
                t_proceso_planificable* proceso = NULL;
                if (interfaz->solicitud_actual != NULL) {
                    proceso = interfaz->solicitud_actual->proceso;
                }
                
                if (proceso != NULL) {
                    t_paquete* paq_write = crear_paquete();
                    paq_write->codigo_operacion = ESCRIBIR_MEMORIA;
                    agregar_a_paquete(paq_write, &(proceso->pid), sizeof(uint32_t));
                    agregar_a_paquete(paq_write, &(proceso->io_dir_fisica), sizeof(uint32_t));
                    agregar_a_paquete(paq_write, &(proceso->io_tamanio), sizeof(uint32_t));
                    agregar_a_paquete(paq_write, mensaje, proceso->io_tamanio);
                    
                    pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
                    enviar_paquete(paq_write, ctx->socket_memoria);
                    eliminar_paquete(paq_write);
                    
                    int r_mem = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
                    if (r_mem == MENSAJE) {
                        int s_m; char* msj_m = recibir_buffer(&s_m, ctx->socket_memoria); free(msj_m);
                    }
                    pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
                }
                
                planificador_io_terminada(ctx->estratega, cliente_fd);

                if (ctx->estratega->proceso_exec == NULL && !list_is_empty(ctx->estratega->sockets_cpu)) {
                    for(int i = 0; i < list_size(ctx->estratega->sockets_cpu); i++) {
                        int* fd_cpu = list_get(ctx->estratega->sockets_cpu, i);
                        if (ctx->estratega->proceso_exec == NULL) {
                            despachar_proceso_a_cpu(ctx, *fd_cpu);
                        }
                    }
                }
            }
        } else {
            // Es el nombre de una nueva interfaz registrandose
            planificador_registrar_io(ctx->estratega, mensaje, cliente_fd);
        }
    }
    free(mensaje);
} */