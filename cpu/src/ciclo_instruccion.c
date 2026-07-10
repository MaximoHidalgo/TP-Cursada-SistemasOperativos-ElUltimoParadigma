#include "ciclo_instruccion.h"

// -------------------------------------------------------------
// PRIVATE FUNCTIONS
// -------------------------------------------------------------

static void destruir_instruccion(t_instruccion* inst) {
    if (inst->identificador) free(inst->identificador);
    if (inst->parametro1) free(inst->parametro1);
    if (inst->parametro2) free(inst->parametro2);
    if (inst->parametro3) free(inst->parametro3);
    free(inst);
}

static char* extraer_instruccion_del_paquete(int socket_memoria) {
    t_list* lista = recibir_paquete(socket_memoria);
    char* instruccion_texto = string_duplicate(list_get(lista, 0));
    list_destroy_and_destroy_elements(lista, free);
    return instruccion_texto;
}

static int manejar_interrupcion_quantum(t_cpu_context* ctx, uint32_t pid_actual) {
    t_list* paquete = recibir_paquete(ctx->conexiones->scheduler);
    uint32_t pid_interrumpido = *(uint32_t*)list_get(paquete, 0);
    list_destroy_and_destroy_elements(paquete, free);

    if (pid_interrumpido == pid_actual) {
        log_info(ctx->logger, "## Interrupción recibida para PID %d", pid_interrumpido);
        return 1;
    } else {
        log_warning(ctx->logger, "Se descartó interrupción del PID %d (el actual es %d)", pid_interrumpido, pid_actual);
        return 0;
    }
}

static int procesar_interrupcion(t_cpu_context* ctx, uint32_t pid_actual) {
    int operacion = recibir_operacion(ctx->conexiones->scheduler);
    
    if (operacion == INTERRUPCION_QUANTUM) { 
        return manejar_interrupcion_quantum(ctx, pid_actual);
    } 
    
    log_warning(ctx->logger, "Se recibió un código inesperado durante el ciclo: %d", operacion);
    return 0;
}

// -------------------------------------------------------------
// CICLO DE INSTRUCCION
// -------------------------------------------------------------

void ejecutar_ciclo_de_instruccion(t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    int desalojar = 0;

    while (!desalojar) {

        /* 1. FETCH */ 
        char* instruccion_texto = fetch(contexto, ctx);
        if (instruccion_texto == NULL) {
            log_error(ctx->logger, "Fetch falló (probablemente Memoria se desconectó). Desalojando PID %d.", contexto->pid);
            break;
        }

        /* 2. DECODE */
        t_instruccion* instruccion_decodificada = decode(instruccion_texto);
        
        /* 3. EXECUTE */
        uint32_t pc_previo = contexto->registros.PC;
        desalojar = execute(instruccion_decodificada, contexto, ctx);
        
        // Incrementamos el PC solo si no hubo desalojo y la instrucción NO modificó el PC
        if (!desalojar && contexto->registros.PC == pc_previo) {
            contexto->registros.PC++;
        }

        /* 4. CHECK INTERRUPT */
        if (!desalojar) {
            int hubo_interrupcion = chequear_interrupciones(ctx, contexto->pid);
            if (hubo_interrupcion) {
                log_info(ctx->logger, "## Interrupción por Fin de Quantum recibida para PID %d", contexto->pid);
                desalojar = 1; // Obligamos a romper el ciclo de instrucción
                
                enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
                notificar_desalojo_a_scheduler(contexto->pid, FIN_DE_QUANTUM, ctx);
            }
        }

        // Limpiamos memoria
        free(instruccion_texto);
        destruir_instruccion(instruccion_decodificada);
    }
}

char* fetch(t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "## PID: %d - FETCH - Program Counter: %d", contexto->pid, contexto->registros.PC);
    
    enviar_peticion_fetch_instruccion(contexto->pid, contexto->registros.PC, ctx->conexiones->memoria);
    
    int op_code = recibir_operacion(ctx->conexiones->memoria);
    
    if (op_code == ENVIO_INSTRUCCION) {
        return extraer_instruccion_del_paquete(ctx->conexiones->memoria);
    } else {
        log_error(ctx->logger, "Error: Se esperaba una instrucción, pero llegó la operación: %d", op_code);
        return NULL;
    }
}

t_instruccion* decode(char* instruccion_texto) {
    t_instruccion* instruccion = malloc(sizeof(t_instruccion));
    
    char** inst_separada = string_split(instruccion_texto, " ");
    int arg_count = string_array_size(inst_separada);
    
    instruccion->identificador = arg_count > 0 ? string_duplicate(inst_separada[0]) : NULL;
    instruccion->parametro1 = arg_count > 1 ? string_duplicate(inst_separada[1]) : NULL;
    instruccion->parametro2 = arg_count > 2 ? string_duplicate(inst_separada[2]) : NULL;
    instruccion->parametro3 = arg_count > 3 ? string_duplicate(inst_separada[3]) : NULL;

    string_array_destroy(inst_separada);
    return instruccion;
}

int execute(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    log_info(ctx->logger, "## PID: %d - Ejecutando: %s - %s %s %s", 
             contexto->pid, 
             instruccion->identificador, 
             instruccion->parametro1 ? instruccion->parametro1 : "", 
             instruccion->parametro2 ? instruccion->parametro2 : "", 
             instruccion->parametro3 ? instruccion->parametro3 : "");

    t_instruction *inst = instruction_get_by_name(instruccion->identificador);
    
    if (inst != NULL) {
        return inst->instr(instruccion, contexto, ctx);
    } else {
        log_error(ctx->logger, "Instrucción desconocida: %s", instruccion->identificador);
        return 1; // Desalojar
    }
}

int chequear_interrupciones(t_cpu_context* ctx, uint32_t pid_actual) {
    int op_code_espiado;
    
    ssize_t bytes_recibidos = recv(ctx->conexiones->scheduler, &op_code_espiado, sizeof(int), MSG_PEEK | MSG_DONTWAIT);

    if (bytes_recibidos == 0) {
        log_error(ctx->logger, "El Kernel Scheduler se ha desconectado inesperadamente.");
        return 1;
    }
    
    if (bytes_recibidos < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; //el socket está vacío, no hay interrupciones.
        } else {
            log_error(ctx->logger, "Error real en el socket de interrupciones.");
            return 1; // Hubo un error de red, forzamos el desalojo.
        }
    }

    return procesar_interrupcion(ctx, pid_actual);
}