#include "scheduler_dispatcher.h"

//int handler_seg_fault(int cliente_fd, t_scheduler_context* ctx) ;

int handler_desalojo_compactacion(int fd, t_scheduler_context* ctx) {
    t_list* paquete = recibir_paquete(fd);
    
    if (ctx->estratega->proceso_exec != NULL) {
        t_proceso_planificable* desalojado = ctx->estratega->proceso_exec;
        desalojado->estado = ESTADO_PROCESO_READY;       
        // El TP exige ponerlo al PRINCIPIO de la cola
        t_cola_planificacion* cola = obtener_cola_planificacion(ctx->estratega->colas, desalojado->cola_prioridad);
        
        list_add_in_index(cola->procesos->elements, 0, desalojado);      
        ctx->estratega->proceso_exec = NULL;
    }
    
    // Ahora que la CPU está libre, LE DAMOS EL OK A Memoria para que compacte
    int op_resp = RESPUESTA_COMPACTACION;
    send(ctx->socket_memoria, &op_resp, sizeof(int), 0);

    
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
        t_proceso_planificable* culpable = NULL;
        for (int i = 0; i < list_size(ctx->estratega->colas); i++) {
            t_cola_planificacion* cola = list_get(ctx->estratega->colas, i);
            if (!queue_is_empty(cola->procesos)) {
                culpable = list_get(cola->procesos->elements, 0);
                break;
            }
        }
        
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
        log_info(ctx->logger, "## (%d) finalizó su ejecución con motivo de SEG_FAULT", ctx->estratega->proceso_exec->pid);
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
    
    if (!list_is_empty(ctx->estratega->sockets_cpu)) {
        for(int i = 0; i < list_size(ctx->estratega->sockets_cpu); i++) {
            int* fd = list_get(ctx->estratega->sockets_cpu, i);
            if(fd != NULL && *fd != -1) {
                despachar_proceso_a_cpu(ctx, *fd);
            }
        }
    }
    return 1;
}

int handler_mem_expandida(int cliente_fd, t_scheduler_context* ctx){

    // Registramos que memoria tiene más espacio y luego:
    // 1. Simplemente logeamos el evento para cumplir y dejar rastro
    log_info(ctx->logger, "Memoria expandida: Se detectó un nuevo Memory Stick.");
    // 2. Disparamos la planificación de mediano plazo
    planificador_intentar_desuspender(ctx->estratega);
    return 1;
}

int handler_mem_corrupta(int cliente_fd, t_scheduler_context* ctx){
    planificador_finalizar_todo_por_bsod(ctx->estratega);
    log_error(ctx->logger, "Finalizando Kernel Scheduler con motivo de Blue Screen of Death (BSOD).");
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
    return 1; 
}



