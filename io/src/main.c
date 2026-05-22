#include <utils/utils_client.h>
#include <utils/utils_server.h>

void ejecutar_stdin(int pid, int socket_scheduler, t_log* logger) {
    int cantidad_a_leer = recibir_int(socket_scheduler);
    
    log_info(logger, "## PID: %d - Ingrese %d caracteres:", pid, cantidad_a_leer);
    
    char* leido = readline("> ");
    char* texto_final = malloc(cantidad_a_leer + 1);

    int len_leido = strlen(leido);
    for (int i = 0; i < cantidad_a_leer; i++) {
        if (i < len_leido) {
            texto_final[i] = leido[i];
        } else {
            texto_final[i] = '\0';
        }
    }
    texto_final[cantidad_a_leer] = '\0';

    enviar_mensaje(texto_final, socket_scheduler);

    free(leido);
    free(texto_final);
}

void ejecutar_stdout(int pid, int socket_scheduler, t_log* logger) {
    char* texto = recibir_string(socket_scheduler);
    
    log_info(logger, "## PID: %d - %s", pid, texto);
    
    free(texto);
}

void ejecutar_sleep(int pid, int socket_scheduler, t_log* logger) {
    int milisegundos = recibir_int(socket_scheduler); 

    log_info(logger, "## PID: %d - Haciendo sleep por %d milisegundos.", pid, milisegundos);
    
    usleep(milisegundos * 1000);
}

int main(int argc, char* argv[]) {
    if (argc != 3){
        printf("Error en parametros. Uso: ./io [path] [io]\n");
        return EXIT_FAILURE;
    }
    char* ConfigPath = argv[1]; //Guardamos el path en una variable
    char* IoName = argv[2]; //Guardamos si es STDIN, STOUT, SLEEP en una variable

    t_config* config = config_create(ConfigPath);
    if (config == NULL){
        printf("No se pudo leer el archivo de config del modulo IO\n");
        return EXIT_FAILURE;
    }

    //Si existe el archivo config y lo pudimos leer, guardamos la ip y puerto del kernel
    char* ipKernel = config_get_string_value(config, "IP_SCHEDULER");
    char* puertoKernel = config_get_string_value(config, "PUERTO_SCHEDULER");
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    
    //Inicializamos el log
    t_log_level nivelLog = log_level_from_string(log_level_str);
    t_log* logger = log_create("io.log", IoName, true, nivelLog);

    if (logger == NULL){
        printf("No se pudo crear el logger en modulo IO\n");
        config_destroy(config);
        return EXIT_FAILURE;
    }

    //Ahora si todo salio bien, nos conectamos al kernel
    int socket_scheduler = crear_conexion(ipKernel, puertoKernel);

    if(socket_scheduler == -1){
        log_error(logger, "No se pudo establecer conexion con el Kernel Scheduler\n");
        log_destroy(logger);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    //Si se conecto, lo avisamos
    log_info(logger, "## Conectado a Kernel Scheduler");
    enviar_mensaje(IoName, socket_scheduler);
    log_info(logger, "Interfaz %s lista. Presione ENTER para cerrar la conexión", IoName);

    while (1) {
        int cod_op = recibir_operacion(socket_scheduler);
        
        if (cod_op == PETICION_IO) { 
            int pid = recibir_int(socket_scheduler); 
            
            log_info(logger, "## PID: %d - Inicio de IO", pid);

            if (string_equals_ignore_case(IoName, "SLEEP")) {
                ejecutar_sleep(pid, socket_scheduler, logger);
            } 
            else if (string_equals_ignore_case(IoName, "STDOUT")) {
                ejecutar_stdout(pid, socket_scheduler, logger);
            } 
            else if (string_equals_ignore_case(IoName, "STDIN")) {
                ejecutar_stdin(pid, socket_scheduler, logger);
            }

            log_info(logger, "## PID: %d - Fin de IO", pid);
            
            enviar_mensaje("OK", socket_scheduler);
        }
        else if (cod_op == -1) {
            log_error(logger, "Se desconectó el Kernel Scheduler.");
            break;
        }
    }

    //Limpiamos recursos
    liberar_conexion(socket_scheduler);
    log_destroy(logger);
    config_destroy(config);

    
    return 0;
}

