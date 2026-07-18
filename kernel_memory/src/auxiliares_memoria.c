#include "auxiliares_memoria.h"
#include <commons/string.h>
#include <unistd.h>

bool comparar_por_base(void* a, void* b)
{
    t_hueco_libre* h1 = (t_hueco_libre*) a;
    t_hueco_libre* h2 = (t_hueco_libre*) b;

    return h1->base < h2->base;
}

void fusionar_huecos_libres(t_list* huecos_libres){
    if (list_size(huecos_libres) < 2){
        return;
    }

    list_sort(huecos_libres, comparar_por_base);

    for (int i = 0; i < list_size(huecos_libres) - 1; )
    {
        t_hueco_libre* actual = list_get(huecos_libres, i);
        t_hueco_libre* siguiente = list_get(huecos_libres, i + 1);

        if (actual->limite >= siguiente->base)
        {
            actual->limite = siguiente->limite;

            list_remove(huecos_libres, i + 1);
            free(siguiente);
        }
        else
        {
            i++;
        }
    }                    
}

t_hueco_libre* buscar_hueco_libre_x_lista(t_list* huecos_libres, uint32_t tamanio, t_memory_context* ctx) {
    t_hueco_libre* hueco_elegido = NULL;

    for (int i = 0; i < list_size(huecos_libres); i++) {
        t_hueco_libre* hueco = list_get(huecos_libres, i);
        uint32_t tam_hueco = hueco->limite - hueco->base;

        if (tam_hueco >= tamanio) {
            if (string_equals_ignore_case(ctx->allocation_strategy, "FIRST")) {
                return hueco;
            } else if (string_equals_ignore_case(ctx->allocation_strategy, "BEST")) {
                if (hueco_elegido == NULL || tam_hueco < (hueco_elegido->limite - hueco_elegido->base)) {
                    hueco_elegido = hueco;
                }
            } else if (string_equals_ignore_case(ctx->allocation_strategy, "WORST")) {
                if (hueco_elegido == NULL || tam_hueco > (hueco_elegido->limite - hueco_elegido->base)) {
                    hueco_elegido = hueco;
                }
            }
        }
    }
    return hueco_elegido;
}

void notificar_memoria_corrupta(int scheduler_socket){
    enviar_int(scheduler_socket, CORRUPCION_MEMORIA);
}

void notificar_memoria_expandida(int scheduler_socket){
    enviar_int(scheduler_socket, MEMORIA_EXPANDIDA);
}

/**
 * Busca un rango contiguo de bloques libres en un bitmap de SWAP.
 * Retorna el índice del primer bloque libre, o -1 si no hay espacio.
 */
static int buscar_bloques_libres_en_swap(t_bitarray* bitmap, int total_bloques, int cant_bloques_necesarios) {
    for (int b = 0; b <= total_bloques - cant_bloques_necesarios; b++) {
        bool libre = true;
        for (int j = 0; j < cant_bloques_necesarios; j++) {
            if (bitarray_test_bit(bitmap, b + j)) {
                libre = false;
                break;
            }
        }
        if (libre) return b;
    }
    return -1;
}

bool hay_espacio_para_restaurar_segmentos(t_memory_context* ctx, t_contexto_ejecucion* proc){
    bool hay_espacio = true;

    t_list* huecos_aux = list_create();

    //simulo la asignacion para saber si hay espacio
    for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
        t_hueco_libre* h = list_get(ctx->huecos_libres, i);

        t_hueco_libre* copia = malloc(sizeof(t_hueco_libre));
        copia->base = h->base;
        copia->limite = h->limite;

        list_add(huecos_aux, copia);
    }

    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->tamanio == 0) continue;

        t_hueco_libre* hueco_libre = buscar_hueco_libre_x_lista(huecos_aux, seg->tamanio, ctx);
        if (!hueco_libre) {
            hay_espacio = false;
            break;
        }

        hueco_libre->base += seg->tamanio;

        if (hueco_libre->base == hueco_libre->limite) {
            list_remove_element(huecos_aux, hueco_libre);
            free(hueco_libre);
        }
    }
    list_destroy_and_destroy_elements(huecos_aux, free);

    return hay_espacio;
}

