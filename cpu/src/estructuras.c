#include "estructuras.h"
#include <math.h>
#include "instrucciones.h"

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

int mmu_traducir(t_cpu_context* ctx, t_contexto_ejecucion* contexto, int dir_logica, int tam_bytes, int tam_max_segmento, uint32_t* dir_fisica) {
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
            // Consumimos el payload del paquete para no dejar basura en el buffer del socket
            int size;
            void* buffer = recibir_buffer(&size, ctx->conexiones->scheduler);
            free(buffer);

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