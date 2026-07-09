#include "instrucciones.h"
#include "registros.h"
#include "mmu.h"
#include "ciclo_instruccion.h"
#include <math.h>
// Tabla de despacho
t_instruction INSTRUCTION_SET[] = {
    {"SET", instr_set},
    {"SUM", instr_sum},
    {"SUB", instr_sub},
    {"JNZ", instr_jnz},
    {"MOV_IN", instr_mov_in},
    {"MOV_OUT", instr_mov_out},
    {"COPY_MEM", instr_copy_mem},
    {"SLEEP", instr_sleep},
    {"STDOUT", instr_stdout},
    {"STDIN", instr_stdin},
    {"MUTEX_CREATE", instr_mutex_create},
    {"MUTEX_LOCK", instr_mutex_lock},
    {"MUTEX_UNLOCK", instr_mutex_unlock},
    {"MEM_ALLOC", instr_mem_alloc},
    {"MEM_FREE", instr_mem_free},
    {"INIT_PROC", instr_init_proc},
    {"EXIT", instr_exit},
    {"NOOP", instr_noop},
    {NULL, NULL}
};

t_instruction *instruction_get_by_name(char *name) {
    for (uint8_t i = 0; INSTRUCTION_SET[i].name != NULL; i++) {
        if (string_equals_ignore_case(INSTRUCTION_SET[i].name, name)) {
            return &INSTRUCTION_SET[i];
        }
    }
    return NULL;
}

// -------------------------------------------------------------
// IMPLEMENTACIONES DE HANDLERS
// -------------------------------------------------------------

int instr_set(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    uint32_t valor = atoi(instruccion->parametro2);
    asignar_valor_a_registro(contexto, instruccion->parametro1, valor);
    return 0; // 0 significa no desalojar
}

int instr_sum(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    uint32_t destino_val = leer_registro(contexto, instruccion->parametro1);
    uint32_t origen_val = leer_registro(contexto, instruccion->parametro2);
    asignar_valor_a_registro(contexto, instruccion->parametro1, destino_val + origen_val);
    return 0;
}

int instr_sub(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    uint32_t destino_val = leer_registro(contexto, instruccion->parametro1);
    uint32_t origen_val = leer_registro(contexto, instruccion->parametro2);
    asignar_valor_a_registro(contexto, instruccion->parametro1, destino_val - origen_val);
    return 0;
}

int instr_jnz(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    uint32_t registro_val = leer_registro(contexto, instruccion->parametro1);

    if (registro_val != 0) {
        contexto->registros.PC = atoi(instruccion->parametro2);
    }
    return 0;
}

