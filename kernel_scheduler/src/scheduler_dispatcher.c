#include "scheduler_dispatcher.h"
#include "planificador_memoria.h"

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

        t_proceso_planificable* pcb_a_destruir = ctx->estratega->proceso_exec;
        planificador_liberar_exec(ctx->estratega, ctx->estratega->proceso_exec->pid);
        pcb_destruir(pcb_a_destruir);
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

void remover_cpu_de_lista(t_estratega_planificacion* estratega, int fd) {
    if (estratega->sockets_cpu == NULL) return;
    for (int i = 0; i < list_size(estratega->sockets_cpu); i++) {
        int* fd_ptr = list_get(estratega->sockets_cpu, i);
        if (fd_ptr != NULL && *fd_ptr == fd) {
            list_remove_and_destroy_element(estratega->sockets_cpu, i, free);
            break;
        }
    }
}

int atenderRespuesta(int cliente_fd, t_scheduler_context* ctx, int mensaje_cliente) {
    if (mensaje_cliente == -1) {
        log_warning(ctx->logger, "El cliente se desconectó.");
        return -1;
    }

    if (mensaje_cliente < 0 || mensaje_cliente >= 256 || tabla_despacho[mensaje_cliente] == NULL) {
        log_warning(ctx->logger, "No entiendo el mensaje o código de operación: %d", mensaje_cliente);
        return -1;
    }

    return tabla_despacho[mensaje_cliente](cliente_fd, ctx);
}

void atender_cliente_scheduler(void* arg) {
    t_args_hilo_scheduler* args = (t_args_hilo_scheduler*)arg;
    int cliente_fd = args->cliente_fd;
    t_scheduler_context* ctx = args->ctx;
    free(args);

    while (1) {
        int mensaje_cliente = recibir_operacion(cliente_fd);
        
        int status = atenderRespuesta(cliente_fd, ctx, mensaje_cliente);
        
        if (status == -1) {
            remover_cpu_de_lista(ctx->estratega, cliente_fd);
            close(cliente_fd);
            break;
        }
    }
}