#define _DEFAULT_SOURCE //para evitar el warning de usleep
#include <commons/string.h>
#include <unistd.h>
#include <sys/time.h>
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

void planificador_enviar_interrupcion(t_estratega_planificacion* estratega, int opcode_interrupcion) {
    if (estratega->sockets_cpu == NULL) return;
    for (int i = 0; i < list_size(estratega->sockets_cpu); i++) {
        int* fd_cpu = list_get(estratega->sockets_cpu, i);
        if (fd_cpu != NULL && *fd_cpu != -1) {
            send(*fd_cpu, &opcode_interrupcion, sizeof(int), 0);
        }
    }
}

t_estratega_planificacion* crear_estratega_planificacion(t_config* config, t_log* logger) {
    t_estratega_planificacion* estratega = calloc(1, sizeof(t_estratega_planificacion));

    estratega->algoritmo = algoritmo_planificacion_desde_string(config_get_string_value(config, "PLANIFICATION_ALGORITHM"));
    estratega->cola_new = queue_create();
    estratega->cola_susp_ready = list_create(); //agregado para la lista_suspendidos
    estratega->colas_io = dictionary_create();
    estratega->mutex_creados = dictionary_create();
    estratega->quantum_rr = config_get_int_value(config, "RR_QUANTUM");
    
    if (config_has_property(config, "SUSPENSION_TIMEOUT")) {
        estratega->suspension_timeout = config_get_int_value(config, "SUSPENSION_TIMEOUT");
    } else {
        estratega->suspension_timeout = 35000;
    }

    estratega->colas = crear_colas_ready_desde_config(config, estratega->quantum_rr, logger);
    estratega->proceso_exec = NULL;
    estratega->desalojo_entre_colas = config_get_bool_compatible(config, "QUEUE_PREEMPTION");
    estratega->logger = logger;
    //estratega->socket_cpu = -1;
    estratega->sockets_cpu = list_create(); //inicializada para contener varias cpus
    pthread_mutex_init(&estratega->mutex_colas, NULL);
    pthread_mutex_init(&estratega->mutex_socket_memoria, NULL);

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
    list_destroy(estratega->cola_susp_ready); //elimina atributo creado para los suspendidos

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
    pthread_mutex_destroy(&estratega->mutex_socket_memoria);
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

    while (!list_is_empty(estratega->cola_susp_ready) || !queue_is_empty(estratega->cola_new)) {
        t_proceso_planificable* proceso = NULL;
        
        if (!list_is_empty(estratega->cola_susp_ready)) {
            proceso = list_remove(estratega->cola_susp_ready, 0);
        } else {
            proceso = queue_pop(estratega->cola_new);
        }

        // Validación de prioridades
        if (estratega->algoritmo == ALGORITMO_PLANIFICACION_CMN) {
            if (proceso->cola_prioridad < 0 || proceso->cola_prioridad >= list_size(estratega->colas)) {
                log_error(estratega->logger, "Proceso %d rechazado: prioridad %d fuera de rango", proceso->pid, proceso->cola_prioridad);
                proceso->estado = ESTADO_PROCESO_EXIT;
                
                // Avisamos a Memoria para que libere estructuras (LIBERAR_PROCESO)
                pthread_mutex_lock(&estratega->mutex_socket_memoria);
                enviar_int(estratega->socket_memoria, LIBERAR_PROCESO);
                send(estratega->socket_memoria, &proceso->pid, sizeof(uint32_t), 0);
                int resp_mem = esperar_respuesta_memoria(estratega->socket_memoria, estratega);
                if (resp_mem == MENSAJE) {
                    int size_m; char* msj_m = recibir_buffer(&size_m, estratega->socket_memoria); free(msj_m);
                }
                pthread_mutex_unlock(&estratega->mutex_socket_memoria);
                
                free(proceso);
                continue; // Buscamos si hay otro proceso válido en cola_new
            }
        } else {
            // FIFO y RR: La prioridad no tiene injerencia. Forzamos a la única cola (0)
            proceso->cola_prioridad = 0;
        }

        t_estado_proceso estado_anterior = proceso->estado;
        proceso->estado = ESTADO_PROCESO_READY;
        //=============================================================
        //revisar este bloque si es necesario o no este bloque
        if (estado_anterior == ESTADO_PROCESO_SUSP_READY) {
            log_info(estratega->logger, "## Solicitando DESUSPENDER_PROCESO a Memoria para PID %d", proceso->pid);
            pthread_mutex_lock(&estratega->mutex_socket_memoria);
            enviar_int(estratega->socket_memoria, DESUSPENDER_PROCESO);
            send(estratega->socket_memoria, &proceso->pid, sizeof(uint32_t), 0);
            
            int conf = esperar_respuesta_memoria(estratega->socket_memoria, estratega);
            if (conf == SOLICITUD_COMPACTACION) {
                conf = planificador_orquestar_compactacion_sincronica(estratega);
            }
            if (conf == MENSAJE) {
                int size;
                char* msj = recibir_buffer(&size, estratega->socket_memoria);
                free(msj);
            }
            pthread_mutex_unlock(&estratega->mutex_socket_memoria);
        }
        //=============================================================

        log_info(estratega->logger, "## (%d) Pasa del estado %s al estado %s", 
                 proceso->pid, 
                 planificador_nombre_estado(estado_anterior), 
                 planificador_nombre_estado(proceso->estado));

        encolar_en_cola_ready(estratega->colas, proceso);
        //============================================
        //bloque de codigo agregado para verificar que si esta habilitado el desalojo, que evalue si es desalojable el nuevo proceso ingresado
        // Validamos si hay un proceso ejecutando actualmente en la CPU
        if (estratega->proceso_exec != NULL) {
            // La función internamente evalúa si el desalojo está habilitado y si las prioridades lo ameritan
            if (planificador_debe_desalojar(estratega, estratega->proceso_exec->cola_prioridad, proceso->cola_prioridad)) {
                
                // Log obligatorio del TP
                log_info(estratega->logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
                         estratega->proceso_exec->pid, estratega->proceso_exec->cola_prioridad,
                         proceso->pid, proceso->cola_prioridad);
                
                // Enviamos la interrupción usando la nueva operación
                int opcode_interrupcion = INTERRUPCION_DESALOJO;
                planificador_enviar_interrupcion(estratega, opcode_interrupcion);
            }
        }
    }

    pthread_mutex_unlock(&estratega->mutex_colas);
    return NULL;
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

typedef struct {
    t_estratega_planificacion* estratega;
    t_proceso_planificable* pcb;
    int bloqueo_id;
} t_timer_args;

void* suspension_timer_thread(void* arg) {
    t_timer_args* a = (t_timer_args*)arg;
    usleep(a->estratega->suspension_timeout * 1000); // usleep takes microseconds
    
    pthread_mutex_lock(&a->estratega->mutex_colas);
    if (a->pcb->estado == ESTADO_PROCESO_BLOCK && a->pcb->bloqueo_id == a->bloqueo_id) {
        log_info(a->estratega->logger, "## Proceso %d suspendido por timeout", a->pcb->pid);
        a->pcb->estado = ESTADO_PROCESO_SUSP_BLOCK;
        
        // Obtenemos el tiempo actual (timestamp) utilizando la libreria <sys/time.h>
        struct timeval tiempo_actual;
        gettimeofday(&tiempo_actual, NULL);
        
        // Convertimos el tiempo a milisegundos y lo guardamos en el PCB para futuros desempates
        uint64_t segundos_actuales = tiempo_actual.tv_sec;
        uint64_t microsegundos_actuales = tiempo_actual.tv_usec;
        
        uint64_t milisegundos_por_segundo = segundos_actuales * 1000;
        uint64_t milisegundos_por_microsegundo = microsegundos_actuales / 1000;
        
        pthread_mutex_lock(&a->estratega->mutex_socket_memoria);
        enviar_int(a->estratega->socket_memoria, SUSPENDER_PROCESO);
        send(a->estratega->socket_memoria, &a->pcb->pid, sizeof(uint32_t), 0);
        
        int resp_mem = esperar_respuesta_memoria(a->estratega->socket_memoria, a->estratega);
        if (resp_mem == MENSAJE) {
            int size_m; char* msj_m = recibir_buffer(&size_m, a->estratega->socket_memoria); free(msj_m);
        }
        pthread_mutex_unlock(&a->estratega->mutex_socket_memoria);
        pthread_mutex_unlock(&a->estratega->mutex_colas);
        
        planificador_admitir_a_ready(a->estratega);
        free(a);
        return NULL;
    }
    pthread_mutex_unlock(&a->estratega->mutex_colas);
    free(a);
    return NULL;
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

    // Iniciar hilo de timer para suspender proceso
    typedef struct {
        t_estratega_planificacion* estratega;
        t_proceso_planificable* pcb;
        int bloqueo_id;
    } t_timer_args;
    
    t_timer_args* args = malloc(sizeof(t_timer_args));
    args->estratega = estratega;
    args->pcb = pcb;
    args->bloqueo_id = pcb->bloqueo_id;
    
    pthread_t timer_thread;
    pthread_create(&timer_thread, NULL, (void*)suspension_timer_thread, args);
    pthread_detach(timer_thread);

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
            if (liberado != NULL) {
                // Log obligatorio del TP
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
                    return;
                } else {
                    encolar_en_cola_ready(estratega->colas, liberado);
                    //============================================
                    // Validamos si hay un proceso ejecutando actualmente en la CPU
                    if (estratega->proceso_exec != NULL) {
                        // Evaluamos el desalojo con la prioridad del proceso 'liberado'
                        if (planificador_debe_desalojar(estratega, estratega->proceso_exec->cola_prioridad, liberado->cola_prioridad)) {
                            
                            // Log obligatorio del TP
                            log_info(estratega->logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
                                     estratega->proceso_exec->pid, estratega->proceso_exec->cola_prioridad,
                                     liberado->pid, liberado->cola_prioridad);
                            
                            int opcode_interrupcion = INTERRUPCION_DESALOJO;
                            planificador_enviar_interrupcion(estratega, opcode_interrupcion);
                        }
                    }
                    //============================
                }
                
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
        mutex->pcb_duenio = pcb; // <--- Guardamos el puntero directo al dueño
        
        // Log obligatorio del TP
        log_info(estratega->logger, "## (%d) Toma el Mutex %s", pcb->pid, recurso);
        
        pthread_mutex_unlock(&mutex->mutex_semaforo);
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 0; // Retorna 0 (no se bloqueó, sigue en EXEC)
    } else {
        // Ocupado: a la cola de bloqueados del Mutex
        pcb->estado = ESTADO_PROCESO_BLOCK;
        queue_push(mutex->cola_bloqueados, pcb);
        log_info(estratega->logger, "## Proceso %d bloqueado esperando Mutex %s", pcb->pid, recurso);
        
         // =========================================================
        // LÓGICA DE HERENCIA DE PRIORIDADES
        // =========================================================
        // Si el solicitante tiene MAYOR prioridad (número menor) que el dueño
        if (mutex->pcb_duenio != NULL && pcb->cola_prioridad < mutex->pcb_duenio->cola_prioridad) {
            
            // Log Obligatorio del TP
            log_info(estratega->logger, "## %d Cambio de prioridad: %d - %d", 
                     mutex->pcb_duenio->pid, mutex->pcb_duenio->cola_prioridad, pcb->cola_prioridad);

            int prioridad_vieja = mutex->pcb_duenio->cola_prioridad;
            
            // El dueño hereda la prioridad alta
            mutex->pcb_duenio->cola_prioridad = pcb->cola_prioridad; 

            // ¡CRÍTICO!: Si el dueño justo estaba esperando en READY, cambiar el número de 
            // su prioridad no es suficiente. Tenemos que reubicarlo físicamente a la nueva cola.
            if (mutex->pcb_duenio->estado == ESTADO_PROCESO_READY) {
                // 1. Lo sacamos de su cola actual (de baja prioridad)
                t_cola_planificacion* cola_vieja = obtener_cola_planificacion(estratega->colas, prioridad_vieja);
                list_remove_element(cola_vieja->procesos->elements, mutex->pcb_duenio);
                
                // 2. Lo encolamos en la nueva cola (de alta prioridad)
                encolar_en_cola_ready(estratega->colas, mutex->pcb_duenio);
            }
        }
        // =========================================================

        // Lo sacamos de EXEC
        if (estratega->proceso_exec != NULL && estratega->proceso_exec->pid == pcb->pid) {
            estratega->proceso_exec = NULL;
        }

        pthread_mutex_unlock(&mutex->mutex_semaforo);
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 1; // Retorna 1 (se bloqueó)
    }
}

