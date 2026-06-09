#include "planificador.h"
#include "rr.h"

static t_algoritmo_planificacion algoritmo_planificacion_desde_string(char* algoritmo) {
    if (algoritmo == NULL) {
        return ALGORITMO_PLANIFICACION_INVALIDO;
    }

    if (string_equals_ignore_case(algoritmo, "CMN")) {
        return ALGORITMO_PLANIFICACION_CMN;
    }

    if (string_equals_ignore_case(algoritmo, "FIFO")) {
        return ALGORITMO_PLANIFICACION_FIFO;
    }

    if (string_equals_ignore_case(algoritmo, "RR")) {
        return ALGORITMO_PLANIFICACION_RR;
    }

    return ALGORITMO_PLANIFICACION_INVALIDO;
}

static bool config_get_bool_compatible(t_config* config, char* key) {
    char* valor = config_get_string_value(config, key);
    return valor != NULL && string_equals_ignore_case(valor, "TRUE");
}

t_estratega_planificacion* crear_estratega_planificacion(t_config* config, t_log* logger) {
    t_estratega_planificacion* estratega = calloc(1, sizeof(t_estratega_planificacion));

    estratega->algoritmo = algoritmo_planificacion_desde_string(config_get_string_value(config, "PLANIFICATION_ALGORITHM"));
    estratega->cola_new = queue_create();
    estratega->colas_io = dictionary_create();
    estratega->mutex_creados = dictionary_create();
    estratega->quantum_rr = config_get_int_value(config, "RR_QUANTUM");
    estratega->colas = crear_colas_ready_desde_config(config, estratega->quantum_rr, logger);
    estratega->proceso_exec = NULL;
    estratega->desalojo_entre_colas = config_get_bool_compatible(config, "QUEUE_PREEMPTION");
    estratega->logger = logger;
    pthread_mutex_init(&estratega->mutex_colas, NULL);

    if (estratega->algoritmo == ALGORITMO_PLANIFICACION_INVALIDO) {
        log_error(logger, "Algoritmo de planificacion invalido");
        destruir_estratega_planificacion(estratega);
        return NULL;
    }

    if (estratega->colas == NULL || list_is_empty(estratega->colas)) {
        log_error(logger, "No se configuro ninguna cola de planificacion");
        destruir_estratega_planificacion(estratega);
        return NULL;
    }

    log_info(logger, "Planificador inicializado: algoritmo=%s colas=%d desalojo=%s quantum=%d",
        planificador_nombre_algoritmo(estratega->algoritmo),
        list_size(estratega->colas),
        estratega->desalojo_entre_colas ? "TRUE" : "FALSE",
        estratega->quantum_rr);

    return estratega;
}

void destruir_estratega_planificacion(t_estratega_planificacion* estratega) {
    if (estratega == NULL) return;

    queue_destroy(estratega->cola_new);

    if (estratega->colas != NULL) {
        list_destroy_and_destroy_elements(estratega->colas, destruir_cola_planificacion);
    }
    if (estratega->colas_io != NULL) {
        dictionary_destroy_and_destroy_elements(estratega->colas_io, (void*)queue_destroy);
    }
    if (estratega->mutex_creados != NULL) {
        dictionary_destroy_and_destroy_elements(estratega->mutex_creados, (void*)queue_destroy); 
    }

    pthread_mutex_destroy(&estratega->mutex_colas);
    free(estratega);
}

bool planificador_encolar_new(t_estratega_planificacion* estratega, t_proceso_planificable* proceso) {
    if (estratega == NULL || proceso == NULL) {
        return false;
    }

    pthread_mutex_lock(&estratega->mutex_colas);
    proceso->estado = ESTADO_PROCESO_NEW;
    
    log_info(estratega->logger, "## (%d) Se crea el proceso - Estado: NEW", proceso->pid);

    queue_push(estratega->cola_new, proceso);
    pthread_mutex_unlock(&estratega->mutex_colas);
    return true;
}

t_proceso_planificable* planificador_admitir_a_ready(t_estratega_planificacion* estratega) {
    if (estratega == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&estratega->mutex_colas);

    if (queue_is_empty(estratega->cola_new)) {
        pthread_mutex_unlock(&estratega->mutex_colas);
        return NULL;
    }

    t_proceso_planificable* proceso = queue_pop(estratega->cola_new);

    if (proceso->cola_prioridad < 0 || proceso->cola_prioridad >= list_size(estratega->colas)) {
        proceso->cola_prioridad = 0;
    }

    t_estado_proceso estado_anterior = proceso->estado;
    proceso->estado = ESTADO_PROCESO_READY;

    log_info(estratega->logger, "## (%d) Pasa del estado %s al estado %s", 
             proceso->pid, 
             planificador_nombre_estado(estado_anterior), 
             planificador_nombre_estado(proceso->estado));

    encolar_en_cola_ready(estratega->colas, proceso);

    pthread_mutex_unlock(&estratega->mutex_colas);
    return proceso;
}

