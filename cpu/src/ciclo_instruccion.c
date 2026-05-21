#include "ciclo_instruccion.h"

void ejecutar_ciclo_de_instruccion(t_contexto_ejecucion* contexto, int socket_memoria, int socket_scheduler, t_log* logger) {
    int desalojar = 0;

    while (!desalojar) {

        /*1. FETCH
        la etapa de Fetch consiste en buscar la próxima instrucción a ejecutar pidiéndosela 
        al módulo Kernel Memory utilizando el Program Counter (PC) relativo al proceso.
        Una vez que el Fetch termina y la CPU ya tiene el texto de la instrucción guardado 
        en una variable, el ciclo debe avanzar naturalmente a la segunda etapa: el Decode*/ 
        char* instruccion_texto = fetch(contexto, socket_memoria, logger);

        /*2. DECODE
        Interpretamos qué instrucción es y si requiere traducir direcciones.
        Por ejemplo, separar "SET AX 6" en un array de strings ["SET", "AX", "6"]*/
        t_instruccion* instruccion_decodificada = decode(instruccion_texto);
        
        /*3. EXECUTE
        Ejecutamos la instrucción (ej: SUMAR, ASIGNAR) [4, 5].
        IMPORTANTE: Al finalizar el ciclo de instrucción, el Program Counter (PC) 
        deberá ser actualizado sumándole 1, siempre y cuando este no haya sido 
        modificado por la instrucción ejecutada anteriormente (ej: JNZ) [6].*/
        int desalojar = execute(instruccion_decodificada, contexto, socket_scheduler, socket_memoria, logger);
        

        //Incrementamos el PC solo si no hubo desalojo y la instrucción NO fue un JNZ
        if (!desalojar && !string_equals_ignore_case(instruccion_decodificada->identificador, "JNZ"))
        {
            contexto->registros.PC++;
        }

        /* 4. CHECK INTERRUPT
        Chequeamos de forma no bloqueante si el Scheduler nos envió una interrupción al PID actual.
        Si hay interrupción, se actualiza el contexto de ejecución en el Kernel Memory 
        y se le devuelve el PID al Kernel Scheduler con el motivo de la interrupción*/
        desalojar = chequear_interrupciones(socket_scheduler, logger);
        if (desalojar) {
            log_info(logger, "## Interrupción recibida");
            enviar_contexto_a_memoria(contexto, socket_memoria);
            notificar_desalojo_a_scheduler(contexto->pid, INTERRUPCION_QUANTUM, socket_scheduler, logger);
        }

        // Limpiamos memoria (y también liberar la instrucción y sus parámetros)
        free(instruccion_texto);
        free(instruccion_decodificada->identificador);
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