int planificador_mutex_unlock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso) {
    if (estratega == NULL || pcb == NULL || recurso == NULL) return 0;

    int debe_desalojar = 0;

    pthread_mutex_lock(&estratega->mutex_colas);
    t_mutex* mutex = dictionary_get(estratega->mutex_creados, recurso);
    if (mutex == NULL) {
        pthread_mutex_unlock(&estratega->mutex_colas);
        return 0;
    }

    pthread_mutex_lock(&mutex->mutex_semaforo);
    if (mutex->tomado == 1 && mutex->pid_duenio == pcb->pid) {
        // Log obligatorio del TP
        log_info(estratega->logger, "## (%d) Libera el Mutex %s", pcb->pid, recurso);

          // ==============================================================
        // RESTAURACIÓN DE PRIORIDAD (VUELTA DE LA HERENCIA)
        // ==============================================================
        // Si el proceso que libera el Mutex había heredado una prioridad mayor, 
        // se la restauramos a su valor original
        if (pcb->cola_prioridad != pcb->prioridad_original) {
            // Log obligatorio del TP de cambio de prioridad
            log_info(estratega->logger, "## %d Cambio de prioridad: %d - %d", 
                     pcb->pid, pcb->cola_prioridad, pcb->prioridad_original);
            
            pcb->cola_prioridad = pcb->prioridad_original;
        }
        // ==============================================================
        
        // Vemos si hay alguien esperando
        if (!queue_is_empty(mutex->cola_bloqueados)) {
            t_proceso_planificable* liberado = queue_pop(mutex->cola_bloqueados);
            // Le damos la tenencia del Mutex al desencolado
            mutex->pid_duenio = liberado->pid;
            mutex->pcb_duenio = liberado; // asignando atributo nuevo

            log_info(estratega->logger, "## Proceso %d desencolado y asume Mutex %s", liberado->pid, recurso);
            
            // Lo mandamos a READY
            t_estado_proceso estado_anterior = liberado->estado;
            liberado->estado = ESTADO_PROCESO_READY;
            
            log_info(estratega->logger, "## (%d) Pasa del estado %s al estado %s", 
                     liberado->pid, 
                     planificador_nombre_estado(estado_anterior), 
                     planificador_nombre_estado(liberado->estado));

            encolar_en_cola_ready(estratega->colas, liberado);
            
            //============================================
            // Validamos si hay un proceso ejecutando actualmente en la CPU
            if (estratega->proceso_exec != NULL) {
                // Evaluamos el desalojo con la prioridad del proceso 'liberado'
                if (planificador_debe_desalojar(estratega, estratega->proceso_exec->cola_prioridad, liberado->cola_prioridad)) {
                    
                    // Log obligatorio del TP
                    log_info(estratega->logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
                             estratega->proceso_exec->pid, estratega->proceso_exec->cola_prioridad,
                             liberado->pid, liberado->cola_prioridad);
                    
                    debe_desalojar = 1;
                }
            }
            //============================================
        } else {
            // Nadie esperaba, el mutex queda libre
            mutex->tomado = 0;
            mutex->pid_duenio = -1;
            mutex->pcb_duenio = NULL; // <--- Limpiamos el puntero por seguridad

        }
    }
    pthread_mutex_unlock(&mutex->mutex_semaforo);
    pthread_mutex_unlock(&estratega->mutex_colas);
    return debe_desalojar;
}

