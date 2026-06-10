#include "main.h"

t_log* logger;
void manejar_ejecucion_proceso(int pid_recibido, t_conexiones_cpu* conexiones, t_log* logger) {
    log_info(logger, "## PID recibido para ejecutar: %d", pid_recibido);

    t_contexto_ejecucion* contexto = pedir_contexto_a_memoria(pid_recibido, conexiones->memoria, logger);

    if (contexto != NULL) {
        ejecutar_ciclo_de_instruccion(contexto, conexiones, logger);
        free(contexto);
    } else {
        log_error(logger, "Memoria devolvió un contexto nulo para el PID %d", pid_recibido);
    }
}

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
        return EXIT_FAILURE;
    }

    char* ip_scheduler = config_get_string_value(config, "IP_SCHEDULER");
    char* puerto_scheduler = config_get_string_value(config,"PUERTO_SCHEDULER");
    char* ip_memory = config_get_string_value(config, "IP_MEMORY");
    char* puerto_memory = config_get_string_value(config,"PUERTO_MEMORY");
    char* ip_sticks= config_get_string_value(config, "IP_STICK");
    char* puerto_sticks = config_get_string_value(config,"PUERTO_STICK");
    
    t_conexiones_cpu* conexiones = malloc(sizeof(t_conexiones_cpu));

    log_info(logger, "Intentando conectar Kernel Memory...");
    conexiones->memoria = crear_conexion(ip_memory, puerto_memory);
    if (conexiones->memoria != -1) {
        log_info(logger, "## Conectado a Kernel Memory");
        enviar_mensaje(id_cpu, conexiones->memoria);
    }

    log_info(logger, "Intentando conectar Kernel Scheduler...");
    conexiones->scheduler = crear_conexion(ip_scheduler, puerto_scheduler);
    if (conexiones->scheduler != -1) {
        log_info(logger, "## Conectado a Kernel Scheduler");
        enviar_mensaje(id_cpu, conexiones->scheduler);
    }

    log_info(logger, "Intentando conectar con los Memory Sticks...");
    conexiones->sticks = crear_conexion(ip_sticks, puerto_sticks);
    if (conexiones->sticks != -1) {
        log_info(logger, "## Conectado a Memory Stick");
        enviar_mensaje(id_cpu, conexiones->sticks);
    }

    while (1) {
        log_info(logger, "Esperando un nuevo PID del Kernel Scheduler...");
        int process_ID = recibir_operacion(conexiones->scheduler);
        
        switch(process_ID) {
            case PID_A_EJECUTAR:
            {
                int size;
                void* buffer = recibir_buffer(&size, conexiones->scheduler);
                
                int desplazamiento = 0;
                int size_payload;
                memcpy(&size_payload, buffer + desplazamiento, sizeof(int));
                desplazamiento += sizeof(int);
                
                int pid_recibido;
                memcpy(&pid_recibido, buffer + desplazamiento, sizeof(int));
                free(buffer);

                manejar_ejecucion_proceso(pid_recibido, conexiones, logger);
                break;
            }
            case -1:
                log_error(logger, "El Kernel Scheduler se desconectó inesperadamente.");
                limpieza_programa(conexiones, logger, config);
                printf("Termino la CPU y se cerro correctamente\n");
                return EXIT_FAILURE;
            default:
                log_warning(logger, "Operación desconocida recibida del Scheduler: %d", process_ID);
                limpieza_programa(conexiones, logger, config);
                return EXIT_FAILURE;
        }
    }

    limpieza_programa(conexiones, logger, config);
    return 0;
}