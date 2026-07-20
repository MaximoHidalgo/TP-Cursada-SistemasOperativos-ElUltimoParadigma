#include "scheduler_syscalls_io.h"

void manejar_io_sleep(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
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

void manejar_io_stdout(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
    uint32_t* dir_fisica_ptr = list_get(paquete, 1);
    uint32_t* tamanio_ptr = list_get(paquete, 2);
    uint32_t dir_fisica = *dir_fisica_ptr;
    uint32_t tamanio = *tamanio_ptr;
    
    t_interfaz_io* interfaz = buscar_interfaz_por_nombre(ctx->estratega, "STDOUT");
    if (interfaz != NULL) {
        // Pedimos leer a memoria
        t_paquete* paq_mem = crear_paquete();
        paq_mem->codigo_operacion = LEER_MEMORIA;
        agregar_a_paquete(paq_mem, &pid, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, &dir_fisica, sizeof(uint32_t));
        agregar_a_paquete(paq_mem, &tamanio, sizeof(uint32_t));
        
        pthread_mutex_lock(&ctx->estratega->mutex_socket_memoria);
        enviar_paquete(paq_mem, ctx->socket_memoria);
        eliminar_paquete(paq_mem);
        
        int op = esperar_respuesta_memoria(ctx->socket_memoria, ctx->estratega);
        if (op == MENSAJE) {
            int size;
            void* buf = recibir_buffer(&size, ctx->socket_memoria);
            pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
            
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
        } else {
            pthread_mutex_unlock(&ctx->estratega->mutex_socket_memoria);
        }
    }
    planificador_atender_peticion_io(ctx->estratega, ctx->estratega->proceso_exec, "STDOUT");
}

void manejar_io_stdin(t_scheduler_context* ctx, t_list* paquete, uint32_t pid) {
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
    
    // Log obligatorio del TP
    log_info(ctx->logger, "## (%d) - Solicitó syscall: %s", pid, obtener_nombre_syscall(tipo_syscall));
    
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