#include "ciclo_instruccion.h"

void ejecutar_ciclo_de_instruccion(t_contexto_ejecucion* contexto, int socket_memoria, int socket_scheduler, t_log* logger) {
    int desalojar = 0;

    while (!desalojar) {

        /* 1. FETCH */ 
        char* instruccion_texto = fetch(contexto, socket_memoria, logger);

        /* 2. DECODE */
        t_instruccion* instruccion_decodificada = decode(instruccion_texto);
        
        /* 3. EXECUTE */
        // ERROR ARREGLADO: Ya no creamos una variable nueva, reasignamos la global del while
        desalojar = execute(instruccion_decodificada, contexto, socket_scheduler, socket_memoria, logger);
        
        // Incrementamos el PC solo si no hubo desalojo y la instrucción NO fue un JNZ
        if (!desalojar && !string_equals_ignore_case(instruccion_decodificada->identificador, "JNZ")) {
            contexto->registros.PC++;
        }

        /* 4. CHECK INTERRUPT */
        // Solo chequeamos interrupciones si la instrucción anterior NO nos obligó ya a desalojar 
        // (por ejemplo, si acabamos de hacer un EXIT, ya no importa si justo llegó el fin de quantum)
        if (!desalojar) {
            int hubo_interrupcion = chequear_interrupciones(socket_scheduler, logger);
            if (hubo_interrupcion) {
                log_info(logger, "## Interrupción por Fin de Quantum recibida para PID %d", contexto->pid);
                desalojar = 1; // Obligamos a romper el ciclo de instrucción
                
                // Actualizamos el contexto en memoria antes de avisarle al scheduler
                enviar_contexto_a_memoria(contexto, socket_memoria);
                
                // Le avisamos al Scheduler que lo desalojamos por Fin de Quantum
                notificar_desalojo_a_scheduler(contexto->pid, FIN_DE_QUANTUM, socket_scheduler, logger);
            }
        }

        // Limpiamos memoria (y también liberar la instrucción y sus parámetros)
        free(instruccion_texto);
        if (instruccion_decodificada->identificador) free(instruccion_decodificada->identificador);
        if (instruccion_decodificada->parametro1) free(instruccion_decodificada->parametro1);
        if (instruccion_decodificada->parametro2) free(instruccion_decodificada->parametro2);
        if (instruccion_decodificada->parametro3) free(instruccion_decodificada->parametro3);
        free(instruccion_decodificada);
    }
}

void notificar_desalojo_a_scheduler(uint32_t pid, int motivo_desalojo, int conexion_scheduler, t_log* logger) {
    // 1. Creamos un paquete. Usamos el 'motivo_desalojo' directamente como el op_code
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = motivo_desalojo; 
    
    // 2. Agregamos únicamente el PID al paquete
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    
    // 3. Enviamos el paquete por el socket conectado al Scheduler
    enviar_paquete(paquete, conexion_scheduler);
    
    // 4. Liberamos el paquete para evitar memory leaks
    eliminar_paquete(paquete);

    log_info(logger, "PID %d devuelto al Scheduler. Motivo (op_code): %d", pid, motivo_desalojo);
}

void notificar_syscall_sleep_a_scheduler(uint32_t pid, uint32_t tiempo_sleep, int conexion_scheduler) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = SYSCALL_SLEEP; 
    
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tiempo_sleep, sizeof(uint32_t)); // ¡Dato extra!
    
    enviar_paquete(paquete, conexion_scheduler);
    eliminar_paquete(paquete);
}