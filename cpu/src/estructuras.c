#include "estructuras.h"
#include <math.h>

uint32_t leer_registro(t_contexto_ejecucion* contexto, char* nombre_registro) {
    if (string_equals_ignore_case(nombre_registro, "AX")) return contexto->registros.AX;
    if (string_equals_ignore_case(nombre_registro, "EAX")) return contexto->registros.EAX;
    if (string_equals_ignore_case(nombre_registro, "BX")) return contexto->registros.BX;
    if (string_equals_ignore_case(nombre_registro, "EBX")) return contexto->registros.EBX;
    if (string_equals_ignore_case(nombre_registro, "CX")) return contexto->registros.CX;
    if (string_equals_ignore_case(nombre_registro, "ECX")) return contexto->registros.ECX;
    if (string_equals_ignore_case(nombre_registro, "DX")) return contexto->registros.DX;
    if (string_equals_ignore_case(nombre_registro, "EDX")) return contexto->registros.EDX;
    if (string_equals_ignore_case(nombre_registro, "SI")) return contexto->registros.SI;
    if (string_equals_ignore_case(nombre_registro, "DI")) return contexto->registros.DI;
    return 0;
}

int obtener_tamanio_registro(char* nombre_registro) {
    if (string_equals_ignore_case(nombre_registro, "AX") || string_equals_ignore_case(nombre_registro, "BX") || 
        string_equals_ignore_case(nombre_registro, "CX") || string_equals_ignore_case(nombre_registro, "DX")) return 1;
    if (string_equals_ignore_case(nombre_registro, "EAX") || string_equals_ignore_case(nombre_registro, "EBX") || 
        string_equals_ignore_case(nombre_registro, "ECX") || string_equals_ignore_case(nombre_registro, "EDX") ||
        string_equals_ignore_case(nombre_registro, "SI") || string_equals_ignore_case(nombre_registro, "DI")) return 4;
    return 0;
}

int mmu_traducir(t_contexto_ejecucion* contexto, int dir_logica, int tam_bytes, int tam_max_segmento, uint32_t* dir_fisica) {
    int num_segmento = floor((double)dir_logica / tam_max_segmento);
    int desplazamiento = dir_logica % tam_max_segmento;

    t_segmento* seg = NULL;
    if (contexto->tabla_segmentos != NULL) {
        for (int i = 0; i < list_size(contexto->tabla_segmentos); i++) {
            t_segmento* s = list_get(contexto->tabla_segmentos, i);
            if (s->id == num_segmento) {
                seg = s;
                break;
            }
        }
    }

    if (!seg) return -1; // SEG FAULT

    if (desplazamiento + tam_bytes > seg->tamanio) {
        return -1; // SEG FAULT
    }

    *dir_fisica = seg->base + desplazamiento;
    return 0;
}

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
        deserializar_contexto(buffer, size, contexto);
        free(buffer);

        int cant = contexto->tabla_segmentos ? list_size(contexto->tabla_segmentos) : 0;
        log_info(ctx->logger, "Contexto de ejecución del PID %d recibido exitosamente con %d segmentos.", contexto->pid, cant);
        for(int i=0; i<cant; i++) {
            t_segmento* s = list_get(contexto->tabla_segmentos, i);
            log_info(ctx->logger, "  -> Seg ID: %d, Base: %d, Tamanio: %d", s->id, s->base, s->tamanio);
        }

        return contexto;
    } else {
        log_error(ctx->logger, "Error: Se esperaba el contexto de ejecución, pero llegó la operación: %d", operacion);
        return NULL;
    }
}

