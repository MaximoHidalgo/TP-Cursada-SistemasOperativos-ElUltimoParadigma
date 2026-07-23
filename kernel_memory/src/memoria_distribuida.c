#include "memoria_distribuida.h"

void* leer_memoria_distribuida(t_memory_context* ctx, uint32_t dir_fisica, uint32_t tamanio) {
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
            log_error(ctx->logger, "Error: Direccion fisica fuera de los limites de la memoria distribuida");
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
            // El Memory Stick usa agregar_a_paquete, que prefija el payload con un int de tamanio.
            // Asi que copiamos saltando ese int (res + sizeof(int)).
            memcpy(buffer + leido, res + sizeof(int), bytes_a_leer);
            free(res);
        } else {
            log_error(ctx->logger, "Error al leer del memory stick");
            break;
        }
        
        leido += bytes_a_leer;
    }
    
    return buffer;
}

void escribir_memoria_distribuida(t_memory_context* ctx, uint32_t dir_fisica, uint32_t tamanio, void* buffer) {
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
            log_error(ctx->logger, "Error: Direccion fisica fuera de los limites de la memoria distribuida");
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
            log_error(ctx->logger, "Error al escribir en memory stick");
            break;
        }
        
        escrito += bytes_a_escribir;
    }
}

void serializar_tabla_sticks_en_paquete(t_paquete* paq, t_list* memory_sticks) {
    int cant = list_size(memory_sticks);
    agregar_a_paquete(paq, &cant, sizeof(int));
    for (int i = 0; i < cant; i++) {
        t_memory_stick_info* stick = list_get(memory_sticks, i);
        agregar_a_paquete(paq, &(stick->id), sizeof(int));
        agregar_a_paquete(paq, &(stick->tamanio), sizeof(uint32_t));
        agregar_a_paquete(paq, &(stick->base_address), sizeof(uint32_t));
        agregar_a_paquete(paq, &(stick->puerto), sizeof(int));
        int ip_len = strlen(stick->ip) + 1;
        agregar_a_paquete(paq, &ip_len, sizeof(int));
        agregar_a_paquete(paq, stick->ip, ip_len);
    }
}