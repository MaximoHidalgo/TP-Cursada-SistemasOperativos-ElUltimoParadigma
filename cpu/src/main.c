#include <utils/utils_client.h>
#include <utils/utils_server.h>
#include <utils/utils_estructuras.h>
#include "ciclo_instruccion.h"

int main(int argc, char* argv[]) {

    validar_argumentos(argc);

    char* path_config = argv[1];
    char* id_cpu = argv[2];

    printf("Se arranco el CPU correctamente con el id: %s y el path del config: %s\n", id_cpu, path_config);

    //Creo el logger
    char* nombre_log = string_from_format("%s.log", id_cpu);
    t_log* logger = log_create(nombre_log, "CPU", true, LOG_LEVEL_INFO);
    free(nombre_log);

    //Levanto el archivo config
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
    

	// Creamos una conexión hacia el servidor de kernel Memory
	log_info(logger, "Intentando conectar Kernel Memory...");
	int conexion_memory = crear_conexion(ip_memory, puerto_memory);
    if (conexion_memory != -1) {
        log_info(logger, "## Conectado a Kernel Memory");
        // Enviamos la ID del CPU al servidor de kernel Memory
        enviar_mensaje(id_cpu, conexion_memory);
    }

    // Creamos una conexión hacia el servidor de kernel Scheduler
    log_info(logger, "Intentando conectar Kernel Scheduler...");
	int conexion_scheduler = crear_conexion(ip_scheduler, puerto_scheduler);
	if (conexion_scheduler != -1) {
        log_info(logger, "## Conectado a Kernel Scheduler");
        // Enviamos la ID del CPU al servidor de kernel Scheduler
        enviar_mensaje(id_cpu, conexion_scheduler);
    }

    // Creamos una conexión hacia el servidor de los Memory Sticks
    log_info(logger, "Intentando conectar con los Memory Sticks...");
	int conexion_sticks = crear_conexion(ip_sticks, puerto_sticks);
	if (conexion_sticks != -1) {
        log_info(logger, "## Conectado a Memory Stick");
        // Enviamos la ID del CPU al servidor de los Memory sticks
        enviar_mensaje(id_cpu, conexion_sticks);
    }

    // Esperamos que el Kernel Scheduler nos envíe el contexto de ejecución del proceso a ejecutar
    log_info(logger, "Esperando el PID del Kernel Scheduler...");
    int process_ID = recibir_operacion(conexion_scheduler);
    
    switch (process_ID){
        case PID_A_EJECUTAR:
            //Recibimos el payload (el tamaño y el número de PID) y extraemos el PID del buffer
            int size;
            void* buffer = recibir_buffer(&size, conexion_scheduler);
            int pid_recibido = *(int*)buffer; 
            free(buffer);

            log_info(logger, "## PID recibido para ejecutar: %d", pid_recibido);

            //Le pedimos a Kernel Memory el contexto de ejecución de este PID
            t_contexto_ejecucion* contexto = pedir_contexto_a_memoria(pid_recibido, conexion_memory, logger);

            if (contexto != NULL) {
                ejecutar_ciclo_de_instruccion(contexto, conexion_memory, conexion_scheduler, logger);
                free(contexto);
            }
            break;
        case -1:
            log_error(logger, "El Kernel Scheduler se desconectó inesperadamente o fallo.");
            break;
        default:
            log_warning(logger, "Operación desconocida recibida del Scheduler: %d", process_ID);
            break;
    }

    // Limpiar recursos antes de cerrar el programa
    limpieza_programa(conexion_scheduler, conexion_memory, conexion_sticks, logger, config);
    printf("Termino la CPU y se cerro correctamente\n");

    return 0;
}