bool requiere_compactacion(t_memory_context* ctx, uint32_t tamanio){
    bool necesita_compactar = false;

    uint32_t memoria_libre_total = 0;
    for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
        t_hueco_libre* h = list_get(ctx->huecos_libres, i);
        memoria_libre_total += (h->limite - h->base);
    }

    if (memoria_libre_total >= tamanio) {
        log_info(ctx->logger, "Espacio suficiente pero fragmentado. Compactacion requerida...");
        necesita_compactar = true;
    }

    return necesita_compactar;
    
}

t_hueco_libre* buscar_hueco_libre(t_memory_context* ctx, uint32_t tamanio) {
    return buscar_hueco_libre_x_lista(ctx->huecos_libres, tamanio, ctx);
}

bool restaurar_segmentos(t_contexto_ejecucion* proc, t_memory_context* ctx, int cliente_fd){
    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->tamanio == 0) continue;
        
        t_hueco_libre* hueco_disponible = buscar_hueco_libre(ctx, seg->tamanio);

        if (hueco_disponible == NULL) {
            enviar_mensaje("ERROR", cliente_fd);
            log_error(ctx->logger, "Error interno: no se encontró un hueco libre");
            return false;
        }
        
        // Asignar base
        seg->base = hueco_disponible->base;
        hueco_disponible->base += seg->tamanio;
        if (hueco_disponible->base == hueco_disponible->limite) {
            list_remove_element(ctx->huecos_libres, hueco_disponible);
            free(hueco_disponible);
        }
        
        list_add(ctx->segmentos_globales, seg);
        
        // Leer de SWAP y escribir en Memoria Principal
        int cant_bloques = (seg->tamanio + ctx->swap_block_size - 1) / ctx->swap_block_size;
        int offset = 0;
        void* buffer_seg = malloc(seg->tamanio);
        
        for (int j = 0; j < cant_bloques; j++) {
            enviar_int(ctx->swap_socket, LECTURA);
            enviar_int(ctx->swap_socket, seg->bloque_swap + j);
            
            int opcode = recibir_operacion(ctx->swap_socket);
            if (opcode == LECTURA) {
                int size_buf;
                void* res = recibir_buffer(&size_buf, ctx->swap_socket);
                int copy_size = (offset + ctx->swap_block_size <= seg->tamanio) ? ctx->swap_block_size : (seg->tamanio - offset);
                memcpy(buffer_seg + offset, res, copy_size);
                free(res);
            }
            offset += ctx->swap_block_size;
            bitarray_clean_bit(ctx->swap_bitmap, seg->bloque_swap + j);
        }
        
        escribir_memoria_distribuida(ctx, seg->base, seg->tamanio, buffer_seg);
        free(buffer_seg);
        seg->bloque_swap = -1;
    }

    return true;
}

t_segmento* crear_segmento(t_memory_context* ctx, uint32_t pid, uint32_t id, uint32_t tamanio, int* needs_compaction) {
    if (tamanio > ctx->segment_max_size) {
        log_error(ctx->logger, "Tamanio solicitado %d mayor a SEGMENT_MAX_SIZE %d", tamanio, ctx->segment_max_size);
        return NULL;
    }

    t_hueco_libre* hueco = buscar_hueco_libre(ctx, tamanio);
    
    if (hueco == NULL) {

        if (requiere_compactacion(ctx, tamanio))
        {
            *needs_compaction = 1;
            return NULL;
        }
        
        log_error(ctx->logger, "Out of Memory para segmento %d de %d bytes", id, tamanio);
        return NULL; // OOM
    }

    t_segmento* segmento = malloc(sizeof(t_segmento));
    segmento->id = id;
    segmento->base = hueco->base;
    segmento->tamanio = tamanio;

    hueco->base += tamanio;
    if (hueco->base == hueco->limite) {
        for (int i = 0; i < list_size(ctx->huecos_libres); i++) {
            if (list_get(ctx->huecos_libres, i) == hueco) {
                list_remove_and_destroy_element(ctx->huecos_libres, i, free);
                break;
            }
        }
    }

    list_add(ctx->segmentos_globales, segmento);
    // Log obligatorio del TP (strict format)
    log_info(ctx->logger, "## PID: %d - Segmento Creado %d - Tamaño: %d", pid, id, tamanio);
    return segmento;
}

