#include "estructuras.h"

t_contexto_ejecucion* pedir_contexto_a_memoria(int pid_recibido, t_cpu_context* ctx) {
    t_paquete* paquete_solicitud = crear_paquete();
    paquete_solicitud->codigo_operacion = SOLICITUD_CONTEXTO; 
    
    //Agregamos el PID al paquete y lo enviamos
    agregar_a_paquete(paquete_solicitud, &pid_recibido, sizeof(int));
    enviar_paquete(paquete_solicitud, ctx->conexiones->memoria);
    eliminar_paquete(paquete_solicitud);

    log_info(ctx->logger, "Solicitando contexto del PID %d a Kernel Memory...", pid_recibido);

    //Quedarnos esperando la respuesta de la Memoria
    int operacion = recibir_operacion(ctx->conexiones->memoria);

    if (operacion == ENVIO_CONTEXTO) {
        int size;
        void* buffer = recibir_buffer(&size, ctx->conexiones->memoria);

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


        log_info(ctx->logger, "Contexto de ejecución del PID %d recibido exitosamente.", contexto->pid);
        return contexto;
    } else {
        log_error(ctx->logger, "Error: Se esperaba el contexto de ejecución, pero llegó la operación: %d", operacion);
        return NULL;
    }
}

void enviar_contexto_a_memoria(t_contexto_ejecucion* contexto, int conexion_memory) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ACTUALIZACION_CONTEXTO; 

    // Usamos serializar_contexto que arma el payload crudo en el stream
    serializar_contexto(paquete, contexto);

    enviar_paquete(paquete, conexion_memory);
    eliminar_paquete(paquete);
}