void planificador_finalizar_todo_por_bsod(t_estratega_planificacion* estratega) {
    
    // ✅ 1. Finalizar el proceso que se está ejecutando actualmente en la CPU
    if (estratega->proceso_exec != NULL) {
        estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
        log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", estratega->proceso_exec->pid);
    }

    // ✅ 2. Vaciar y finalizar todos los procesos en estado NEW
    while (!queue_is_empty(estratega->cola_new)) {
        t_proceso_planificable* proceso = queue_pop(estratega->cola_new);
        proceso->estado = ESTADO_PROCESO_EXIT;
        log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
        free(proceso);
    }

    // ✅ 3. Vaciar y finalizar todos los procesos en estado READY (Colas Multinivel)
    for (int i = 0; i < list_size(estratega->colas); i++) {
        t_cola_planificacion* cola = list_get(estratega->colas, i);
        while (!queue_is_empty(cola->procesos)) {
            t_proceso_planificable* proceso = queue_pop(cola->procesos);
            proceso->estado = ESTADO_PROCESO_EXIT;
            log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
            free(proceso);
        }
    }
    
    // ✅ 4. Vaciar procesos BLOCKED por I/O
    t_list* interfaces_io = dictionary_elements(estratega->colas_io);
    for (int i = 0; i < list_size(interfaces_io); i++) {
        t_interfaz_io* interfaz = list_get(interfaces_io, i);
        
        // A. Matar la solicitud que está ejecutando actualmente
        if (interfaz->solicitud_actual != NULL) {
            t_proceso_planificable* proceso = interfaz->solicitud_actual->proceso;
            proceso->estado = ESTADO_PROCESO_EXIT;
            log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
            free(interfaz->solicitud_actual);
            interfaz->solicitud_actual = NULL;
        }

        // B. Vaciar la cola de bloqueados esperando por esta interfaz
        while (!queue_is_empty(interfaz->cola_bloqueados)) {
            // Cuidado: Esta cola guarda wrappers t_solicitud_io*, no procesos directos
            t_solicitud_io* solicitud = queue_pop(interfaz->cola_bloqueados);
            t_proceso_planificable* proceso = solicitud->proceso;
            
            proceso->estado = ESTADO_PROCESO_EXIT;
            log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
            
            free(solicitud); // Liberamos el wrapper
            free(proceso);   // Liberamos el PCB local
        }

        // ✅ 5. Vaciar procesos BLOCKED por Mutex
        t_list* lista_mutex = dictionary_elements(estratega->mutex_creados);
        for (int i = 0; i < list_size(lista_mutex); i++) {
            t_mutex* mutex = list_get(lista_mutex, i);
            
            while (!queue_is_empty(mutex->cola_bloqueados)) {
                t_proceso_planificable* proceso = queue_pop(mutex->cola_bloqueados);
                proceso->estado = ESTADO_PROCESO_EXIT;
                log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
                free(proceso);
            }
        }
        list_destroy(lista_mutex);
        
        // ✅ 6. Vaciar SUSP_READY y SUSP_BLOCKED
        // Como usamos t_list para poder ordenarlas por prioridad, vaciamos usando list_size y list_remove

        if (estratega->cola_susp_ready != NULL) {
            while (list_size(estratega->cola_susp_ready) > 0) {
                // list_remove en el índice 0 funciona igual que un queue_pop
                t_proceso_planificable* proceso = list_remove(estratega->cola_susp_ready, 0);
                proceso->estado = ESTADO_PROCESO_EXIT;
                log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
                free(proceso);
            }
        }

        if (estratega->cola_susp_blocked != NULL) {
            while (list_size(estratega->cola_susp_blocked) > 0) {
                t_proceso_planificable* proceso = list_remove(estratega->cola_susp_blocked, 0);
                proceso->estado = ESTADO_PROCESO_EXIT;
                log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
                free(proceso);
            }
        }
    }
    list_destroy(interfaces_io); // Limpiamos la lista auxiliar generada por el diccionario
    // (Nota: Si tuvieras procesos en las colas de BLOQUEADOS de I/O o Mutex, 
    // podrías iterar los diccionarios aquí de la misma forma para ser 100% exhaustivo).
}


