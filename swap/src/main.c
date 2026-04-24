#include <utils/utils_client.h>

int main(void){
    
    t_log* logger = log_create("mainSwap.log","SWAP", true, LOG_LEVEL_INFO);

    t_config* config = config_create("swap.config");

    if (config == NULL) {
        log_error(logger, "No se pudo leer el config");
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    char* IP_MEMORY = config_get_string_value(config, "IP_MEMORY");
    char* PUERTO_MEMORY = config_get_string_value(config, "PUERTO_MEMORY");

    int socket_kernel_memory = crear_conexion(IP_MEMORY,PUERTO_MEMORY);

    if (socket_kernel_memory == -1) {
        log_error(logger, "No se pudo conectar a Kernel Memory");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    log_info(logger, "## Conectado a Kernel Memory");
    enviar_mensaje("mensaje de SWAP", socket_kernel_memory);
    log_info(logger, "Mensaje enviado a kernel Memory correctamente");

    // 4. HANDSHAKE CORRECTO [2]
    // Debés enviar BLOCK_SIZE y SWAP_FILE_SIZE. Te sugiero armar un paquete o un string formateado.
    char* msj_handshake = string_from_format("SWAP|%d|%d", BLOCK_SIZE, SWAP_FILE_SIZE);
    enviar_mensaje(msj_handshake, socket_kernel_memory);
    free(msj_handshake);

    // 5. CICLO DE ESCUCHA (Operaciones) [2, 4]
    // El SWAP se queda esperando peticiones de escritura o lectura.
    while(1) {
        // Ejemplo genérico:
        int cod_op = recibir_operacion(socket_kernel_memory);
        switch(cod_op) {
             case LECTURA:
                  log_info(logger, "## Lectura del bloque: <NUMERO_BLOQUE>");
                  // Lógica de leer el archivo
                  break;
             case ESCRITURA:
                  log_info(logger, "## Escritura del bloque: <NUMERO_BLOQUE>");
                  // Lógica de escribir en el archivo
                 break;
             case -1:
                  log_error(logger, "Desconexión de Kernel Memory");
                 break; // Salir del while
         }
    }

    //LIMPIEZA
    close(socket_kernel_memory);
    config_destroy(config);
    log_destroy(logger);

    return 0;
}
