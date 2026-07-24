#include "scheduler_operaciones.h"

void despachar_proceso_a_cpu(t_scheduler_context* ctx, int socket_cpu) {
    t_proceso_planificable* proceso_a_ejecutar = planificador_despachar_siguiente(ctx->estratega);
    
    if (proceso_a_ejecutar != NULL) {
        t_paquete* paquete_pid = crear_paquete();
        paquete_pid->codigo_operacion = PID_A_EJECUTAR;
        
        agregar_a_paquete(paquete_pid, &(proceso_a_ejecutar->pid), sizeof(int));
        enviar_paquete(paquete_pid, socket_cpu);
        eliminar_paquete(paquete_pid);
        
        log_info(ctx->logger, "Se envió el PID %d a la CPU para ejecutar", proceso_a_ejecutar->pid);
        planificador_iniciar_quantum_si_corresponde(ctx->estratega, proceso_a_ejecutar, socket_cpu);
    } else {
        log_info(ctx->logger, "No hay procesos en la cola READY para despachar a la CPU.");
    }
}

void procesar_fin_quantum(int cliente_fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    if (ctx->estratega->proceso_exec != NULL) {
        log_info(ctx->logger, "## Fin de Quantum para proceso %d - Desalojado", ctx->estratega->proceso_exec->pid);
        
        planificador_reencolar_por_quantum(ctx->estratega, ctx->estratega->proceso_exec);
        planificador_liberar_exec(ctx->estratega, ctx->estratega->proceso_exec->pid);
    }
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void devolver_ejecucion_a_cpu(t_scheduler_context* ctx, int cliente_fd) {
    t_paquete* paquete_pid = crear_paquete();
    paquete_pid->codigo_operacion = PID_A_EJECUTAR;
    agregar_a_paquete(paquete_pid, &(ctx->estratega->proceso_exec->pid), sizeof(int));
    enviar_paquete(paquete_pid, cliente_fd);
    eliminar_paquete(paquete_pid);
}


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
}