void eliminar_segmento(t_memory_context* ctx, uint32_t pid, t_segmento* segmento) {
    for (int i = 0; i < list_size(ctx->segmentos_globales); i++) {
        if (list_get(ctx->segmentos_globales, i) == segmento) {
            list_remove(ctx->segmentos_globales, i);
            break;
        }
    }

    t_hueco_libre* nuevo_hueco = malloc(sizeof(t_hueco_libre));
    nuevo_hueco->base = segmento->base;
    nuevo_hueco->limite = segmento->base + segmento->tamanio;
    list_add(ctx->huecos_libres, nuevo_hueco);
    fusionar_huecos_libres(ctx->huecos_libres);

    log_info(ctx->logger, "## PID: %d - Eliminar Segmento: %d - Base: %d - TAMAÑO: %d", pid, segmento->id, segmento->base, segmento->tamanio);
    free(segmento);
}

bool ordenar_segmentos_por_base(void* a, void* b) {
    t_segmento* seg1 = (t_segmento*)a;
    t_segmento* seg2 = (t_segmento*)b;
    return seg1->base < seg2->base;
}

void actualizar_tablas_segmentos(t_memory_context* ctx, uint32_t base_vieja, uint32_t id, uint32_t nueva_base)
{
    pthread_mutex_lock(&ctx->mutex_programas);
    for (int i = 0; i < list_size(ctx->programas); i++) {

        t_programa* prog = list_get(ctx->programas, i);

        for (int j = 0; j < list_size(prog->contexto->tabla_segmentos); j++) {

            t_segmento* seg = list_get(prog->contexto->tabla_segmentos, j);

            if (seg->base == base_vieja && seg->id == id) {
                seg->base = nueva_base;
                pthread_mutex_unlock(&ctx->mutex_programas);
                return;
            }
        }
    }
    pthread_mutex_unlock(&ctx->mutex_programas);
}

void compactar_memoria(t_memory_context* ctx) {
    log_info(ctx->logger, "## Inicio de compactación");
    usleep(ctx->compaction_delay * 1000);
    
    list_sort(ctx->segmentos_globales, ordenar_segmentos_por_base);
    
    uint32_t offset_actual = 0;
    for (int i = 0; i < list_size(ctx->segmentos_globales); i++) {
        t_segmento* seg = list_get(ctx->segmentos_globales, i);
        if (seg->base != offset_actual) {
            uint32_t base_vieja = seg->base;
            void* temp_buffer = leer_memoria_distribuida(ctx, seg->base, seg->tamanio);
            if (temp_buffer != NULL) {
                escribir_memoria_distribuida(ctx, offset_actual, seg->tamanio, temp_buffer);
                free(temp_buffer);
            }
            seg->base = offset_actual;

            actualizar_tablas_segmentos(ctx, base_vieja, seg->id, offset_actual);
        }
        offset_actual += seg->tamanio;
    }

    
    list_clean_and_destroy_elements(ctx->huecos_libres, free);
    if (offset_actual < ctx->memoria_total) {
        t_hueco_libre* hueco_final = malloc(sizeof(t_hueco_libre));
        hueco_final->base = offset_actual;
        hueco_final->limite = ctx->memoria_total;
        list_add(ctx->huecos_libres, hueco_final);
    }
    
    log_info(ctx->logger, "## Fin de compactación");
}

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
            memcpy(buffer + leido, res, bytes_a_leer);
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

void liberar_segmento_de_memoria(t_memory_context* ctx, t_segmento* seg){

    // Liberar espacio en memoria principal
    for (int h = 0; h < list_size(ctx->segmentos_globales); h++) {
        t_segmento* gs = list_get(ctx->segmentos_globales, h);
        if (gs->base == seg->base && gs->id == seg->id) {
            t_hueco_libre* hueco = malloc(sizeof(t_hueco_libre));
            hueco->base = seg->base;
            hueco->limite = seg->base + seg->tamanio;
            list_add(ctx->huecos_libres, hueco);
            fusionar_huecos_libres(ctx->huecos_libres);
                
            list_remove(ctx->segmentos_globales, h);
            break;
        }
    }
}

