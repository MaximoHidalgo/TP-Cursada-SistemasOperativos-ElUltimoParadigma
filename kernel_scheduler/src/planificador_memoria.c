#include "planificador_memoria.h"
#include "planificador.h"
#include <pthread.h>

void planificador_finalizar_todo_por_bsod(t_estratega_planificacion* estratega) {
    if (estratega->proceso_exec != NULL) {
        estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
        log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", estratega->proceso_exec->pid);
    }

    while (!queue_is_empty(estratega->cola_new)) {
        t_proceso_planificable* proceso = queue_pop(estratega->cola_new);
        proceso->estado = ESTADO_PROCESO_EXIT;
        log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
        free(proceso);
    }

    for (int i = 0; i < list_size(estratega->colas); i++) {
        t_cola_planificacion* cola = list_get(estratega->colas, i);
        while (!queue_is_empty(cola->procesos)) {
            t_proceso_planificable* proceso = queue_pop(cola->procesos);
            proceso->estado = ESTADO_PROCESO_EXIT;
            log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
            free(proceso);
        }
    }
    
    t_list* interfaces_io = dictionary_elements(estratega->colas_io);
    for (int i = 0; i < list_size(interfaces_io); i++) {
        t_interfaz_io* interfaz = list_get(interfaces_io, i);
        
        if (interfaz->solicitud_actual != NULL) {
            t_proceso_planificable* proceso = interfaz->solicitud_actual->proceso;
            proceso->estado = ESTADO_PROCESO_EXIT;
            log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
            free(interfaz->solicitud_actual);
            interfaz->solicitud_actual = NULL;
        }

        while (!queue_is_empty(interfaz->cola_bloqueados)) {
            t_solicitud_io* solicitud = queue_pop(interfaz->cola_bloqueados);
            t_proceso_planificable* proceso = solicitud->proceso;
            
            proceso->estado = ESTADO_PROCESO_EXIT;
            log_info(estratega->logger, "## (%d) finalizó su ejecución con motivo de BSOD", proceso->pid);
            
            free(solicitud);
            free(proceso);
        }
    }
    
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
    
    if (estratega->cola_susp_ready != NULL) {
        while (list_size(estratega->cola_susp_ready) > 0) {
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
    
    list_destroy(interfaces_io);
}

bool comparador_procesos_suspendidos(void* a, void* b) {
    t_proceso_planificable* p1 = (t_proceso_planificable*)a;
    t_proceso_planificable* p2 = (t_proceso_planificable*)b;

    if (p1->cola_prioridad != p2->cola_prioridad) {
        return p1->cola_prioridad < p2->cola_prioridad;
    }
    return p1->instante_suspension < p2->instante_suspension;
}

void planificador_intentar_desuspender(t_estratega_planificacion* estratega) {
    list_sort(estratega->cola_susp_ready, comparador_procesos_suspendidos);

    int i = 0;
    while (i < list_size(estratega->cola_susp_ready)) {
        t_proceso_planificable* proceso = list_get(estratega->cola_susp_ready, i);

        pthread_mutex_lock(&estratega->mutex_socket_memoria);
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = CONSULTAR_ESPACIO_LIBRE;
        agregar_a_paquete(paq, &(proceso->pid), sizeof(uint32_t));
        enviar_paquete(paq, estratega->socket_memoria);
        eliminar_paquete(paq);

        int codigo_rta = esperar_respuesta_memoria(estratega->socket_memoria, estratega);
        int hay_espacio = 0;
        if (codigo_rta == MENSAJE) { 
            hay_espacio = recibir_int(estratega->socket_memoria); 
        }
        pthread_mutex_unlock(&estratega->mutex_socket_memoria);

        if (hay_espacio) {
            list_remove(estratega->cola_susp_ready, i);
            
            pthread_mutex_lock(&estratega->mutex_socket_memoria);
            t_paquete* paq_desusp = crear_paquete();
            paq_desusp->codigo_operacion = DESUSPENDER_PROCESO;
            agregar_a_paquete(paq_desusp, &(proceso->pid), sizeof(uint32_t));
            enviar_paquete(paq_desusp, estratega->socket_memoria);
            eliminar_paquete(paq_desusp);
            
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
                    continue; 
                }
                free(msj);
            }
            pthread_mutex_unlock(&estratega->mutex_socket_memoria);

            proceso->estado = ESTADO_PROCESO_READY;
            log_info(estratega->logger, "## (%d) Pasa del estado SUSP_READY al estado READY", proceso->pid);
            encolar_en_cola_ready(estratega->colas, proceso);
            
            if (estratega->proceso_exec != NULL) {
                if (planificador_debe_desalojar(estratega, estratega->proceso_exec->cola_prioridad, proceso->cola_prioridad)) {
                    log_info(estratega->logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d",
                             estratega->proceso_exec->pid, estratega->proceso_exec->cola_prioridad,
                             proceso->pid, proceso->cola_prioridad);
                    
                    int opcode_interrupcion = INTERRUPCION_DESALOJO;
                    planificador_enviar_interrupcion(estratega, opcode_interrupcion);
                }
            }
        } else {
            i++;
        }
    }
}

int planificador_orquestar_compactacion_sincronica(t_estratega_planificacion* estratega) {
    log_info(estratega->logger, "## Inicio de compactación (Desuspender)");

    if (estratega->proceso_exec != NULL && estratega->sockets_cpu != NULL && list_size(estratega->sockets_cpu) > 0) {
        planificador_enviar_interrupcion(estratega, INTERRUPCION_COMPACTACION);
        
        int socket_cpu = *(int*)list_get(estratega->sockets_cpu, 0);
        
        int op_cpu = recibir_operacion(socket_cpu);
        if (op_cpu == DESALOJO_COMPACTACION) {
            t_list* paquete = recibir_paquete(socket_cpu);
            list_destroy_and_destroy_elements(paquete, free);
            
            t_proceso_planificable* desalojado = estratega->proceso_exec;
            desalojado->estado = ESTADO_PROCESO_READY;
            
            t_cola_planificacion* cola = obtener_cola_planificacion(estratega->colas, desalojado->cola_prioridad);
            list_add_in_index(cola->procesos->elements, 0, desalojado);
            
            estratega->proceso_exec = NULL;
        } else {
            log_error(estratega->logger, "Operación inesperada de CPU esperando desalojo por compactación: %d", op_cpu);
        }
    }

    int op_resp = RESPUESTA_COMPACTACION;
    send(estratega->socket_memoria, &op_resp, sizeof(int), 0);
    
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
