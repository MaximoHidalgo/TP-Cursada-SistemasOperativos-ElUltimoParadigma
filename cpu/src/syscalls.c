#include "syscalls.h"

void preparar_syscall(t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
}

int syscall_mutex_create(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MUTEX_CREATE. Desalojando a Kernel...");
    preparar_syscall(contexto, ctx);
    
    notificar_syscall_recurso_a_scheduler(SYSCALL_MUTEX_CREATE, contexto->pid, instruccion->parametro1, ctx);
    return 1;
}

int syscall_mutex_lock(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MUTEX_LOCK. Desalojando a Kernel...");
    preparar_syscall(contexto, ctx);
    
    notificar_syscall_recurso_a_scheduler(SYSCALL_MUTEX_LOCK, contexto->pid, instruccion->parametro1, ctx);
    return 1;
}

int syscall_mutex_unlock(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MUTEX_UNLOCK. Desalojando a Kernel...");
    preparar_syscall(contexto, ctx);
    
    notificar_syscall_recurso_a_scheduler(SYSCALL_MUTEX_UNLOCK, contexto->pid, instruccion->parametro1, ctx);
    return 1;
}

int syscall_mem_alloc(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MEM_ALLOC. Desalojando a Kernel...");
    preparar_syscall(contexto, ctx);
    
    uint32_t id_seg = atoi(instruccion->parametro1);
    uint32_t tamanio = atoi(instruccion->parametro2);
    notificar_syscall_memoria_a_scheduler(SYSCALL_MEM_ALLOC, contexto->pid, id_seg, tamanio, ctx);
    
    return 1;
}

int syscall_mem_free(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MEM_FREE. Desalojando a Kernel...");
    preparar_syscall(contexto, ctx);
    
    uint32_t id_seg = atoi(instruccion->parametro1);
    enviar_operacion_con_pid_y_parametro(SYSCALL_MEM_FREE, contexto->pid, id_seg, ctx->conexiones->scheduler);
    
    return 1;
}

int syscall_sleep(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall SLEEP. Desalojando a Kernel...");
    
    preparar_syscall(contexto, ctx);
    
    uint32_t tiempo_sleep = atoi(instruccion->parametro1);
    notificar_syscall_sleep_a_scheduler(contexto->pid, tiempo_sleep, ctx);
    
    return 1; // Desalojar
}

int syscall_stdout(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall STDOUT. Desalojando a Kernel...");
    preparar_syscall(contexto, ctx);
    
    uint32_t dir_logica = leer_registro(contexto, instruccion->parametro1);
    uint32_t tamanio = leer_registro(contexto, instruccion->parametro2);

    uint32_t dir_fisica = 0;
    if (mmu_traducir(ctx, contexto, dir_logica, tamanio, ctx->tam_max_segmento, &dir_fisica) == -1) {
        log_error(ctx->logger, "Segmentation Fault en STDOUT logica %d", dir_logica);
        return desalojar_proceso(contexto, ctx, SYSCALL_SEG_FAULT);
    } else {
        notificar_syscall_memoria_a_scheduler(SYSCALL_STDOUT, contexto->pid, dir_fisica, tamanio, ctx);
        return 1;
    }
}

int syscall_stdin(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall STDIN. Desalojando a Kernel...");
    preparar_syscall(contexto, ctx);
    
    uint32_t dir_logica = leer_registro(contexto, instruccion->parametro1);
    uint32_t tamanio = leer_registro(contexto, instruccion->parametro2);
    
    uint32_t dir_fisica = 0;
    if (mmu_traducir(ctx, contexto, dir_logica, tamanio, ctx->tam_max_segmento, &dir_fisica) == -1) {
        log_error(ctx->logger, "Segmentation Fault en STDIN logica %d", dir_logica);
        return desalojar_proceso(contexto, ctx, SYSCALL_SEG_FAULT);
    } else {
        notificar_syscall_memoria_a_scheduler(SYSCALL_STDIN, contexto->pid, dir_fisica, tamanio, ctx);
        return 1;
    }
}

int syscall_init_proc(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall INIT_PROC. Desalojando a Kernel...");
    preparar_syscall(contexto, ctx);
    
    char* script_path = instruccion->parametro1;
    uint32_t prioridad = atoi(instruccion->parametro2);
    notificar_syscall_init_proc_a_scheduler(contexto->pid, script_path, prioridad, ctx);
    
    return 1;
}

int syscall_exit(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall EXIT. Desalojando a Kernel...");
    contexto->registros.PC++;
    return desalojar_proceso(contexto, ctx, SYSCALL_EXIT);
}