int instr_mov_in(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    char* reg_datos = instruccion->parametro1;
    int tam = obtener_tamanio_registro(reg_datos);
    uint32_t dir_logica = contexto->registros.SI;
    uint32_t dir_fisica = 0;

    if (mmu_traducir(ctx, contexto, dir_logica, tam, ctx->tam_max_segmento, &dir_fisica) == -1) {
        log_error(ctx->logger, "Segmentation Fault accediendo a dir logica %d", dir_logica);
        enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = SYSCALL_SEG_FAULT;
        agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
        enviar_paquete(paq, ctx->conexiones->scheduler);
        eliminar_paquete(paq);
        return 1; // Desalojar
    } else {
        void* buf = leer_memoria_distribuida_cpu(ctx, dir_fisica, tam);
        if (buf != NULL) {
            uint32_t valor = 0;
            memcpy(&valor, buf, tam);
            free(buf);
            asignar_valor_a_registro(contexto, reg_datos, valor);
            log_info(ctx->logger, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d", contexto->pid, dir_fisica, valor);
        }
        return 0;
    }
}

int instr_mov_out(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    char* reg_datos = instruccion->parametro1;
    int tam = obtener_tamanio_registro(reg_datos);
    uint32_t valor = leer_registro(contexto, reg_datos);
    uint32_t dir_logica = contexto->registros.DI;
    uint32_t dir_fisica = 0;

    if (mmu_traducir(ctx, contexto, dir_logica, tam, ctx->tam_max_segmento, &dir_fisica) == -1) {
        log_error(ctx->logger, "Segmentation Fault escribiendo a dir logica %d", dir_logica);
        enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = SYSCALL_SEG_FAULT;
        agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
        enviar_paquete(paq, ctx->conexiones->scheduler);
        eliminar_paquete(paq);
        return 1; // Desalojar
    } else {
        escribir_memoria_distribuida_cpu(ctx, dir_fisica, tam, &valor);
        log_info(ctx->logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d", contexto->pid, dir_fisica, valor);
        return 0;
    }
}

int instr_copy_mem(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    uint32_t tam = atoi(instruccion->parametro1);
    uint32_t dir_logica_si = contexto->registros.SI;
    uint32_t dir_logica_di = contexto->registros.DI;
    uint32_t dir_fisica_si = 0;
    uint32_t dir_fisica_di = 0;

    if (mmu_traducir(ctx, contexto, dir_logica_si, tam, ctx->tam_max_segmento, &dir_fisica_si) == -1 || 
        mmu_traducir(ctx, contexto, dir_logica_di, tam, ctx->tam_max_segmento, &dir_fisica_di) == -1) {
        log_error(ctx->logger, "Segmentation Fault en COPY_MEM");
        enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = SYSCALL_SEG_FAULT;
        agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
        enviar_paquete(paq, ctx->conexiones->scheduler);
        eliminar_paquete(paq);
        return 1;
    } else {
        void* buf = leer_memoria_distribuida_cpu(ctx, dir_fisica_si, tam);
        if (buf != NULL) {
            escribir_memoria_distribuida_cpu(ctx, dir_fisica_di, tam, buf);
            log_info(ctx->logger, "PID: %d - Acción: COPY_MEM - Origen Fisico: %d - Destino Fisico: %d", contexto->pid, dir_fisica_si, dir_fisica_di);
            free(buf);
        }
        return 0;
    }
}

// --- SYSCALLS ---

int instr_sleep(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall SLEEP. Desalojando a Kernel...");
    
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    t_paquete* paquete_sleep = crear_paquete();
    paquete_sleep->codigo_operacion = SYSCALL_SLEEP;
    uint32_t tiempo_sleep = atoi(instruccion->parametro1);
    
    agregar_a_paquete(paquete_sleep, &(contexto->pid), sizeof(uint32_t));
    agregar_a_paquete(paquete_sleep, &tiempo_sleep, sizeof(uint32_t));
    enviar_paquete(paquete_sleep, ctx->conexiones->scheduler);
    eliminar_paquete(paquete_sleep);
    
    return 1; // Desalojar
}

int instr_stdout(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall STDOUT. Desalojando a Kernel...");
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    uint32_t dir_logica = leer_registro(contexto, instruccion->parametro1);
    uint32_t tamanio = leer_registro(contexto, instruccion->parametro2);

    uint32_t dir_fisica = 0;
    if (mmu_traducir(ctx, contexto, dir_logica, tamanio, ctx->tam_max_segmento, &dir_fisica) == -1) {
        log_error(ctx->logger, "Segmentation Fault en STDOUT logica %d", dir_logica);
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = SYSCALL_SEG_FAULT;
        agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
        enviar_paquete(paq, ctx->conexiones->scheduler);
        eliminar_paquete(paq);
        return 1;
    } else {
        t_paquete* paquete = crear_paquete();
        paquete->codigo_operacion = SYSCALL_STDOUT;
        agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
        agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
        enviar_paquete(paquete, ctx->conexiones->scheduler);
        eliminar_paquete(paquete);
        return 1;
    }
}

int instr_stdin(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall STDIN. Desalojando a Kernel...");
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    uint32_t dir_logica = leer_registro(contexto, instruccion->parametro1);
    uint32_t tamanio = leer_registro(contexto, instruccion->parametro2);
    
    uint32_t dir_fisica = 0;
    if (mmu_traducir(ctx, contexto, dir_logica, tamanio, ctx->tam_max_segmento, &dir_fisica) == -1) {
        log_error(ctx->logger, "Segmentation Fault en STDIN logica %d", dir_logica);
        t_paquete* paq = crear_paquete();
        paq->codigo_operacion = SYSCALL_SEG_FAULT;
        agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
        enviar_paquete(paq, ctx->conexiones->scheduler);
        eliminar_paquete(paq);
        return 1;
    } else {
        t_paquete* paquete = crear_paquete();
        paquete->codigo_operacion = SYSCALL_STDIN;
        agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
        agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
        enviar_paquete(paquete, ctx->conexiones->scheduler);
        eliminar_paquete(paquete);
        return 1;
    }
}

int instr_mutex_create(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MUTEX_CREATE. Desalojando a Kernel...");
    
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SYSCALL_MUTEX_CREATE;
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    
    char* recurso = instruccion->parametro1;
    agregar_a_paquete(paquete, recurso, strlen(recurso) + 1);

    enviar_paquete(paquete, ctx->conexiones->scheduler);
    eliminar_paquete(paquete);
    
    return 1;
}

int instr_mutex_lock(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MUTEX_LOCK. Desalojando a Kernel...");
    
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SYSCALL_MUTEX_LOCK;
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    
    char* recurso = instruccion->parametro1;
    agregar_a_paquete(paquete, recurso, strlen(recurso) + 1);

    enviar_paquete(paquete, ctx->conexiones->scheduler);
    eliminar_paquete(paquete);
    
    return 1;
}

int instr_mutex_unlock(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MUTEX_UNLOCK. Desalojando a Kernel...");
    
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SYSCALL_MUTEX_UNLOCK;
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    
    char* recurso = instruccion->parametro1;
    agregar_a_paquete(paquete, recurso, strlen(recurso) + 1);

    enviar_paquete(paquete, ctx->conexiones->scheduler);
    eliminar_paquete(paquete);
    
    return 1;
}

int instr_mem_alloc(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MEM_ALLOC. Desalojando a Kernel...");
    
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SYSCALL_MEM_ALLOC;
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    
    uint32_t id_seg = atoi(instruccion->parametro1);
    uint32_t tamanio = atoi(instruccion->parametro2);
    
    agregar_a_paquete(paquete, &id_seg, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));

    enviar_paquete(paquete, ctx->conexiones->scheduler);
    eliminar_paquete(paquete);
    
    return 1;
}

int instr_mem_free(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall MEM_FREE. Desalojando a Kernel...");
    
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SYSCALL_MEM_FREE;
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    
    uint32_t id_seg = atoi(instruccion->parametro1);
    
    agregar_a_paquete(paquete, &id_seg, sizeof(uint32_t));

    enviar_paquete(paquete, ctx->conexiones->scheduler);
    eliminar_paquete(paquete);
    
    return 1;
}

int instr_init_proc(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall INIT_PROC. Desalojando a Kernel...");
    
    contexto->registros.PC++;
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SYSCALL_INIT_PROC;
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    
    char* script_path = instruccion->parametro1;
    uint32_t prioridad = atoi(instruccion->parametro2);
    
    agregar_a_paquete(paquete, script_path, strlen(script_path) + 1);
    agregar_a_paquete(paquete, &prioridad, sizeof(uint32_t));

    enviar_paquete(paquete, ctx->conexiones->scheduler);
    eliminar_paquete(paquete);
    
    return 1;
}

int instr_exit(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "Atrapada Syscall EXIT. Desalojando a Kernel...");
    
    // 0. Incrementamos PC (syscall)
    contexto->registros.PC++;
    
    // 1. Actualizamos contexto en memoria
    enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
    
    // 2. Le avisamos al Scheduler que el proceso terminó
    t_paquete* paquete_exit = crear_paquete();
    paquete_exit->codigo_operacion = SYSCALL_EXIT;
    
    agregar_a_paquete(paquete_exit, &(contexto->pid), sizeof(uint32_t));
    enviar_paquete(paquete_exit, ctx->conexiones->scheduler);
    eliminar_paquete(paquete_exit);
    
    return 1;
}

int instr_noop(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    int repeticiones = 1;
    if (instruccion->parametro1 != NULL) {
        repeticiones = atoi(instruccion->parametro1);
    }
    
    usleep(1000 * 100); // 100ms provisorio, podés ajustarlo al RETARDO_INSTRUCCION si luego lo agregás al config
    
    for (int i = 1; i < repeticiones; i++) {
        // Simulamos la ejecución reiterada
        log_info(ctx->logger, "## PID: %d - Ejecutando: NOOP -   ", contexto->pid);
        usleep(1000 * 100); // 100ms de retardo por cada repetición simulada
    }
    
    return 0;
}