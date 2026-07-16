#include "atender_cliente_scheduler.h"

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

static void devolver_ejecucion_a_cpu(t_scheduler_context* ctx, int cliente_fd) {
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
                if (ctx->estratega->proceso_exec == NULL && ctx->estratega->socket_cpu != -1) {
                    despachar_proceso_a_cpu(ctx, ctx->estratega->socket_cpu);
                }
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
                    agregar_a_paquete(paq_write, &(proceso->io_dir_fisica), sizeof(uint32_t));
                    agregar_a_paquete(paq_write, &(proceso->io_tamanio), sizeof(uint32_t));
                    agregar_a_paquete(paq_write, mensaje, proceso->io_tamanio);
                    enviar_paquete(paq_write, ctx->socket_memoria);
                    eliminar_paquete(paq_write);
                    
                    int r_mem = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
                    if (r_mem == MENSAJE) {
                        int s_m; char* msj_m = recibir_buffer(&s_m, ctx->socket_memoria); free(msj_m);
                    }
                }
                
                planificador_io_terminada(ctx->estratega, cliente_fd);
                if (ctx->estratega->proceso_exec == NULL && ctx->estratega->socket_cpu != -1) {
                    despachar_proceso_a_cpu(ctx, ctx->estratega->socket_cpu);
                }
            }
        } else {
            // Es el nombre de una nueva interfaz registrandose
            planificador_registrar_io(ctx->estratega, mensaje, cliente_fd);
        }
    }
    free(mensaje);
}

static void manejar_io_sleep(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
    uint32_t* tiempo_ptr = list_get(paquete, 1);
    t_interfaz_io* interfaz = buscar_interfaz_por_nombre(ctx->estratega, "SLEEP");
    if (interfaz != NULL) {
        int op = PETICION_IO;
        send(interfaz->socket_fd, &op, sizeof(int), 0);
        send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
        send(interfaz->socket_fd, tiempo_ptr, sizeof(uint32_t), 0);
    }
    planificador_atender_peticion_io(ctx->estratega, ctx->estratega->proceso_exec, "SLEEP");
}

static void manejar_io_stdout(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
    uint32_t* dir_fisica_ptr = list_get(paquete, 1);
    uint32_t* tamanio_ptr = list_get(paquete, 2);
    uint32_t dir_fisica = *dir_fisica_ptr;
    uint32_t tamanio = *tamanio_ptr;
    
    t_interfaz_io* interfaz = buscar_interfaz_por_nombre(ctx->estratega, "STDOUT");
    if (interfaz != NULL) {
        // Pedimos leer a memoria
        t_paquete* paq_mem = crear_paquete();
        paq_mem->codigo_operacion = LEER_MEMORIA;
        agregar_a_paquete(paq_mem, &dir_fisica, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, &tamanio, sizeof(uint32_t));
        enviar_paquete(paq_mem, ctx->socket_memoria);
        eliminar_paquete(paq_mem);
        
        int op = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
        if (op == MENSAJE) {
            int size;
            void* buf = recibir_buffer(&size, ctx->socket_memoria);
            
            int op_io = PETICION_IO;
            send(interfaz->socket_fd, &op_io, sizeof(int), 0);
            send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
            
            // Mandamos el tamaño y el buffer al IO
            int len = tamanio + 1;
            char* dummy = malloc(len);
            memcpy(dummy, buf, tamanio);
            dummy[tamanio] = '\0';
            
            send(interfaz->socket_fd, &len, sizeof(int), 0);
            send(interfaz->socket_fd, dummy, len, 0);
            free(dummy);
            free(buf);
        }
    }
    planificador_atender_peticion_io(ctx->estratega, ctx->estratega->proceso_exec, "STDOUT");
}

