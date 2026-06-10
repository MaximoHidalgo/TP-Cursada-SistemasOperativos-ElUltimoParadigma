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
    estratega->socket_cpu = -1;
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

void destruir_mutex(void* element) {
    t_mutex* m = (t_mutex*)element;
    if (m) {
        free(m->nombre);
        queue_destroy(m->cola_bloqueados);
        pthread_mutex_destroy(&m->mutex_semaforo);
        free(m);
    }
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
        dictionary_destroy_and_destroy_elements(estratega->mutex_creados, (void*)destruir_mutex); 
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

// -------------------------------------------------------------
// FUNCIONES DE I/O
// -------------------------------------------------------------

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
    if (dictionary_has_key(estratega->colas_io, nombre)) {
        return dictionary_get(estratega->colas_io, nombre);
    }
    return NULL;
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
    log_info(estratega->logger, "## Proceso %d bloqueado esperando I/O en %s", pcb->pid, interfaz_nombre);

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
    
    if (interfaz != NULL) {
        pthread_mutex_lock(&interfaz->mutex_interfaz);
        if (interfaz->solicitud_actual != NULL) {
            t_proceso_planificable* liberado = interfaz->solicitud_actual->proceso;
            log_info(estratega->logger, "## Proceso %d finalizó I/O en %s", liberado->pid, interfaz->nombre);
            free(interfaz->solicitud_actual);
            interfaz->solicitud_actual = NULL;
            
            t_estado_proceso estado_anterior = liberado->estado;
            liberado->estado = ESTADO_PROCESO_READY;
            
            log_info(estratega->logger, "## (%d) Pasa del estado %s al estado %s", 
                     liberado->pid, 
                     planificador_nombre_estado(estado_anterior), 
                     planificador_nombre_estado(liberado->estado));

            encolar_en_cola_ready(estratega->colas, liberado);
            
            if (!queue_is_empty(interfaz->cola_bloqueados)) {
                interfaz->solicitud_actual = queue_pop(interfaz->cola_bloqueados);
            }
        }
        pthread_mutex_unlock(&interfaz->mutex_interfaz);
    }
    pthread_mutex_unlock(&estratega->mutex_colas);
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

void planificador_mutex_create(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso) {
    if (estratega == NULL || pcb == NULL || recurso == NULL) return;

    pthread_mutex_lock(&estratega->mutex_colas);
    if (!dictionary_has_key(estratega->mutex_creados, recurso)) {
        t_mutex* nuevo_mutex = malloc(sizeof(t_mutex));
        nuevo_mutex->nombre = string_duplicate(recurso);
        nuevo_mutex->tomado = 0;
        nuevo_mutex->pid_duenio = -1;
        nuevo_mutex->cola_bloqueados = queue_create();
        pthread_mutex_init(&nuevo_mutex->mutex_semaforo, NULL);

        dictionary_put(estratega->mutex_creados, recurso, nuevo_mutex);
        log_info(estratega->logger, "## Mutex Creado: %s", recurso);
    }
    pthread_mutex_unlock(&estratega->mutex_colas);
}

int planificador_mutex_lock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso) {
    if (estratega == NULL || pcb == NULL || recurso == NULL) return 0;

    pthread_mutex_lock(&estratega->mutex_colas);
    t_mutex* mutex = dictionary_get(estratega->mutex_creados, recurso);
    if (mutex == NULL) {
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 0; // Si no existe, no hacemos nada y sigue en EXEC
    }

    pthread_mutex_lock(&mutex->mutex_semaforo);
    if (mutex->tomado == 0) {
        // Libre: se lo damos al proceso actual
        mutex->tomado = 1;
        mutex->pid_duenio = pcb->pid;
        log_info(estratega->logger, "## Proceso %d toma el mutex %s", pcb->pid, recurso);
        pthread_mutex_unlock(&mutex->mutex_semaforo);
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 0; // Retorna 0 (no se bloqueó, sigue en EXEC)
    } else {
        // Ocupado: a la cola de bloqueados del Mutex
        pcb->estado = ESTADO_PROCESO_BLOCK;
        queue_push(mutex->cola_bloqueados, pcb);
        log_info(estratega->logger, "## Proceso %d bloqueado esperando Mutex %s", pcb->pid, recurso);
        
        // Lo sacamos de EXEC
        if (estratega->proceso_exec != NULL && estratega->proceso_exec->pid == pcb->pid) {
            estratega->proceso_exec = NULL;
        }

        pthread_mutex_unlock(&mutex->mutex_semaforo);
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 1; // Retorna 1 (se bloqueó)
    }
}

void planificador_mutex_unlock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso) {
    if (estratega == NULL || pcb == NULL || recurso == NULL) return;

    pthread_mutex_lock(&estratega->mutex_colas);
    t_mutex* mutex = dictionary_get(estratega->mutex_creados, recurso);
    if (mutex == NULL) {
        pthread_mutex_unlock(&estratega->mutex_colas);
        return;
    }

    pthread_mutex_lock(&mutex->mutex_semaforo);
    if (mutex->tomado == 1 && mutex->pid_duenio == pcb->pid) {
        log_info(estratega->logger, "## Proceso %d libera Mutex %s", pcb->pid, recurso);
        
        // Vemos si hay alguien esperando
        if (!queue_is_empty(mutex->cola_bloqueados)) {
            t_proceso_planificable* liberado = queue_pop(mutex->cola_bloqueados);
            // Le damos la tenencia del Mutex al desencolado
            mutex->pid_duenio = liberado->pid;
            
            log_info(estratega->logger, "## Proceso %d desencolado y asume Mutex %s", liberado->pid, recurso);
            
            // Lo mandamos a READY
            t_estado_proceso estado_anterior = liberado->estado;
            liberado->estado = ESTADO_PROCESO_READY;
            
            log_info(estratega->logger, "## (%d) Pasa del estado %s al estado %s", 
                     liberado->pid, 
                     planificador_nombre_estado(estado_anterior), 
                     planificador_nombre_estado(liberado->estado));

            encolar_en_cola_ready(estratega->colas, liberado);
        } else {
            // Nadie esperaba, el mutex queda libre
            mutex->tomado = 0;
            mutex->pid_duenio = -1;
        }
    }
    pthread_mutex_unlock(&mutex->mutex_semaforo);
    pthread_mutex_unlock(&estratega->mutex_colas);
}