bool comparador_procesos_suspendidos(void* a, void* b) {
    t_proceso_planificable* p1 = (t_proceso_planificable*)a;
    t_proceso_planificable* p2 = (t_proceso_planificable*)b;

    // Si tienen distinta prioridad, priorizamos el de menor número (Prioridad 0 es máxima)
    if (p1->cola_prioridad != p2->cola_prioridad) {
        return p1->cola_prioridad < p2->cola_prioridad;
    }
    
    // Si tienen la misma prioridad, el que tenga el instante de suspensión más antiguo
    return p1->instante_suspension < p2->instante_suspension;
}

void planificador_intentar_desuspender(t_estratega_planificacion* estratega) {
    list_sort(estratega->cola_susp_ready, comparador_procesos_suspendidos);

    int i = 0;
    while (i < list_size(estratega->cola_susp_ready)) {
        t_proceso_planificable* proceso = list_get(estratega->cola_susp_ready, i);

        // A. Consultamos a memoria
        pthread_mutex_lock(&estratega->mutex_socket_memoria);
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = CONSULTAR_ESPACIO_LIBRE;
        agregar_a_paquete(paq, &(proceso->pid), sizeof(uint32_t));
        enviar_paquete(paq, estratega->socket_memoria); // Usamos el socket del estratega
        eliminar_paquete(paq);

        // B. Esperamos respuesta
        int codigo_rta = esperar_respuesta_memoria(estratega->socket_memoria, estratega);
        int hay_espacio = 0;
        if (codigo_rta == MENSAJE) { // Ajustá al opcode real que te mande la Memoria
            hay_espacio = recibir_int(estratega->socket_memoria); 
        }
        pthread_mutex_unlock(&estratega->mutex_socket_memoria);

        // C. Evaluamos
        if (hay_espacio) {
            list_remove(estratega->cola_susp_ready, i);
            
            // Avisamos a memoria para des-suspender
            pthread_mutex_lock(&estratega->mutex_socket_memoria);
            t_paquete* paq_desusp = crear_paquete();
            paq_desusp->codigo_operacion = DESUSPENDER_PROCESO;
            agregar_a_paquete(paq_desusp, &(proceso->pid), sizeof(uint32_t));
            enviar_paquete(paq_desusp, estratega->socket_memoria);
            eliminar_paquete(paq_desusp);
            
            // Esperamos confirmacion de memoria
            int confirmacion = esperar_respuesta_memoria(estratega->socket_memoria, estratega);
            
            if (confirmacion == SOLICITUD_COMPACTACION) {
                confirmacion = planificador_orquestar_compactacion_sincronica(estratega);
            }

            if (confirmacion == -1) {
                log_error(estratega->logger, "Error: Kernel Memory se desconectó");
                pthread_mutex_unlock(&estratega->mutex_socket_memoria);
                return;
            }
            if (confirmacion == MENSAJE) {
                int size;
                char* msj = recibir_buffer(&size, estratega->socket_memoria);
                if (string_equals_ignore_case(msj, "ERROR")) {
                    log_error(estratega->logger, "No hay espacio en Memoria Principal para desuspender PID %d", proceso->pid);
                    free(msj);
                    pthread_mutex_unlock(&estratega->mutex_socket_memoria);
                    continue; // Intentamos con el siguiente
                }
                free(msj);
            }
            pthread_mutex_unlock(&estratega->mutex_socket_memoria);

            // Lo pasamos a READY
            proceso->estado = ESTADO_PROCESO_READY;
            log_info(estratega->logger, "## (%d) Pasa del estado SUSP_READY al estado READY", proceso->pid);
            encolar_en_cola_ready(estratega->colas, proceso);
            //============================================
                //bloque de codigo agregado para verificar que si esta habilitado el desalojo, que evalue si es desalojable el nuevo proceso ingresado
                // Validamos si hay un proceso ejecutando actualmente en la CPU
                if (estratega->proceso_exec != NULL) {
                    // La función internamente evalúa si el desalojo está habilitado y si las prioridades lo ameritan
                    if (planificador_debe_desalojar(estratega, estratega->proceso_exec->cola_prioridad, proceso->cola_prioridad)) {
                        
                        // Log obligatorio del TP
                        log_info(estratega->logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
                                 estratega->proceso_exec->pid, estratega->proceso_exec->cola_prioridad,
                                 proceso->pid, proceso->cola_prioridad);
                        
                        // Enviamos la interrupción usando la nueva operación
                        int opcode_interrupcion = INTERRUPCION_DESALOJO;
                        planificador_enviar_interrupcion(estratega, opcode_interrupcion);
                    }
                }
                //============================
        } else {
            i++;
        }
    }
}