void enviar_contexto_a_memoria(t_contexto_ejecucion* contexto, int conexion_memory) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ACTUALIZACION_CONTEXTO;
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
    else if (string_equals_ignore_case(instruccion->identificador, "MOV_IN")) {
        char* reg_datos = instruccion->parametro1;
        int tam = obtener_tamanio_registro(reg_datos);
        uint32_t dir_logica = contexto->registros.SI;
        uint32_t dir_fisica = 0;

        if (mmu_traducir(contexto, dir_logica, tam, ctx->tam_max_segmento, &dir_fisica) == -1) {
            log_error(ctx->logger, "Segmentation Fault accediendo a dir logica %d", dir_logica);
            enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
            t_paquete* paq = crear_paquete();
            paq->codigo_operacion = SYSCALL_SEG_FAULT;
            agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
            enviar_paquete(paq, ctx->conexiones->scheduler);
            eliminar_paquete(paq);
            desalojar = 1;
        } else {
            void* buf = leer_memoria_distribuida_cpu(ctx, dir_fisica, tam);
            if (buf != NULL) {
                uint32_t valor = 0;
                memcpy(&valor, buf, tam);
                free(buf);
                asignar_valor_a_registro(contexto, reg_datos, valor);
                log_info(ctx->logger, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d", contexto->pid, dir_fisica, valor);
            }
        }
    }
    else if (string_equals_ignore_case(instruccion->identificador, "MOV_OUT")) {
        char* reg_datos = instruccion->parametro1;
        int tam = obtener_tamanio_registro(reg_datos);
        uint32_t valor = leer_registro(contexto, reg_datos);
        uint32_t dir_logica = contexto->registros.DI;
        uint32_t dir_fisica = 0;

        if (mmu_traducir(contexto, dir_logica, tam, ctx->tam_max_segmento, &dir_fisica) == -1) {
            log_error(ctx->logger, "Segmentation Fault escribiendo a dir logica %d", dir_logica);
            enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
            t_paquete* paq = crear_paquete();
            paq->codigo_operacion = SYSCALL_SEG_FAULT;
            agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
            enviar_paquete(paq, ctx->conexiones->scheduler);
            eliminar_paquete(paq);
            desalojar = 1;
        } else {
            escribir_memoria_distribuida_cpu(ctx, dir_fisica, tam, &valor);
            log_info(ctx->logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d", contexto->pid, dir_fisica, valor);
        }
    }
    else if (string_equals_ignore_case(instruccion->identificador, "COPY_MEM")) {
        uint32_t tam = atoi(instruccion->parametro1);
        uint32_t dir_logica_si = contexto->registros.SI;
        uint32_t dir_logica_di = contexto->registros.DI;
        uint32_t dir_fisica_si = 0;
        uint32_t dir_fisica_di = 0;

        if (mmu_traducir(contexto, dir_logica_si, tam, ctx->tam_max_segmento, &dir_fisica_si) == -1 || 
            mmu_traducir(contexto, dir_logica_di, tam, ctx->tam_max_segmento, &dir_fisica_di) == -1) {
            log_error(ctx->logger, "Segmentation Fault en COPY_MEM");
            enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
            t_paquete* paq = crear_paquete();
            paq->codigo_operacion = SYSCALL_SEG_FAULT;
            agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
            enviar_paquete(paq, ctx->conexiones->scheduler);
            eliminar_paquete(paq);
            desalojar = 1;
        } else {
            void* buf = leer_memoria_distribuida_cpu(ctx, dir_fisica_si, tam);
            if (buf != NULL) {
                escribir_memoria_distribuida_cpu(ctx, dir_fisica_di, tam, buf);
                log_info(ctx->logger, "PID: %d - Acción: COPY_MEM - Origen Fisico: %d - Destino Fisico: %d", contexto->pid, dir_fisica_si, dir_fisica_di);
                free(buf);
            }
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
        
        uint32_t dir_logica = 0, tamanio = 0;
        char* reg1 = instruccion->parametro1;
        if (string_equals_ignore_case(reg1, "AX")) dir_logica = contexto->registros.AX;
        else if (string_equals_ignore_case(reg1, "EAX")) dir_logica = contexto->registros.EAX;
        else if (string_equals_ignore_case(reg1, "BX")) dir_logica = contexto->registros.BX;
        else if (string_equals_ignore_case(reg1, "EBX")) dir_logica = contexto->registros.EBX;
        else if (string_equals_ignore_case(reg1, "CX")) dir_logica = contexto->registros.CX;
        else if (string_equals_ignore_case(reg1, "ECX")) dir_logica = contexto->registros.ECX;
        else if (string_equals_ignore_case(reg1, "DX")) dir_logica = contexto->registros.DX;
        else if (string_equals_ignore_case(reg1, "EDX")) dir_logica = contexto->registros.EDX;
        else if (string_equals_ignore_case(reg1, "SI")) dir_logica = contexto->registros.SI;
        else if (string_equals_ignore_case(reg1, "DI")) dir_logica = contexto->registros.DI;
        
        char* reg2 = instruccion->parametro2;
        if (string_equals_ignore_case(reg2, "AX")) tamanio = contexto->registros.AX;
        else if (string_equals_ignore_case(reg2, "EAX")) tamanio = contexto->registros.EAX;
        else if (string_equals_ignore_case(reg2, "BX")) tamanio = contexto->registros.BX;
        else if (string_equals_ignore_case(reg2, "EBX")) tamanio = contexto->registros.EBX;
        else if (string_equals_ignore_case(reg2, "CX")) tamanio = contexto->registros.CX;
        else if (string_equals_ignore_case(reg2, "ECX")) tamanio = contexto->registros.ECX;
        else if (string_equals_ignore_case(reg2, "DX")) tamanio = contexto->registros.DX;
        else if (string_equals_ignore_case(reg2, "EDX")) tamanio = contexto->registros.EDX;
        else if (string_equals_ignore_case(reg2, "SI")) tamanio = contexto->registros.SI;
        else if (string_equals_ignore_case(reg2, "DI")) tamanio = contexto->registros.DI;

        uint32_t dir_fisica = 0;
        if (mmu_traducir(contexto, dir_logica, tamanio, ctx->tam_max_segmento, &dir_fisica) == -1) {
            log_error(ctx->logger, "Segmentation Fault en STDOUT logica %d", dir_logica);
            t_paquete* paq = crear_paquete();
            paq->codigo_operacion = SYSCALL_SEG_FAULT;
            agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
            enviar_paquete(paq, ctx->conexiones->scheduler);
            eliminar_paquete(paq);
            desalojar = 1;
        } else {
            t_paquete* paquete = crear_paquete();
            paquete->codigo_operacion = SYSCALL_STDOUT;
            agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
            agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
            enviar_paquete(paquete, ctx->conexiones->scheduler);
            eliminar_paquete(paquete);
            desalojar = 1;
        }
    }
    else if (string_equals_ignore_case(instruccion->identificador, "STDIN")) {
        log_info(ctx->logger, "Atrapada Syscall STDIN. Desalojando a Kernel...");
        contexto->registros.PC++;
        enviar_contexto_a_memoria(contexto, ctx->conexiones->memoria);
        
        uint32_t dir_logica = 0, tamanio = 0;
        char* reg1 = instruccion->parametro1;
        if (string_equals_ignore_case(reg1, "AX")) dir_logica = contexto->registros.AX;
        else if (string_equals_ignore_case(reg1, "EAX")) dir_logica = contexto->registros.EAX;
        else if (string_equals_ignore_case(reg1, "BX")) dir_logica = contexto->registros.BX;
        else if (string_equals_ignore_case(reg1, "EBX")) dir_logica = contexto->registros.EBX;
        else if (string_equals_ignore_case(reg1, "CX")) dir_logica = contexto->registros.CX;
        else if (string_equals_ignore_case(reg1, "ECX")) dir_logica = contexto->registros.ECX;
        else if (string_equals_ignore_case(reg1, "DX")) dir_logica = contexto->registros.DX;
        else if (string_equals_ignore_case(reg1, "EDX")) dir_logica = contexto->registros.EDX;
        else if (string_equals_ignore_case(reg1, "SI")) dir_logica = contexto->registros.SI;
        else if (string_equals_ignore_case(reg1, "DI")) dir_logica = contexto->registros.DI;
        
        char* reg2 = instruccion->parametro2;
        if (string_equals_ignore_case(reg2, "AX")) tamanio = contexto->registros.AX;
        else if (string_equals_ignore_case(reg2, "EAX")) tamanio = contexto->registros.EAX;
        else if (string_equals_ignore_case(reg2, "BX")) tamanio = contexto->registros.BX;
        else if (string_equals_ignore_case(reg2, "EBX")) tamanio = contexto->registros.EBX;
        else if (string_equals_ignore_case(reg2, "CX")) tamanio = contexto->registros.CX;
        else if (string_equals_ignore_case(reg2, "ECX")) tamanio = contexto->registros.ECX;
        else if (string_equals_ignore_case(reg2, "DX")) tamanio = contexto->registros.DX;
        else if (string_equals_ignore_case(reg2, "EDX")) tamanio = contexto->registros.EDX;
        else if (string_equals_ignore_case(reg2, "SI")) tamanio = contexto->registros.SI;
        else if (string_equals_ignore_case(reg2, "DI")) tamanio = contexto->registros.DI;
        
        uint32_t dir_fisica = 0;
        if (mmu_traducir(contexto, dir_logica, tamanio, ctx->tam_max_segmento, &dir_fisica) == -1) {
            log_error(ctx->logger, "Segmentation Fault en STDIN logica %d", dir_logica);
            t_paquete* paq = crear_paquete();
            paq->codigo_operacion = SYSCALL_SEG_FAULT;
            agregar_a_paquete(paq, &(contexto->pid), sizeof(uint32_t));
            enviar_paquete(paq, ctx->conexiones->scheduler);
            eliminar_paquete(paq);
            desalojar = 1;
        } else {
            t_paquete* paquete = crear_paquete();
            paquete->codigo_operacion = SYSCALL_STDIN;
            agregar_a_paquete(paquete, &(contexto->pid), sizeof(uint32_t));
            agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
            enviar_paquete(paquete, ctx->conexiones->scheduler);
            eliminar_paquete(paquete);
            desalojar = 1;
        }
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
    else if (string_equals_ignore_case(instruccion->identificador, "MEM_ALLOC")) {
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
        
        desalojar = 1;
    }
    else if (string_equals_ignore_case(instruccion->identificador, "MEM_FREE")) {
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
        
        desalojar = 1;
    }
    else if (string_equals_ignore_case(instruccion->identificador, "INIT_PROC")) {
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
    else if (string_equals_ignore_case(instruccion->identificador, "NOOP")) {
        // No-op: no hace nada, el PC se incrementa normalmente en el ciclo
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

void* leer_memoria_distribuida_cpu(t_cpu_context* ctx, uint32_t dir_fisica, uint32_t tamanio) {
    void* buffer = malloc(tamanio);
    uint32_t leido = 0;
    
    while (leido < tamanio) {
        uint32_t dir_actual = dir_fisica + leido;
        t_memory_stick_info* stick_target = NULL;
        
        for (int i = 0; i < list_size(ctx->memory_sticks); i++) {
            t_memory_stick_info* stick = list_get(ctx->memory_sticks, i);
            if (dir_actual >= stick->base_address && dir_actual < stick->base_address + stick->tamanio) {
                stick_target = stick;
                break;
            }
        }
        
        if (stick_target == NULL) {
            log_error(ctx->logger, "Error: Direccion fisica fuera de los limites de la memoria distribuida (CPU)");
            break;
        }
        
        uint32_t offset_en_stick = dir_actual - stick_target->base_address;
        uint32_t bytes_disponibles = stick_target->tamanio - offset_en_stick;
        uint32_t bytes_a_leer = (tamanio - leido < bytes_disponibles) ? (tamanio - leido) : bytes_disponibles;
        
        enviar_peticion_lectura(offset_en_stick, bytes_a_leer, stick_target->socket_fd);
        
        int r_op = recibir_operacion(stick_target->socket_fd);
        if (r_op == LECTURA) {
            int size_buf;
            void* res = recibir_buffer(&size_buf, stick_target->socket_fd);
            memcpy(buffer + leido, res, bytes_a_leer);
            free(res);
        } else {
            log_error(ctx->logger, "Error al leer del memory stick en CPU");
            break;
        }
        
        leido += bytes_a_leer;
    }
    
    return buffer;
}

void escribir_memoria_distribuida_cpu(t_cpu_context* ctx, uint32_t dir_fisica, uint32_t tamanio, void* buffer) {
    uint32_t escrito = 0;
    
    while (escrito < tamanio) {
        uint32_t dir_actual = dir_fisica + escrito;
        t_memory_stick_info* stick_target = NULL;
        
        for (int i = 0; i < list_size(ctx->memory_sticks); i++) {
            t_memory_stick_info* stick = list_get(ctx->memory_sticks, i);
            if (dir_actual >= stick->base_address && dir_actual < stick->base_address + stick->tamanio) {
                stick_target = stick;
                break;
            }
        }
        
        if (stick_target == NULL) {
            log_error(ctx->logger, "Error: Direccion fisica fuera de los limites de la memoria distribuida (CPU)");
            break;
        }
        
        uint32_t offset_en_stick = dir_actual - stick_target->base_address;
        uint32_t bytes_disponibles = stick_target->tamanio - offset_en_stick;
        uint32_t bytes_a_escribir = (tamanio - escrito < bytes_disponibles) ? (tamanio - escrito) : bytes_disponibles;
        
        enviar_peticion_escritura(offset_en_stick, bytes_a_escribir, buffer + escrito, stick_target->socket_fd);
        
        int r_op = recibir_operacion(stick_target->socket_fd);
        if (r_op == MENSAJE) {
            int size_buf;
            void* res = recibir_buffer(&size_buf, stick_target->socket_fd);
            free(res); // Esperamos MSG_OK
        } else {
            log_error(ctx->logger, "Error al escribir en memory stick en CPU");
            break;
        }
        
        escrito += bytes_a_escribir;
    }
}
