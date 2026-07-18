#include "instrucciones.h"

// Tabla de despacho
t_instruction INSTRUCTION_SET[] = {
    {"SET", instr_set},
    {"SUM", instr_sum},
    {"SUB", instr_sub},
    {"JNZ", instr_jnz},
    {"MOV_IN", instr_mov_in},
    {"MOV_OUT", instr_mov_out},
    {"COPY_MEM", instr_copy_mem},
    {"SLEEP", syscall_sleep},
    {"STDOUT", syscall_stdout},
    {"STDIN", syscall_stdin},
    {"MUTEX_CREATE", syscall_mutex_create},
    {"MUTEX_LOCK", syscall_mutex_lock},
    {"MUTEX_UNLOCK", syscall_mutex_unlock},
    {"MEM_ALLOC", syscall_mem_alloc},
    {"MEM_FREE", syscall_mem_free},
    {"INIT_PROC", syscall_init_proc},
    {"EXIT", syscall_exit},
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
// IMPLEMENTACIONES DE HANDLERS DE INSTRUCCIONES
// -------------------------------------------------------------

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
        return desalojar_proceso(contexto, ctx, SYSCALL_SEG_FAULT);
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
        return desalojar_proceso(contexto, ctx, SYSCALL_SEG_FAULT);
    } else {
        escribir_memoria_distribuida_cpu(ctx, dir_fisica, tam, &valor);
        log_info(ctx->logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d", contexto->pid, dir_fisica, valor);
        return 0;
    }
}

int instr_copy_mem(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    uint32_t tam = leer_registro(contexto, instruccion->parametro1);
    uint32_t dir_logica_si = contexto->registros.SI;
    uint32_t dir_logica_di = contexto->registros.DI;
    uint32_t dir_fisica_si = 0;
    uint32_t dir_fisica_di = 0;

    if (mmu_traducir(ctx, contexto, dir_logica_si, tam, ctx->tam_max_segmento, &dir_fisica_si) == -1 || 
        mmu_traducir(ctx, contexto, dir_logica_di, tam, ctx->tam_max_segmento, &dir_fisica_di) == -1) {
        log_error(ctx->logger, "Segmentation Fault en COPY_MEM");
        return desalojar_proceso(contexto, ctx, SYSCALL_SEG_FAULT);
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