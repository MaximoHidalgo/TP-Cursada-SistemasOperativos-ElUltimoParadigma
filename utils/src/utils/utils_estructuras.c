#include "utils_estructuras.h"

t_contexto_ejecucion* pedir_contexto_a_memoria(int pid_recibido, int conexion_memory, t_log* logger) {
    t_paquete* paquete_solicitud = crear_paquete();
    paquete_solicitud->codigo_operacion = SOLICITUD_CONTEXTO; 
    
    //Agregamos el PID al paquete y lo enviamos
    agregar_a_paquete(paquete_solicitud, &pid_recibido, sizeof(int));
    enviar_paquete(paquete_solicitud, conexion_memory);
    eliminar_paquete(paquete_solicitud);

    log_info(logger, "Solicitando contexto del PID %d a Kernel Memory...", pid_recibido);

    //Quedarnos esperando la respuesta de la Memoria
    int operacion = recibir_operacion(conexion_memory);

    if (operacion == ENVIO_CONTEXTO) {
        int size;
        void* buffer = recibir_buffer(&size, conexion_memory);

        //Deserializar el buffer recibido para armar el struct localmente
        t_contexto_ejecucion* contexto = malloc(sizeof(t_contexto_ejecucion));
        int desplazamiento = 0;
        // Extraemos el PID
        memcpy(&(contexto->pid), buffer + desplazamiento, sizeof(uint32_t));
        desplazamiento += sizeof(uint32_t);
        // Extraemos los registros de la CPU (PC, AX, BX, etc.) de un solo golpe
        memcpy(&(contexto->registros), buffer + desplazamiento, sizeof(t_registros));
        desplazamiento += sizeof(t_registros);


        // (Más adelante en el Check 3 acá también deserializarás la Tabla de Segmentos)
        free(buffer);


        log_info(logger, "Contexto de ejecución del PID %d recibido exitosamente.", contexto->pid);
        return contexto;
    } else {
        log_error(logger, "Error: Se esperaba el contexto de ejecución, pero llegó la operación: %d", operacion);
        return NULL;
    }
}

void enviar_contexto_a_memoria(t_contexto_ejecucion* contexto, int conexion_memory) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ACTUALIZACION_CONTEXTO; 

    // Serializamos el contexto de ejecución para enviarlo a Memoria
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros), sizeof(t_registros));
    // (Más adelante en el Check 3 acá también agregarás la Tabla de Segmentos al paquete)

    enviar_paquete(paquete, conexion_memory);
    eliminar_paquete(paquete);
}

char* fetch(t_contexto_ejecucion* contexto, int conexion_memory, t_log* logger){
    
    //Imprimir el Log mínimo y obligatorio de la cátedra
    log_info(logger, "## PID: %d - FETCH - Program Counter: %d", contexto->pid, contexto->registros.PC);

    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = FETCH_INSTRUCCION;
    
    agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(contexto->registros.PC), sizeof(uint32_t));
    enviar_paquete(paquete, conexion_memory);
    eliminar_paquete(paquete);

    //Quedarnos esperando la respuesta
    int operacion = recibir_operacion(conexion_memory);

    if (operacion == ENVIO_INSTRUCCION) {
        int size;
        void* buffer = recibir_buffer(&size, conexion_memory);

        // Deserializamos el string. Como es texto, le asignamos la memoria exacta del size recibido 
        char* instruccion_texto = malloc(size);
        memcpy(instruccion_texto, buffer, size);
        free(buffer);
        return instruccion_texto;
    } else {
        log_error(logger, "Error: Se esperaba una instrucción de memoria, pero llegó: %d", operacion);
        return NULL;
    }
}

t_instruccion* decode(char* instruccion_texto) {
    t_instruccion* instruccion = malloc(sizeof(t_instruccion));
    instruccion->parametro1 = NULL;
    instruccion->parametro2 = NULL;
    instruccion->parametro3 = NULL;

    char** instruccion_separada = string_split(instruccion_texto, " ");

    // El primer string siempre es el comando a ejecutar
    instruccion->identificador = string_duplicate(instruccion_separada[0]);

    //HAY QUE REVISAR SI NO ESTA MAL PUESTO LOS NUMEROS DE LOS PARAMETROS ENTRE CORCHETES
    //SI ES instruccion_separada[1] O ES instruccion_separada[2] LA PRIMERA Y ASI PARA LAS SIGUIENTE (1,2,3) O (2,3,4)
    // Revisamos cuántos parámetros vinieron y los guardamos
    if (instruccion_separada[1] != NULL) {
    instruccion->parametro1 = string_duplicate(instruccion_separada[1]);
        if (instruccion_separada[2] != NULL) {
        instruccion->parametro2 = string_duplicate(instruccion_separada[2]);
            if (instruccion_separada[3] != NULL) {
                instruccion->parametro3 = string_duplicate(instruccion_separada[3]);
            }
        }
    }

    // Liberamos el array de strings que generó las Commons para evitar memory leaks
    string_array_destroy(instruccion_separada);

    return instruccion;
}