int planificador_orquestar_compactacion_sincronica(t_estratega_planificacion* estratega) {
    log_info(estratega->logger, "## Inicio de compactación (Desuspender)");

    // 1. Interrumpimos a la CPU que pueda estar ejecutando un proceso
    if (estratega->proceso_exec != NULL && estratega->sockets_cpu != NULL && list_size(estratega->sockets_cpu) > 0) {
        planificador_enviar_interrupcion(estratega, INTERRUPCION_COMPACTACION);
        
        // Asumiendo una sola CPU activa en la arquitectura del sistema, la atrapamos en el index 0
        int socket_cpu = *(int*)list_get(estratega->sockets_cpu, 0);
        
        // Esperamos sincrónicamente a que la CPU frene y nos devuelva el DESALOJO
        int op_cpu = recibir_operacion(socket_cpu);
        if (op_cpu == DESALOJO_COMPACTACION) {
            t_list* paquete = recibir_paquete(socket_cpu);
            list_destroy_and_destroy_elements(paquete, free);
            
            t_proceso_planificable* desalojado = estratega->proceso_exec;
            desalojado->estado = ESTADO_PROCESO_READY;
            
            // "Los Procesos desalojados, excepcionalmente, serán colocados al principio de la cola de READY."
            t_cola_planificacion* cola = obtener_cola_planificacion(estratega->colas, desalojado->cola_prioridad);
            list_add_in_index(cola->procesos->elements, 0, desalojado);
            
            estratega->proceso_exec = NULL;
        } else {
            log_error(estratega->logger, "Operación inesperada de CPU esperando desalojo por compactación: %d", op_cpu);
        }
    }

    // 2. Autorizamos la compactación en Memoria enviando RESPUESTA_COMPACTACION
    int op_resp = RESPUESTA_COMPACTACION;
    send(estratega->socket_memoria, &op_resp, sizeof(int), 0);
    
    // 3. Esperamos el resultado de la memoria luego de compactar (OK/ERROR)
    return esperar_respuesta_memoria(estratega->socket_memoria, estratega);
}

void* hilo_desuspender_memoria(void* arg) {
    t_estratega_planificacion* estratega = (t_estratega_planificacion*)arg;
    planificador_intentar_desuspender(estratega);
    return NULL;
}

int esperar_respuesta_memoria(int socket_memoria, t_estratega_planificacion* estratega) {
    int respuesta = recibir_operacion(socket_memoria);
    bool hubo_expansion = false;
    while (respuesta == MEMORIA_EXPANDIDA) {
        log_info(estratega->logger, "Memoria expandida: Se detectó un nuevo Memory Stick.");
        hubo_expansion = true;
        respuesta = recibir_operacion(socket_memoria);
    }
    
    if (hubo_expansion) {
        pthread_t hilo_desuspension;
        pthread_create(&hilo_desuspension, NULL, (void*)hilo_desuspender_memoria, estratega);
        pthread_detach(hilo_desuspension);
    }
    
    return respuesta;
}
