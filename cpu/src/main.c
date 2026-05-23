#include <utils/utils_client.h>
#include <utils/utils_server.h>
#include <utils/utils_estructuras.h>
#include "ciclo_instruccion.h"

int main(int argc, char* argv[]) {

    validar_argumentos(argc);

    char* path_config = argv[1];
    char* id_cpu = argv[2];

    printf("Se arranco el CPU correctamente con el id: %s y el path del config: %s\n", id_cpu, path_config);

    char* nombre_log = string_from_format("%s.log", id_cpu);
    t_log* logger = log_create(nombre_log, "CPU", true, LOG_LEVEL_INFO);
    free(nombre_log);

    t_config* config = config_create(path_config);
    if (config == NULL) {
        log_error(logger, "No se pudo leer el archivo de configuracion");
        exit(EXIT_FAILURE);
    }

    char* ip_scheduler = config_get_string_value(config, "IP_SCHEDULER");
    char* puerto_scheduler = config_get_string_value(config,"PUERTO_SCHEDULER");
    char* ip_memory = config_get_string_value(config, "IP_MEMORY");
    char* puerto_memory = config_get_string_value(config,"PUERTO_MEMORY");
    char* ip_sticks= config_get_string_value(config, "IP_STICK");
    char* puerto_sticks = config_get_string_value(config,"PUERTO_STICK");
    
    log_info(logger, "Intentando conectar Kernel Memory...");
    int conexion_memory = crear_conexion(ip_memory, puerto_memory);
    if (conexion_memory != -1) {
        log_info(logger, "## Conectado a Kernel Memory");
        enviar_mensaje(id_cpu, conexion_memory);
    }

    log_info(logger, "Intentando conectar Kernel Scheduler...");
    int conexion_scheduler = crear_conexion(ip_scheduler, puerto_scheduler);
    if (conexion_scheduler != -1) {
        log_info(logger, "## Conectado a Kernel Scheduler");
        enviar_mensaje(id_cpu, conexion_scheduler);
    }

    log_info(logger, "Intentando conectar con los Memory Sticks...");
    int conexion_sticks = crear_conexion(ip_sticks, puerto_sticks);
    if (conexion_sticks != -1) {
        log_info(logger, "## Conectado a Memory Stick");
        enviar_mensaje(id_cpu, conexion_sticks);
    }

    // --- BUCLE INFINITO DE LA CPU ---
    while (1) {
        log_info(logger, "Esperando un nuevo PID del Kernel Scheduler...");
        int process_ID = recibir_operacion(conexion_scheduler);
        
        if (process_ID == PID_A_EJECUTAR) {
            int size;
            void* buffer = recibir_buffer(&size, conexion_scheduler);
            
            // CORRECCIÓN DE LECTURA DEL PID
            int desplazamiento = 0;
            int size_payload;
            memcpy(&size_payload, buffer + desplazamiento, sizeof(int));
            desplazamiento += sizeof(int);
            
            int pid_recibido;
            memcpy(&pid_recibido, buffer + desplazamiento, sizeof(int));
            free(buffer);

            log_info(logger, "## PID recibido para ejecutar: %d", pid_recibido);

            t_contexto_ejecucion* contexto = pedir_contexto_a_memoria(pid_recibido, conexion_memory, logger);

            if (contexto != NULL) {
                ejecutar_ciclo_de_instruccion(contexto, conexion_memory, conexion_scheduler, logger);
                free(contexto);
            } else {
                log_error(logger, "Memoria devolvió un contexto nulo para el PID %d", pid_recibido);
            }
        } 
        else if (process_ID == -1) {
            log_error(logger, "El Kernel Scheduler se desconectó inesperadamente.");
            break; 
        } 
        else {
            log_warning(logger, "Operación desconocida recibida del Scheduler: %d", process_ID);
            break;
        }
    }

    limpieza_programa(conexion_scheduler, conexion_memory, conexion_sticks, logger, config);
    printf("Termino la CPU y se cerro correctamente\n");

    return 0;
}