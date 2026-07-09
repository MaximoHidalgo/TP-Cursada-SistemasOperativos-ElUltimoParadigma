#include "mmu.h"
#include <stdlib.h>
#include <string.h>
#include "cpu_utils.h"

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
    return 0; // OK
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
            actualizar_tabla_sticks(ctx);
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
            actualizar_tabla_sticks(ctx);
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
