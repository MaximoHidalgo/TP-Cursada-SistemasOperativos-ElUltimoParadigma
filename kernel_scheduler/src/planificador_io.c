#define _DEFAULT_SOURCE
#include "planificador_memoria.h"

// =========================================================================
//                               ESTRUCTURAS
// =========================================================================

typedef struct {
    t_estratega_planificacion* estratega;
    t_proceso_planificable* pcb;
    int bloqueo_id;
} t_timer_args;

// =========================================================================
//                          FUNCIONES PRIVADAS
// =========================================================================

static uint64_t obtener_tiempo_actual_ms(void) {
    struct timeval tiempo_actual;
    gettimeofday(&tiempo_actual, NULL);
    return (tiempo_actual.tv_sec * 1000) + (tiempo_actual.tv_usec / 1000);
}

static void notificar_suspension_memoria(t_estratega_planificacion* estratega, uint32_t pid) {
    pthread_mutex_lock(&estratega->mutex_socket_memoria);
    enviar_operacion_con_pid(SUSPENDER_PROCESO, pid, estratega->socket_memoria);
    int resp_mem = esperar_respuesta_memoria(estratega->socket_memoria, estratega);
    if (resp_mem == MENSAJE) {
        int size_m; char* msj_m = recibir_buffer(&size_m, estratega->socket_memoria); free(msj_m);
    }
    pthread_mutex_unlock(&estratega->mutex_socket_memoria);
}

static void* suspension_timer_thread(void* arg) {
    t_timer_args* a = (t_timer_args*)arg;
    usleep(a->estratega->suspension_timeout * 1000);
    
    pthread_mutex_lock(&a->estratega->mutex_colas);
    if (a->pcb->estado == ESTADO_PROCESO_BLOCK && a->pcb->bloqueo_id == a->bloqueo_id) {
        log_info(a->estratega->logger, "## Proceso %d suspendido por timeout", a->pcb->pid);
        a->pcb->estado = ESTADO_PROCESO_SUSP_BLOCK;
        
        a->pcb->instante_suspension = obtener_tiempo_actual_ms();
        
        notificar_suspension_memoria(a->estratega, a->pcb->pid);
        
        pthread_mutex_unlock(&a->estratega->mutex_colas);
        
        planificador_admitir_a_ready(a->estratega);
        free(a);
        return NULL;
    }
    pthread_mutex_unlock(&a->estratega->mutex_colas);
    free(a);
    return NULL;
}

static void lanzar_hilo_suspension(t_estratega_planificacion* estratega, t_proceso_planificable* pcb) {
    t_timer_args* args = malloc(sizeof(t_timer_args));
    args->estratega = estratega;
    args->pcb = pcb;
    args->bloqueo_id = pcb->bloqueo_id;
    
    pthread_t timer_thread;
    pthread_create(&timer_thread, NULL, (void*)suspension_timer_thread, args);
    pthread_detach(timer_thread);
}

static void evaluar_desalojo_por_io(t_estratega_planificacion* estratega, t_proceso_planificable* liberado) {
    if (estratega->proceso_exec != NULL) {
        if (planificador_debe_desalojar(estratega, estratega->proceso_exec->cola_prioridad, liberado->cola_prioridad)) {
            log_info(estratega->logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
                     estratega->proceso_exec->pid, estratega->proceso_exec->cola_prioridad,
                     liberado->pid, liberado->cola_prioridad);
            
            int opcode_interrupcion = INTERRUPCION_DESALOJO;
            planificador_enviar_interrupcion(estratega, opcode_interrupcion);
        }
    }
}

static void reintegrar_proceso_post_io(t_estratega_planificacion* estratega, t_interfaz_io* interfaz, t_proceso_planificable* liberado) {
    log_info(estratega->logger, "## (%d) finalizó IO y pasa a %s", liberado->pid, (liberado->estado == ESTADO_PROCESO_SUSP_BLOCK) ? "SUSP. READY" : "READY");
    free(interfaz->solicitud_actual);
    interfaz->solicitud_actual = NULL;
    
    t_estado_proceso estado_anterior = liberado->estado;
    if (estado_anterior == ESTADO_PROCESO_SUSP_BLOCK) {
        liberado->estado = ESTADO_PROCESO_SUSP_READY;
    } else {
        liberado->estado = ESTADO_PROCESO_READY;
    }
    
    if (!queue_is_empty(interfaz->cola_bloqueados)) {
        interfaz->solicitud_actual = queue_pop(interfaz->cola_bloqueados);
    }

    if (liberado->estado == ESTADO_PROCESO_SUSP_READY) {
        list_add(estratega->cola_susp_ready, liberado);
        log_info(estratega->logger, "Proceso %d movido a la lista de SUSP_READY", liberado->pid);
        
        pthread_mutex_unlock(&interfaz->mutex_interfaz);
        pthread_mutex_unlock(&estratega->mutex_colas);
        
        planificador_admitir_a_ready(estratega);
    } else {
        encolar_en_cola_ready(estratega->colas, liberado);
        evaluar_desalojo_por_io(estratega, liberado);
        pthread_mutex_unlock(&interfaz->mutex_interfaz);
        pthread_mutex_unlock(&estratega->mutex_colas);
    }
}