static void manejar_io_stdin(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
    uint32_t* dir_fisica_ptr = list_get(paquete, 1);
    uint32_t* tamanio_ptr = list_get(paquete, 2);
    uint32_t dir_fisica = *dir_fisica_ptr;
    uint32_t tamanio = *tamanio_ptr;
    
    t_interfaz_io* interfaz = buscar_interfaz_por_nombre(ctx->estratega, "STDIN");
    if (interfaz != NULL) {
        int op = PETICION_IO;
        send(interfaz->socket_fd, &op, sizeof(int), 0);
        send(interfaz->socket_fd, &pid, sizeof(uint32_t), 0);
        send(interfaz->socket_fd, &tamanio, sizeof(uint32_t), 0);
        
        ctx->estratega->proceso_exec->io_dir_fisica = dir_fisica;
        ctx->estratega->proceso_exec->io_tamanio = tamanio;
    }
    planificador_atender_peticion_io(ctx->estratega, ctx->estratega->proceso_exec, "STDIN");
}

void procesar_syscall_io(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    
    // Guard Clause: Validación del proceso actual
    if (ctx->estratega->proceso_exec == NULL || ctx->estratega->proceso_exec->pid != pid) {
        despachar_proceso_a_cpu(ctx, cliente_fd);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    switch (tipo_syscall) {
        case SYSCALL_SLEEP:
            manejar_io_sleep(ctx, paquete, pid);
            break;
        case SYSCALL_STDOUT:
            manejar_io_stdout(ctx, paquete, pid);
            break;
        case SYSCALL_STDIN:
            manejar_io_stdin(ctx, paquete, pid);
            break;
    }
    
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_exit(int cliente_fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    if (ctx->estratega->proceso_exec != NULL) {
        log_info(ctx->logger, "## Finaliza el proceso %d - Motivo: SUCCESS", ctx->estratega->proceso_exec->pid);
        ctx->estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
        
        t_paquete* paq_liberar = crear_paquete();
        paq_liberar->codigo_operacion = LIBERAR_PROCESO;
        agregar_a_paquete(paq_liberar, &(ctx->estratega->proceso_exec->pid), sizeof(uint32_t));
        enviar_paquete(paq_liberar, ctx->socket_memoria);
        eliminar_paquete(paq_liberar);
        
        planificador_liberar_exec(ctx->estratega, ctx->estratega->proceso_exec->pid);
    }
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_mutex(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    char* recurso = list_get(paquete, 1);

    // Guard Clause
    if (ctx->estratega->proceso_exec == NULL || ctx->estratega->proceso_exec->pid != pid) {
        despachar_proceso_a_cpu(ctx, cliente_fd);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    switch (tipo_syscall) {
        case SYSCALL_MUTEX_CREATE:
            planificador_mutex_create(ctx->estratega, ctx->estratega->proceso_exec, recurso);
            devolver_ejecucion_a_cpu(ctx, cliente_fd);
            break;

        case SYSCALL_MUTEX_LOCK: {
            int se_bloqueo = planificador_mutex_lock(ctx->estratega, ctx->estratega->proceso_exec, recurso);
            if (se_bloqueo == 0) {
                devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC
            } else {
                despachar_proceso_a_cpu(ctx, cliente_fd); // Se bloqueó
            }
            break;
        }

        case SYSCALL_MUTEX_UNLOCK:
            planificador_mutex_unlock(ctx->estratega, ctx->estratega->proceso_exec, recurso);
            devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC
            break;
    }
    
    list_destroy_and_destroy_elements(paquete, free);
}

void procesar_syscall_memoria(int cliente_fd, t_scheduler_context* ctx, int tipo_syscall) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;

    // Guard Clause: Validación del proceso actual
    if (ctx->estratega->proceso_exec == NULL || ctx->estratega->proceso_exec->pid != pid) {
        despachar_proceso_a_cpu(ctx, cliente_fd);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    if (tipo_syscall == SYSCALL_MEM_ALLOC) {
        uint32_t* id_seg_ptr = list_get(paquete, 1);
        uint32_t* tamanio_ptr = list_get(paquete, 2);
        
        t_paquete* paq_mem = crear_paquete();
        paq_mem->codigo_operacion = CREAR_SEGMENTO;
        agregar_a_paquete(paq_mem, &pid, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, id_seg_ptr, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, tamanio_ptr, sizeof(uint32_t));
        enviar_paquete(paq_mem, ctx->socket_memoria);
        eliminar_paquete(paq_mem);

        int respuesta = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
    
        if (respuesta == SOLICITUD_COMPACTACION) {
            log_info(ctx->logger, "## Inicio de compactación"); // Log obligatorio del TP
            
            // Le mandamos un "tiro" a la CPU para que frene
            // NOTA: Deberás agregar INTERRUPCION_COMPACTACION en tu opcodes.h
            int op_interrupcion = INTERRUPCION_COMPACTACION;
            send(ctx->estratega->socket_cpu, &op_interrupcion, sizeof(int), 0);
            
            // SALIMOS de la función. No esperamos nada. El hilo vuelve a escuchar el socket.
            return; 
        }
        if (respuesta == MENSAJE) {
            int size;
            char* msj = recibir_buffer(&size, ctx->socket_memoria);
            if (string_equals_ignore_case(msj, "OK")) {
                devolver_ejecucion_a_cpu(ctx, cliente_fd);
            } else {
                log_error(ctx->logger, "Out of memory. Finalizando proceso %d", pid);
                log_error(ctx->logger, "ERROR: Memoria insuficiente para crear el segmento del proceso %d.", pid);
                log_error(ctx->logger, "## (%d) finalizo su ejecucion con motivo de FALTA_DE_MEMORIA  %d.", pid);

                ctx->estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;
                
                t_paquete* paq_liberar = crear_paquete();
                paq_liberar->codigo_operacion = LIBERAR_PROCESO;
                agregar_a_paquete(paq_liberar, &pid, sizeof(uint32_t));
                enviar_paquete(paq_liberar, ctx->socket_memoria);
                eliminar_paquete(paq_liberar);

                planificador_liberar_exec(ctx->estratega, pid);
                despachar_proceso_a_cpu(ctx, cliente_fd);
            }
            free(msj);
        }
    } else if (tipo_syscall == SYSCALL_MEM_FREE) {
        uint32_t* id_seg_ptr = list_get(paquete, 1);

        t_paquete* paq_mem = crear_paquete();
        paq_mem->codigo_operacion = ELIMINAR_SEGMENTO;
        agregar_a_paquete(paq_mem, &pid, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, id_seg_ptr, sizeof(uint32_t));
        enviar_paquete(paq_mem, ctx->socket_memoria);
        eliminar_paquete(paq_mem);

        int respuesta = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
        if (respuesta == MENSAJE) {
            int size;
            char* msj = recibir_buffer(&size, ctx->socket_memoria);
            if (string_equals_ignore_case(msj, "OK")) {
                devolver_ejecucion_a_cpu(ctx, cliente_fd);
            }
            free(msj);
        }else{
            log_error(ctx->logger,"Error desconocido al intentar liberar el segmento %d del PID %d.",*id_seg_ptr,pid);
        }
    }
    
    list_destroy_and_destroy_elements(paquete, free);
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

void procesar_syscall_init_proc(int cliente_fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    uint32_t* pid_ptr = list_get(paquete, 0);
    uint32_t pid = *pid_ptr;
    char* script_path = list_get(paquete, 1);
    uint32_t* prioridad_ptr = list_get(paquete, 2);
    uint32_t prioridad = *prioridad_ptr;

    // Guard Clause
    if (ctx->estratega->proceso_exec == NULL || ctx->estratega->proceso_exec->pid != pid) {
        despachar_proceso_a_cpu(ctx, cliente_fd);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    static int contador_pid = 1;
    int nuevo_pid = contador_pid++;

    log_info(ctx->logger, "## Proceso %d solicita crear nuevo proceso con script %s", pid, script_path);

    enviar_peticion_crear_proceso(nuevo_pid, script_path, ctx->socket_memoria);

    t_proceso_planificable* pcb_nuevo = malloc(sizeof(t_proceso_planificable));
    pcb_nuevo->pid = nuevo_pid;
    pcb_nuevo->cola_prioridad = prioridad;
    pcb_nuevo->prioridad_original = prioridad;  // inicializar la prioridad original
    pcb_nuevo->bloqueo_id = 0;

    planificador_encolar_new(ctx->estratega, pcb_nuevo);
    planificador_admitir_a_ready(ctx->estratega);

    devolver_ejecucion_a_cpu(ctx, cliente_fd); // Sigue en EXEC

    list_destroy_and_destroy_elements(paquete, free);
}


// 1. Definimos la firma profesional 
//typedef int (*handler_t)(int cliente_fd, t_scheduler_context* ctx);

// 2. Aislamos la lógica compleja del SEG_FAULT en su propio handler
int handler_desalojo_compactacion(int fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(fd);
    
    if (ctx->estratega->proceso_exec != NULL) {
        t_proceso_planificable* desalojado = ctx->estratega->proceso_exec;
        desalojado->estado = ESTADO_PROCESO_READY;
        
        // El TP exige ponerlo al PRINCIPIO de la cola
        t_cola_planificacion* cola = obtener_cola_planificacion(ctx->estratega->colas, desalojado->cola_prioridad);
        
        // Como usamos t_queue de commons (que mete al final), accedemos a su lista interna 'elements'
        // y lo insertamos a la fuerza en el índice 0.
        //el tipo de procesos es: t_queue* procesos;, pero t_queue contiene una lista, lo que hago es aprovechar eso y romper el
        //encapsulamiento de la cola y manipulando su lista interna, para meter al proceso al principio de la cola ready
        //por que hago esto de esta forma?: porque la biblioteca de t_queue no me proporciona ninguna funcion de colocarlo al 
        //principo de la cola, solo el 	queue_push que lo agrega al final, pero no es algo que el enunciado diga ante este evento
        list_add_in_index(cola->procesos->elements, 0, desalojado);
        
        ctx->estratega->proceso_exec = NULL;
    }
    
    // Ahora que la CPU está libre, LE DAMOS EL OK A MEMORIA para que compacte
    t_paquete* paq_resp = crear_paquete();
    paq_resp->codigo_operacion = RESPUESTA_COMPACTACION;
    enviar_paquete(paq_resp, ctx->socket_memoria);
    eliminar_paquete(paq_resp);
    
    list_destroy_and_destroy_elements(paquete, free);
    return 1;
}

int handler_interrupcion_desalojo(int fd, t_scheduler_context* ctx) {
    // Recibimos el paquete que nos mandó la CPU
    t_list* paquete = recibir_paquete(fd);
    
    if (ctx->estratega->proceso_exec != NULL) {
        t_proceso_planificable* desalojado = ctx->estratega->proceso_exec;
        
        // 1. Lo pasamos a estado READY
        desalojado->estado = ESTADO_PROCESO_READY;
        
        // 2. Lo reencolamos en la cola multinivel que le corresponde
        encolar_en_cola_ready(ctx->estratega->colas, desalojado);
        
        // 3. Liberamos el puntero EXEC para que la CPU quede libre para el Scheduler
        ctx->estratega->proceso_exec = NULL;
        
        // 4. Espiamos quién es el nuevo proceso más prioritario (el culpable) para el log
        t_proceso_planificable* culpable = planificador_obtener_siguiente(ctx->estratega);
        
        if (culpable != NULL) {
            // LOG MÍNIMO Y OBLIGATORIO EXACTO
            log_info(ctx->logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso %d con prioridad %d", 
                     desalojado->pid, desalojado->cola_prioridad, 
                     culpable->pid, culpable->cola_prioridad);
        }
    }
    
    // 5. Mandamos a ejecutar a ese nuevo proceso que lidera la cola
    despachar_proceso_a_cpu(ctx, fd);
    
    list_destroy_and_destroy_elements(paquete, free);
    return 1;
}

int handler_seg_fault(int cliente_fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(cliente_fd);
    
    if (ctx->estratega->proceso_exec != NULL) {
        log_info(ctx->logger, "## Finaliza el proceso %d - Motivo: SEG_FAULT", ctx->estratega->proceso_exec->pid);
        ctx->estratega->proceso_exec->estado = ESTADO_PROCESO_EXIT;

        t_paquete* paq_liberar = crear_paquete();
        paq_liberar->codigo_operacion = LIBERAR_PROCESO;
        agregar_a_paquete(paq_liberar, &(ctx->estratega->proceso_exec->pid), sizeof(uint32_t));
        enviar_paquete(paq_liberar, ctx->socket_memoria);
        eliminar_paquete(paq_liberar);

        planificador_liberar_exec(ctx->estratega, ctx->estratega->proceso_exec->pid);
    }
    
    despachar_proceso_a_cpu(ctx, cliente_fd);
    list_destroy_and_destroy_elements(paquete, free);
    
    return 1; // Retornamos 1 para indicar que el hilo debe continuar escuchando
}

int handler_respuesta_compactacion(int fd, t_scheduler_context* ctx) {
    log_info(ctx->logger, "## Fin de compactación");
    
    // Primero intentamos des-suspender procesos (ahora que la memoria está contigua)
    planificador_intentar_desuspender(ctx->estratega);
    
    // Luego, volvemos a enviar procesos a CPU que estaban detenidos
    if (ctx->estratega->socket_cpu != -1) {
        despachar_proceso_a_cpu(ctx, ctx->estratega->socket_cpu);
    }
    return 1;
}

int handler_mem_expandida(int cliente_fd, t_scheduler_context* ctx){
    //recorrer todos procesos supendidos
    //    recorrer por orden de prioridad
    //    ir desuspendiendo
    //    proprocesos suspendidos con misma prioridad: se evalua primero el que mas tiempo lleve suspendido
    
    // Registramos que memoria tiene más espacio y luego:
    // 1. Simplemente logeamos el evento para cumplir y dejar rastro
    log_info(ctx->logger, "Memoria expandida: Se detectó un nuevo Memory Stick.");
    // 2. Disparamos la planificación de mediano plazo
    planificador_intentar_desuspender(ctx->estratega);
    return 1;
}

int handler_mem_corrupta(int cliente_fd, t_scheduler_context* ctx){
    planificador_finalizar_todo_por_bsod(ctx->estratega);
    limpiar_contexto(ctx);
    exit(EXIT_FAILURE);
}


// --- WRAPPERS PARA CADA OPERACIÓN ---
// Retornan 1 para indicar éxito continuo.

int handler_mensaje(int fd, t_scheduler_context* ctx) { procesar_mensaje_handshake(fd, ctx); return 1; }
int handler_exit(int fd, t_scheduler_context* ctx) { procesar_syscall_exit(fd, ctx); return 1; }
int handler_init_proc(int fd, t_scheduler_context* ctx) { procesar_syscall_init_proc(fd, ctx); return 1; }
int handler_fin_quantum(int fd, t_scheduler_context* ctx) { procesar_fin_quantum(fd, ctx); return 1; }

// I/O
int handler_sleep(int fd, t_scheduler_context* ctx) { procesar_syscall_io(fd, ctx, SYSCALL_SLEEP); return 1; }
int handler_stdout(int fd, t_scheduler_context* ctx) { procesar_syscall_io(fd, ctx, SYSCALL_STDOUT); return 1; }
int handler_stdin(int fd, t_scheduler_context* ctx) { procesar_syscall_io(fd, ctx, SYSCALL_STDIN); return 1; }

// Mutex
int handler_mutex_create(int fd, t_scheduler_context* ctx) { procesar_syscall_mutex(fd, ctx, SYSCALL_MUTEX_CREATE); return 1; }
int handler_mutex_lock(int fd, t_scheduler_context* ctx) { procesar_syscall_mutex(fd, ctx, SYSCALL_MUTEX_LOCK); return 1; }
int handler_mutex_unlock(int fd, t_scheduler_context* ctx) { procesar_syscall_mutex(fd, ctx, SYSCALL_MUTEX_UNLOCK); return 1; }

// Memoria
int handler_mem_alloc(int fd, t_scheduler_context* ctx) { procesar_syscall_memoria(fd, ctx, SYSCALL_MEM_ALLOC); return 1; }
int handler_mem_free(int fd, t_scheduler_context* ctx) { 
    procesar_syscall_memoria(fd, ctx, SYSCALL_MEM_FREE); 
    // Luego de que se procesó el free y memoria liberó espacio, intentamos des-suspender
    planificador_intentar_desuspender(ctx->estratega);
    return 1; }

//int handler_mem_expandida(int fd, t_scheduler_context* ctx) { procesar_memoria_expandida(fd, ctx, SYSCALL_MEM_ALLOC); return 1; }
//int handler_mem_corrupta(int fd, t_scheduler_context* ctx) { procesar_corrupcion_memoria(fd, ctx, SYSCALL_MEM_FREE); return 1; }


// Creamos un array estático de tamaño 256 (suficiente para cubrir todos los opcodes).
// Los índices que no declaremos se inicializarán automáticamente en NULL.
static const handler_t tabla_despacho[256] = {
    [MENSAJE]                    = handler_mensaje,
    [SYSCALL_EXIT]               = handler_exit,
    [SYSCALL_SLEEP]              = handler_sleep,
    [SYSCALL_STDOUT]             = handler_stdout,
    [SYSCALL_STDIN]              = handler_stdin,
    [SYSCALL_MUTEX_CREATE]       = handler_mutex_create,
    [SYSCALL_MUTEX_LOCK]         = handler_mutex_lock,
    [SYSCALL_MUTEX_UNLOCK]       = handler_mutex_unlock,
    [SYSCALL_SEG_FAULT]          = handler_seg_fault,
    [SYSCALL_MEM_ALLOC]          = handler_mem_alloc,
    [SYSCALL_MEM_FREE]           = handler_mem_free,
    [SYSCALL_INIT_PROC]          = handler_init_proc,
    [INTERRUPCION_QUANTUM]       = handler_fin_quantum,
    [MEMORIA_EXPANDIDA]          = handler_mem_expandida, 
	[CORRUPCION_MEMORIA]         = handler_mem_corrupta,
    [RESPUESTA_COMPACTACION]     = handler_respuesta_compactacion,
    [INTERRUPCION_DESALOJO]      = handler_interrupcion_desalojo,
    [DESALOJO_COMPACTACION]      = handler_desalojo_compactacion
};


//Por último, tu función orquestadora queda extremadamente simple y segura. Comprueba 
//límites para evitar un Segmentation Fault, busca la función en la tabla mediante el 
//índice y la ejecuta [cite: 450].

int atenderRespuesta(int cliente_fd, t_scheduler_context* ctx, int mensaje_cliente) {
    // 1. Manejo de desconexión
    if (mensaje_cliente == -1) {
        log_warning(ctx->logger, "El cliente se desconectó.");
        return -1; // -1 indica que se debe cortar la conexión
    }

    // 2. Validación de seguridad (Bounds Checking) y función no mapeada
    if (mensaje_cliente < 0 || mensaje_cliente >= 256 || tabla_despacho[mensaje_cliente] == NULL) {
        log_warning(ctx->logger, "No entiendo el mensaje o código de operación: %d", mensaje_cliente);
        return -1; // -1 indica error / cortar conexión
    }

    // 3. ¡Magia! Ejecuta el handler correspondiente en O(1)
    return tabla_despacho[mensaje_cliente](cliente_fd, ctx);
}

void atender_cliente_scheduler(void* arg) {
    t_args_hilo_scheduler* args = (t_args_hilo_scheduler*)arg;
    int cliente_fd = args->cliente_fd;
    t_scheduler_context* ctx = args->ctx;
    free(args);

    while (1) {
        int mensaje_cliente = recibir_operacion(cliente_fd);
        
        // Ejecutamos la respuesta
        int status = atenderRespuesta(cliente_fd, ctx, mensaje_cliente);
        
        // Si la tabla o la validación retornan -1, rompemos el ciclo y cerramos.
        if (status == -1) {
            close(cliente_fd);
            break;
        }
    }
}

void limpiar_contexto(t_scheduler_context* ctx) {
    if (ctx->socket_escucha != -1) close(ctx->socket_escucha);
    if (ctx->socket_memoria != -1) close(ctx->socket_memoria);

    if (ctx->estratega != NULL) destruir_estratega_planificacion(ctx->estratega);
    if (ctx->config != NULL) config_destroy(ctx->config);
    if (ctx->logger != NULL) log_destroy(ctx->logger);

    free(ctx);
}