void escribir_segmento_en_swap(t_memory_context* ctx, t_segmento* seg, int bloque_inicio, void* buffer_seg, int cant_bloques_necesarios){

    int offset = 0;
    for (int j = 0; j < cant_bloques_necesarios; j++) {
        void* bloque_data = malloc(ctx->swap_block_size);
        memset(bloque_data, 0, ctx->swap_block_size);
        int copy_size = (offset + ctx->swap_block_size <= seg->tamanio) ? ctx->swap_block_size : (seg->tamanio - offset);
        memcpy(bloque_data, buffer_seg + offset, copy_size);
            
        enviar_int(ctx->swap_socket, ESCRITURA);
        enviar_int(ctx->swap_socket, bloque_inicio + j);
        enviar_int(ctx->swap_socket, ctx->swap_block_size);
        send(ctx->swap_socket, bloque_data, ctx->swap_block_size, 0);
            
        int r = recibir_operacion(ctx->swap_socket);
        if (r == MENSAJE) {
            int s; char* m = recibir_buffer(&s, ctx->swap_socket); free(m);
        }
        free(bloque_data);
        offset += copy_size;
    }
    free(buffer_seg);
}

void suspender_proceso(t_memory_context* ctx, t_contexto_ejecucion* proc, int cant_bloques_swap){

    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {
        t_segmento* seg = list_get(proc->tabla_segmentos, i);
        if (seg->tamanio == 0) continue;
        
        int cant_bloques_necesarios = (seg->tamanio + ctx->swap_block_size - 1) / ctx->swap_block_size;
        int bloque_inicio = buscar_bloques_libres_en_swap(ctx->swap_bitmap, cant_bloques_swap, cant_bloques_necesarios);
        
        if (bloque_inicio == -1) {
            log_error(ctx->logger, "Error interno: la verificación previa indicó que había espacio en SWAP");
            return;
        }
        
        for (int j = 0; j < cant_bloques_necesarios; j++) {
            bitarray_set_bit(ctx->swap_bitmap, bloque_inicio + j);
        }
        
        seg->bloque_swap = bloque_inicio;
        
        void* buffer_seg = leer_memoria_distribuida(ctx, seg->base, seg->tamanio);
        
        escribir_segmento_en_swap(ctx, seg, bloque_inicio, buffer_seg, cant_bloques_necesarios);
        
        liberar_segmento_de_memoria(ctx, seg);
    }

}

bool hay_espacio_para_suspender_proceso(t_memory_context* ctx, t_contexto_ejecucion* proc)
{
    int cant_bloques_swap = ctx->swap_file_size / ctx->swap_block_size;

    int bitmap_size = (cant_bloques_swap + 7) / 8;
    void* bitmap_aux_data = malloc(bitmap_size);
    memcpy(bitmap_aux_data, ctx->bitarray_data, bitmap_size);

    t_bitarray* bitmap_aux = bitarray_create_with_mode(
        bitmap_aux_data,
        bitmap_size,
        LSB_FIRST
    );

    for (int i = 0; i < list_size(proc->tabla_segmentos); i++) {

        t_segmento* seg = list_get(proc->tabla_segmentos, i);

        if (seg->tamanio == 0)
            continue;

        int cant_bloques_necesarios =
            (seg->tamanio + ctx->swap_block_size - 1) / ctx->swap_block_size;

        int bloque_inicio = buscar_bloques_libres_en_swap(bitmap_aux, cant_bloques_swap, cant_bloques_necesarios);

        if (bloque_inicio == -1) {
            bitarray_destroy(bitmap_aux);
            free(bitmap_aux_data);
            return false;
        }

        for (int j = 0; j < cant_bloques_necesarios; j++) {
            bitarray_set_bit(bitmap_aux, bloque_inicio + j);
        }
    }

    bitarray_destroy(bitmap_aux);
    free(bitmap_aux_data);

    return true;
}