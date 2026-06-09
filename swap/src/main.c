#include <utils/utils_client.h>
#include <utils/utils_server.h>

void procesar_peticion_lectura(int socket_kernel_memory, t_log* logger) {
    log_info(logger, "## Lectura del bloque: <NUMERO_BLOQUE>");
}

void procesar_peticion_escritura(int socket_kernel_memory, t_log* logger) {
    log_info(logger, "## Escritura del bloque: <NUMERO_BLOQUE>");
}

int main(void){
    
    logger = log_create("swap.log","SWAP", true, LOG_LEVEL_INFO);
    t_config* config = config_create("swap.config");

    if (config == NULL) {
        log_error(logger, "No se pudo leer el config");
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    char* IP_MEMORY = config_get_string_value(config, "IP_MEMORY");
    char* PUERTO_MEMORY = config_get_string_value(config, "PUERTO_MEMORY");
    char* SWAP_FILE_PATH = config_get_string_value(config, "SWAP_FILE_PATH");
    int SWAP_FILE_SIZE = config_get_int_value(config, "SWAP_FILE_SIZE");
    int BLOCK_SIZE = config_get_int_value(config, "BLOCK_SIZE");

    FILE* swap_file = fopen(SWAP_FILE_PATH, "wb+");
    fseek(swap_file, SWAP_FILE_SIZE - 1, SEEK_SET);
    fputc('\0', swap_file);

    if (swap_file == NULL) {
        log_error(logger, "No se pudo crear el archivo de SWAP");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    int socket_kernel_memory = crear_conexion(IP_MEMORY,PUERTO_MEMORY);

    if (socket_kernel_memory == -1) {
        log_error(logger, "No se pudo conectar a Kernel Memory");
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }
    log_info(logger, "## Conectado a Kernel Memory");

    char* msj_handshake = string_from_format("SWAP|%d|%d", SWAP_FILE_SIZE, BLOCK_SIZE);
    enviar_mensaje(msj_handshake, socket_kernel_memory);
    free(msj_handshake);
    log_info(logger, "Mensaje enviado a kernel Memory correctamente");

    while(1) {
        log_info(logger, "Esperando peticiones de Kernel Memory...");
        int cod_op = recibir_operacion(socket_kernel_memory);
        
        switch(cod_op) {
             case LECTURA:
                  procesar_peticion_lectura(socket_kernel_memory, logger);
                  break;
             case ESCRITURA:
                  procesar_peticion_escritura(socket_kernel_memory, logger);
                 break;
             case -1:
                  log_error(logger, "Desconexión de Kernel Memory");
                  fclose(swap_file);
                  close(socket_kernel_memory);
                  config_destroy(config);
                  log_destroy(logger);
                  return EXIT_FAILURE; 
             default:
                  log_warning(logger, "Operacion desconocida.");
                  break;
         }
    }
    
    fclose(swap_file); 
    close(socket_kernel_memory);
    config_destroy(config);
    log_destroy(logger);

    return 0;
}