bool planificador_encolar(t_estratega_planificacion* estratega, t_proceso_planificable* proceso) {
    if (estratega == NULL || proceso == NULL) {
        return false;
    }

    pthread_mutex_lock(&estratega->mutex_colas);
    bool encolado = encolar_en_cola_ready(estratega->colas, proceso);

    if (!encolado) {
        pthread_mutex_unlock(&estratega->mutex_colas);
        return false;
    }

    pthread_mutex_unlock(&estratega->mutex_colas);
    return true;
}

t_proceso_planificable* planificador_obtener_siguiente(t_estratega_planificacion* estratega) {
    if (estratega == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&estratega->mutex_colas);

    t_proceso_planificable* proceso = desencolar_siguiente_ready(estratega->colas);
    pthread_mutex_unlock(&estratega->mutex_colas);
    return proceso;
}

t_proceso_planificable* planificador_despachar_siguiente(t_estratega_planificacion* estratega) {
    t_proceso_planificable* proceso = planificador_obtener_siguiente(estratega);

    if (proceso == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&estratega->mutex_colas);
    t_estado_proceso estado_anterior = proceso->estado;
    proceso->estado = ESTADO_PROCESO_EXEC;
    estratega->proceso_exec = proceso;
    
    log_info(estratega->logger, "## (%d) Pasa del estado %s al estado %s", 
             proceso->pid, 
             planificador_nombre_estado(estado_anterior), 
             planificador_nombre_estado(proceso->estado));

    pthread_mutex_unlock(&estratega->mutex_colas);

    return proceso;
}

void planificador_liberar_exec(t_estratega_planificacion* estratega, int pid) {
    if (estratega == NULL) {
        return;
    }

    pthread_mutex_lock(&estratega->mutex_colas);

    if (estratega->proceso_exec != NULL && estratega->proceso_exec->pid == pid) {
        estratega->proceso_exec = NULL;
    }

    pthread_mutex_unlock(&estratega->mutex_colas);
}

bool planificador_reencolar_por_quantum(t_estratega_planificacion* estratega, t_proceso_planificable* proceso) {
    t_cola_planificacion* cola = obtener_cola_planificacion(estratega != NULL ? estratega->colas : NULL, proceso != NULL ? proceso->cola_prioridad : -1);

    if (cola == NULL || cola->algoritmo != ALGORITMO_COLA_RR) {
        return false;
    }

    return planificador_encolar(estratega, proceso);
}

bool planificador_debe_desalojar(t_estratega_planificacion* estratega, int cola_en_ejecucion, int cola_nuevo_proceso) {
    if (estratega == NULL || !estratega->desalojo_entre_colas) {
        return false;
    }

    return cola_nuevo_proceso >= 0 && cola_nuevo_proceso < cola_en_ejecucion;
}

int planificador_quantum_para(t_estratega_planificacion* estratega, int cola_prioridad) {
    if (estratega == NULL) {
        return 0;
    }

    return quantum_para_cola(estratega->colas, cola_prioridad);
}

bool planificador_iniciar_quantum_si_corresponde(t_estratega_planificacion* estratega, t_proceso_planificable* proceso, int socket_cpu) {
    return rr_iniciar_quantum(estratega, proceso, socket_cpu);
}

const char* planificador_nombre_algoritmo(t_algoritmo_planificacion algoritmo) {
    switch (algoritmo) {
        case ALGORITMO_PLANIFICACION_CMN:
            return "CMN";
        case ALGORITMO_PLANIFICACION_FIFO:
            return "FIFO";
        case ALGORITMO_PLANIFICACION_RR:
            return "RR";
        default:
            return "INVALIDO";
    }
}

const char* planificador_nombre_estado(t_estado_proceso estado) {
    switch (estado) {
        case ESTADO_PROCESO_NEW:
            return "NEW";
        case ESTADO_PROCESO_READY:
            return "READY";
        case ESTADO_PROCESO_EXEC:
            return "EXEC";
        case ESTADO_PROCESO_BLOCK:
            return "BLOCK";
        case ESTADO_PROCESO_EXIT:
            return "EXIT";
        case ESTADO_PROCESO_SUSP_READY:
            return "SUSP_READY";
        case ESTADO_PROCESO_SUSP_BLOCK:
            return "SUSP_BLOCK";
        default:
            return "DESCONOCIDO";
    }
}