// =========================================================================
//                          FUNCIONES PUBLICAS
// =========================================================================

t_interfaz_io* buscar_interfaz_por_socket(t_estratega_planificacion* estratega, int socket_fd) {
    t_list* interfaces = dictionary_elements(estratega->colas_io);
    for (int i = 0; i < list_size(interfaces); i++) {
        t_interfaz_io* inter = list_get(interfaces, i);
        if (inter->socket_fd == socket_fd) {
            list_destroy(interfaces);
            return inter;
        }
    }
    list_destroy(interfaces);
    return NULL;
}

t_interfaz_io* buscar_interfaz_por_nombre(t_estratega_planificacion* estratega, char* nombre) {
    return dictionary_get(estratega->colas_io, nombre);
}

void planificador_registrar_io(t_estratega_planificacion* estratega, char* nombre_interfaz, int socket_fd) {
    pthread_mutex_lock(&estratega->mutex_colas);
    t_interfaz_io* nueva_io = malloc(sizeof(t_interfaz_io));
    nueva_io->nombre = string_duplicate(nombre_interfaz);
    nueva_io->socket_fd = socket_fd;
    nueva_io->solicitud_actual = NULL;
    nueva_io->cola_bloqueados = queue_create();
    pthread_mutex_init(&nueva_io->mutex_interfaz, NULL);
    dictionary_put(estratega->colas_io, nombre_interfaz, nueva_io);
    pthread_mutex_unlock(&estratega->mutex_colas);
    log_info(estratega->logger, "## Interfaz I/O %s registrada...", nombre_interfaz);
}

bool planificador_es_io_registrado(t_estratega_planificacion* estratega, int socket_fd) {
    if (estratega == NULL) return false;
    pthread_mutex_lock(&estratega->mutex_colas);
    t_interfaz_io* interfaz = buscar_interfaz_por_socket(estratega, socket_fd);
    pthread_mutex_unlock(&estratega->mutex_colas);
    return interfaz != NULL;
}

void planificador_atender_peticion_io(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* interfaz_nombre) {
    pthread_mutex_lock(&estratega->mutex_colas);
    t_interfaz_io* interfaz = buscar_interfaz_por_nombre(estratega, interfaz_nombre);
    
    if (interfaz == NULL) {
        log_error(estratega->logger, "La interfaz %s no existe", interfaz_nombre);
        pthread_mutex_unlock(&estratega->mutex_colas);
        return;
    }
    
    pthread_mutex_lock(&interfaz->mutex_interfaz);
    
    t_solicitud_io* solicitud = malloc(sizeof(t_solicitud_io));
    solicitud->proceso = pcb;
    solicitud->param1 = NULL;
    solicitud->param2 = NULL;
    
    pcb->estado = ESTADO_PROCESO_BLOCK;
    pcb->bloqueo_id++;
    log_info(estratega->logger, "## Proceso %d bloqueado esperando I/O en %s", pcb->pid, interfaz_nombre);

    lanzar_hilo_suspension(estratega, pcb);

    if (interfaz->solicitud_actual == NULL) {
        interfaz->solicitud_actual = solicitud;
    } else {
        queue_push(interfaz->cola_bloqueados, solicitud);
    }
    
    if (estratega->proceso_exec != NULL && estratega->proceso_exec->pid == pcb->pid) {
        estratega->proceso_exec = NULL;
    }
    
    pthread_mutex_unlock(&interfaz->mutex_interfaz);
    pthread_mutex_unlock(&estratega->mutex_colas);
}

void planificador_io_terminada(t_estratega_planificacion* estratega, int socket_fd) {
    pthread_mutex_lock(&estratega->mutex_colas);
    t_interfaz_io* interfaz = buscar_interfaz_por_socket(estratega, socket_fd);
    
    if (interfaz == NULL) {
        pthread_mutex_unlock(&estratega->mutex_colas);
        return;
    }
    
    pthread_mutex_lock(&interfaz->mutex_interfaz);
    if (interfaz->solicitud_actual == NULL || interfaz->solicitud_actual->proceso == NULL) {
        pthread_mutex_unlock(&interfaz->mutex_interfaz);
        pthread_mutex_unlock(&estratega->mutex_colas);
        return;
    }

    t_proceso_planificable* liberado = interfaz->solicitud_actual->proceso;
    reintegrar_proceso_post_io(estratega, interfaz, liberado);
}