int execute(t_instruccion* instruccion, t_contexto_ejecucion* contexto, int socket_scheduler, int socket_memoria, t_log* logger) {
    int desalojar = 0;

    // 1. Log mínimo y obligatorio de ejecución
    log_info(logger, "## PID: %d - Ejecutando: %s - %s %s %s", 
             contexto->pid, 
             instruccion->identificador, 
             instruccion->parametro1 ? instruccion->parametro1 : "", 
             instruccion->parametro2 ? instruccion->parametro2 : "", 
             instruccion->parametro3 ? instruccion->parametro3 : "");

    // 2. Comparamos usando la función de las Commons
    if (string_equals_ignore_case(instruccion->identificador, "SET")) {
        uint32_t valor = atoi(instruccion->parametro2);
        asignar_valor_a_registro(contexto, instruccion->parametro1, valor);
    } 
    else if (string_equals_ignore_case(instruccion->identificador, "SUM")) {
        // TODO: sumar_registros(contexto, instruccion->parametro1, instruccion->parametro2);
    }
    else if (string_equals_ignore_case(instruccion->identificador, "JNZ")) {
        // TODO: ejecutar_jnz(contexto, instruccion->parametro1, atoi(instruccion->parametro2));
    }
    
    // --- ZONA DE SYSCALLS ---
    else if (string_equals_ignore_case(instruccion->identificador, "SLEEP")) {
        log_info(logger, "Atrapada Syscall SLEEP. Desalojando a Kernel...");
        
        // 1. Actualizamos contexto en memoria
        enviar_contexto_a_memoria(contexto, socket_memoria);
        
        // 2. Le avisamos al Scheduler el motivo y el tiempo
        t_paquete* paquete_sleep = crear_paquete();
        paquete_sleep->codigo_operacion = SYSCALL_SLEEP;
        uint32_t tiempo_sleep = atoi(instruccion->parametro1);
        
        agregar_a_paquete(paquete_sleep, &(contexto->pid), sizeof(uint32_t));
        agregar_a_paquete(paquete_sleep, &tiempo_sleep, sizeof(uint32_t));
        enviar_paquete(paquete_sleep, socket_scheduler);
        eliminar_paquete(paquete_sleep);
        
        desalojar = 1; // Cortamos el ciclo
    }
    else if (string_equals_ignore_case(instruccion->identificador, "EXIT")) {
        log_info(logger, "Atrapada Syscall EXIT. Desalojando a Kernel...");
        
        // 1. Actualizamos contexto en memoria
        enviar_contexto_a_memoria(contexto, socket_memoria);
        
        // 2. Le avisamos al Scheduler que el proceso terminó
        t_paquete* paquete_exit = crear_paquete();
        paquete_exit->codigo_operacion = SYSCALL_EXIT;
        
        agregar_a_paquete(paquete_exit, &(contexto->pid), sizeof(uint32_t));
        enviar_paquete(paquete_exit, socket_scheduler);
        eliminar_paquete(paquete_exit);
        
        desalojar = 1; // Cortamos el ciclo
    }
    else {
        log_error(logger, "Instrucción desconocida: %s", instruccion->identificador);
        desalojar = 1;
    }

    return desalojar;
}

void asignar_valor_a_registro(t_contexto_ejecucion* contexto, char* nombre_registro, uint32_t valor) {
    if (string_equals_ignore_case(nombre_registro, "AX")) {
        contexto->registros.AX = (uint8_t)valor; // Casteamos porque AX es de 1 byte
    } else if (string_equals_ignore_case(nombre_registro, "EAX")) {
        contexto->registros.EAX = valor; // 4 bytes
    }
    // ... repetir para BX, CX, DX, SI, DI, etc.
}

int chequear_interrupciones(int socket_scheduler, t_log* logger) {
    int op_code_espiado;
    
    // Miramos si hay datos en el socket de forma no bloqueante
    ssize_t bytes_recibidos = recv(socket_scheduler, &op_code_espiado, sizeof(int), MSG_PEEK | MSG_DONTWAIT);

    if (bytes_recibidos > 0) {
        int operacion = recibir_operacion(socket_scheduler);
        
        if (operacion == INTERRUPCION_QUANTUM) { 
            // (Acá podés recibir el buffer si el Scheduler te mandó un payload adicional)
            log_info(logger, "## Interrupción recibida");
            return 1;
        } else {
            log_warning(logger, "Se recibió un código inesperado durante el ciclo: %d", operacion);
        }
    } else if (bytes_recibidos == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // No hay mensajes pendientes y la CPU sigue ejecutando tranquila.
        return 0;
    } else if (bytes_recibidos == 0) {
        log_error(logger, "El Kernel Scheduler se ha desconectado inesperadamente.");
        return 1; // Frenamos la CPU porque se cayó el servidor
    }
    return 0;
}