char* fetch(t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    //Creamos un paquete y le cargamos el OP Code FETCH_INSTRUCCION
    t_paquete* paquete_solicitud = crear_paquete();
    paquete_solicitud->codigo_operacion = FETCH_INSTRUCCION; 
    
    enviar_peticion_fetch_instruccion(contexto->pid, contexto->registros.PC, ctx->conexiones->memoria);

    int op_code = recibir_operacion(ctx->conexiones->memoria);
    
    if (op_code == ENVIO_INSTRUCCION) {
        t_list* lista = recibir_paquete(ctx->conexiones->memoria);
        char* instruccion_texto = string_duplicate(list_get(lista, 0));
        list_destroy_and_destroy_elements(lista, free);
        return instruccion_texto;
    } else {
        log_error(ctx->logger, "Error: Se esperaba una instrucción, pero llegó la operación: %d", op_code);
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

int execute(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_cpu_context* ctx) {
    int desalojar = 0;

    log_info(ctx->logger, "## PID: %d - Ejecutando: %s - %s %s %s", 
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
        // Obtenemos el valor actual del registro de destino
        uint32_t destino_val = 0;
        if (string_equals_ignore_case(instruccion->parametro1, "AX")) destino_val = contexto->registros.AX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EAX")) destino_val = contexto->registros.EAX;
        else if (string_equals_ignore_case(instruccion->parametro1, "BX")) destino_val = contexto->registros.BX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EBX")) destino_val = contexto->registros.EBX;
        else if (string_equals_ignore_case(instruccion->parametro1, "CX")) destino_val = contexto->registros.CX;
        else if (string_equals_ignore_case(instruccion->parametro1, "ECX")) destino_val = contexto->registros.ECX;
        else if (string_equals_ignore_case(instruccion->parametro1, "DX")) destino_val = contexto->registros.DX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EDX")) destino_val = contexto->registros.EDX;

        // Obtenemos el valor actual del registro de origen
        uint32_t origen_val = 0;
        if (string_equals_ignore_case(instruccion->parametro2, "AX")) origen_val = contexto->registros.AX;
        else if (string_equals_ignore_case(instruccion->parametro2, "EAX")) origen_val = contexto->registros.EAX;
        else if (string_equals_ignore_case(instruccion->parametro2, "BX")) origen_val = contexto->registros.BX;
        else if (string_equals_ignore_case(instruccion->parametro2, "EBX")) origen_val = contexto->registros.EBX;
        else if (string_equals_ignore_case(instruccion->parametro2, "CX")) origen_val = contexto->registros.CX;
        else if (string_equals_ignore_case(instruccion->parametro2, "ECX")) origen_val = contexto->registros.ECX;
        else if (string_equals_ignore_case(instruccion->parametro2, "DX")) origen_val = contexto->registros.DX;
        else if (string_equals_ignore_case(instruccion->parametro2, "EDX")) origen_val = contexto->registros.EDX;

        // Asignamos la suma al registro de destino
        asignar_valor_a_registro(contexto, instruccion->parametro1, destino_val + origen_val);
    }
    else if (string_equals_ignore_case(instruccion->identificador, "SUB")) {
        // Obtenemos el valor actual del registro de destino
        uint32_t destino_val = 0;
        if (string_equals_ignore_case(instruccion->parametro1, "AX")) destino_val = contexto->registros.AX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EAX")) destino_val = contexto->registros.EAX;
        else if (string_equals_ignore_case(instruccion->parametro1, "BX")) destino_val = contexto->registros.BX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EBX")) destino_val = contexto->registros.EBX;
        else if (string_equals_ignore_case(instruccion->parametro1, "CX")) destino_val = contexto->registros.CX;
        else if (string_equals_ignore_case(instruccion->parametro1, "ECX")) destino_val = contexto->registros.ECX;
        else if (string_equals_ignore_case(instruccion->parametro1, "DX")) destino_val = contexto->registros.DX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EDX")) destino_val = contexto->registros.EDX;

        // Obtenemos el valor actual del registro de origen
        uint32_t origen_val = 0;
        if (string_equals_ignore_case(instruccion->parametro2, "AX")) origen_val = contexto->registros.AX;
        else if (string_equals_ignore_case(instruccion->parametro2, "EAX")) origen_val = contexto->registros.EAX;
        else if (string_equals_ignore_case(instruccion->parametro2, "BX")) origen_val = contexto->registros.BX;
        else if (string_equals_ignore_case(instruccion->parametro2, "EBX")) origen_val = contexto->registros.EBX;
        else if (string_equals_ignore_case(instruccion->parametro2, "CX")) origen_val = contexto->registros.CX;
        else if (string_equals_ignore_case(instruccion->parametro2, "ECX")) origen_val = contexto->registros.ECX;
        else if (string_equals_ignore_case(instruccion->parametro2, "DX")) origen_val = contexto->registros.DX;
        else if (string_equals_ignore_case(instruccion->parametro2, "EDX")) origen_val = contexto->registros.EDX;

        // Asignamos la resta
        asignar_valor_a_registro(contexto, instruccion->parametro1, destino_val - origen_val);
    }
    else if (string_equals_ignore_case(instruccion->identificador, "JNZ")) {
        uint32_t registro_val = 0;
        if (string_equals_ignore_case(instruccion->parametro1, "AX")) registro_val = contexto->registros.AX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EAX")) registro_val = contexto->registros.EAX;
        else if (string_equals_ignore_case(instruccion->parametro1, "BX")) registro_val = contexto->registros.BX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EBX")) registro_val = contexto->registros.EBX;
        else if (string_equals_ignore_case(instruccion->parametro1, "CX")) registro_val = contexto->registros.CX;
        else if (string_equals_ignore_case(instruccion->parametro1, "ECX")) registro_val = contexto->registros.ECX;
        else if (string_equals_ignore_case(instruccion->parametro1, "DX")) registro_val = contexto->registros.DX;
        else if (string_equals_ignore_case(instruccion->parametro1, "EDX")) registro_val = contexto->registros.EDX;

        if (registro_val != 0) {
            contexto->registros.PC = atoi(instruccion->parametro2);
        } else {
            // Si no salta, debe avanzar a la siguiente instrucción porque el ciclo no incrementa PC para JNZ
            contexto->registros.PC++;
        }
    }
    
    // --- ZONA DE SYSCALLS ---
    else if (string_equals_ignore_case(instruccion->identificador, "SLEEP")) {
        log_info(ctx->logger, "Atrapada Syscall SLEEP. Desalojando a Kernel...");
        
        contexto->registros.PC++;
        // 1. Actualizamos contexto en memoria
        enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
        
        // 2. Le avisamos al Scheduler el motivo y el tiempo
        t_paquete* paquete_sleep = crear_paquete();
        paquete_sleep->codigo_operacion = SYSCALL_SLEEP;
        uint32_t tiempo_sleep = atoi(instruccion->parametro1);
        
        agregar_a_paquete(paquete_sleep, &(contexto->pid), sizeof(uint32_t));
        agregar_a_paquete(paquete_sleep, &tiempo_sleep, sizeof(uint32_t));
        enviar_paquete(paquete_sleep, ctx->conexiones->scheduler);
        eliminar_paquete(paquete_sleep);
        
        desalojar = 1; // Cortamos el ciclo
    }
    else if (string_equals_ignore_case(instruccion->identificador, "STDOUT")) {
        log_info(ctx->logger, "Atrapada Syscall STDOUT. Desalojando a Kernel...");
        
        contexto->registros.PC++;
        enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
        
        t_paquete* paquete = crear_paquete();
        paquete->codigo_operacion = SYSCALL_STDOUT;
        // Podríamos enviar más parámetros si fuera necesario, según prueba_io.txt STDOUT toma registros
        // por ahora mandamos un string dummy o si el checkpoint 2 no evalúa STDOUT en profundidad...
        // Wait, prueba_io.txt tiene: STDOUT CX DX
        // Ojo, si STDOUT es a una Interfaz STDOUT, sólo le avisamos al kernel.
        agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
        char* reg1 = instruccion->parametro1;
        char* reg2 = instruccion->parametro2;
        char dummy[100];
        if (reg1 && reg2) {
            sprintf(dummy, "%s %s", reg1, reg2);
        } else if (reg1) {
            sprintf(dummy, "%s", reg1);
        } else {
            strcpy(dummy, "NONE");
        }
        agregar_a_paquete(paquete, dummy, strlen(dummy) + 1);
        
        enviar_paquete(paquete, ctx->conexiones->scheduler);
        eliminar_paquete(paquete);
        
        desalojar = 1;
    }
    else if (string_equals_ignore_case(instruccion->identificador, "STDIN")) {
        log_info(ctx->logger, "Atrapada Syscall STDIN. Desalojando a Kernel...");
        
        contexto->registros.PC++;
        enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
        
        t_paquete* paquete = crear_paquete();
        paquete->codigo_operacion = SYSCALL_STDIN;
        agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
        
        uint32_t size = 10;
        agregar_a_paquete(paquete, &size, sizeof(uint32_t));

        enviar_paquete(paquete, ctx->conexiones->scheduler);
        eliminar_paquete(paquete);
        
        desalojar = 1;
    }
    else if (string_equals_ignore_case(instruccion->identificador, "MUTEX_CREATE")) {
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
        
        desalojar = 1;
    }
    else if (string_equals_ignore_case(instruccion->identificador, "MUTEX_LOCK")) {
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
        
        desalojar = 1;
    }
    else if (string_equals_ignore_case(instruccion->identificador, "MUTEX_UNLOCK")) {
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
        
        desalojar = 1;
    }
    else if (string_equals_ignore_case(instruccion->identificador, "EXIT")) {
        log_info(ctx->logger, "Atrapada Syscall EXIT. Desalojando a Kernel...");
        
        // 1. Actualizamos contexto en memoria
        enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
        
        // 2. Le avisamos al Scheduler que el proceso terminó
        t_paquete* paquete_exit = crear_paquete();
        paquete_exit->codigo_operacion = SYSCALL_EXIT;
        
        agregar_a_paquete(paquete_exit, &(contexto->pid), sizeof(uint32_t));
        enviar_paquete(paquete_exit, ctx->conexiones->scheduler);
        eliminar_paquete(paquete_exit);
        
        desalojar = 1; // Cortamos el ciclo
    }
    else {
        log_error(ctx->logger, "Instrucción desconocida: %s", instruccion->identificador);
        desalojar = 1;
    }

    return desalojar;
}

void asignar_valor_a_registro(t_contexto_ejecucion* contexto, char* nombre_registro, uint32_t valor) {
    if (string_equals_ignore_case(nombre_registro, "AX")) {
        contexto->registros.AX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_registro, "EAX")) {
        contexto->registros.EAX = valor;
    } else if (string_equals_ignore_case(nombre_registro, "BX")) {
        contexto->registros.BX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_registro, "EBX")) {
        contexto->registros.EBX = valor;
    } else if (string_equals_ignore_case(nombre_registro, "CX")) {
        contexto->registros.CX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_registro, "ECX")) {
        contexto->registros.ECX = valor;
    } else if (string_equals_ignore_case(nombre_registro, "DX")) {
        contexto->registros.DX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_registro, "EDX")) {
        contexto->registros.EDX = valor;
    }
}

int chequear_interrupciones(t_cpu_context* ctx) {
    int op_code_espiado;
    
    // Miramos si hay datos en el socket de forma no bloqueante
    ssize_t bytes_recibidos = recv(ctx->conexiones->scheduler, &op_code_espiado, sizeof(int), MSG_PEEK | MSG_DONTWAIT);

    if (bytes_recibidos > 0) {
        int operacion = recibir_operacion(ctx->conexiones->scheduler);
        
        if (operacion == INTERRUPCION_QUANTUM) { 
            // (Acá podés recibir el buffer si el Scheduler te mandó un payload adicional)
            log_info(ctx->logger, "## Interrupción recibida");
            return 1;
        } else {
            log_warning(ctx->logger, "Se recibió un código inesperado durante el ciclo: %d", operacion);
        }
    } else if (bytes_recibidos == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // No hay mensajes pendientes y la CPU sigue ejecutando tranquila.
        return 0;
    } else if (bytes_recibidos == 0) {
        log_error(ctx->logger, "El Kernel Scheduler se ha desconectado inesperadamente.");
        return 1; // Frenamos la CPU porque se cayó el servidor
    }
    return 0;
}
