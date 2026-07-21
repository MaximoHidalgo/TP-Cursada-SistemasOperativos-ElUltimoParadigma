#include "comunicacion_scheduler.h"
#include "gestion_huecos_y_compactacion.h"
#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>

void notificar_memoria_corrupta(int scheduler_socket){
    enviar_int(scheduler_socket, CORRUPCION_MEMORIA);
}

void notificar_memoria_expandida(int scheduler_socket){
    enviar_int(scheduler_socket, MEMORIA_EXPANDIDA);
}

bool solicitar_compactacion_al_scheduler(int cliente_fd, t_memory_context* ctx) {
    int op_sol = SOLICITUD_COMPACTACION;
    send(cliente_fd, &op_sol, sizeof(int), 0);
    
    pthread_mutex_unlock(&ctx->mutex_memoria);
    int res = recibir_operacion(cliente_fd);
    pthread_mutex_lock(&ctx->mutex_memoria);
    
    if (res == RESPUESTA_COMPACTACION) {
        compactar_memoria(ctx);
        return true;
    }
    return false;
}