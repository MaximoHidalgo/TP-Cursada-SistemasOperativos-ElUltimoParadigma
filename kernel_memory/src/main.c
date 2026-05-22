#include <utils/utils_server.h>
#include <utils/utils_client.h>
#include <pthread.h>

typedef struct {
    int pid;
    char** instrucciones;
    int cantidad_instrucciones;
} t_programa;


t_list* programas;
int instruction_delay;
char* base_path;

//FUNCIONES AUX, VER SI SIRVEN EN EL UTILS

t_programa* buscar_programa(int pid) {
    for (int i = 0; i < list_size(programas); i++) {
        t_programa* prog = list_get(programas, i);
        if (prog->pid == pid)
            return prog;
    }
    return NULL;
}

char** leer_lineas(FILE* f, int* cantidad) {

    char** lineas = NULL;
    char buffer[256];
    int count = 0;

    while (fgets(buffer, sizeof(buffer), f)) {

        lineas = realloc(lineas, sizeof(char*) * (count + 1));

        char* linea = strdup(buffer);

        string_trim(&linea);

        lineas[count] = linea;

        count++;
    }

    *cantidad = count;

    return lineas;
}

void crear_proceso(int pid, char* path) {
    FILE* f = fopen(path, "r");

    if (!f) {
        log_error(logger, "No se pudo abrir el archivo %s", path);
        return;
    }

    t_programa* prog = malloc(sizeof(t_programa));
    prog->pid = pid;
    prog->instrucciones = leer_lineas(f, &prog->cantidad_instrucciones);

    fclose(f);

    list_add(programas, prog);

    log_info(logger, "## PID: %d - Proceso Creado", pid);
}

char* obtener_instruccion(int pid, int pc) {
    t_programa* prog = buscar_programa(pid);

    if (!prog || pc >= prog->cantidad_instrucciones)
        return strdup("EXIT");

    usleep(instruction_delay * 1000);

    char* instruccion = strdup(prog->instrucciones[pc]);

    log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s",
             pid, pc, instruccion);

    return instruccion;
}

// Función que va a ejecutar el hilo por cada cliente conectado
void atender_cliente(void *arg)
{
    int cliente_fd = *(int *)arg;
    free(arg);

    while (1)
    {
        int cod_op = recibir_operacion(cliente_fd);

        if (cod_op == MENSAJE)
        {

            char *mensaje = recibir_handshake(cliente_fd);

            if (string_starts_with(mensaje, "STICK|"))
            {
                char **partes = string_split(mensaje, "|");
                log_info(logger, "## Memory Stick de %s bytes Conectada", partes[1]);
                string_array_destroy(partes);
            }
            else if (string_equals_ignore_case(mensaje, "SCHEDULER"))
            {
                log_info(logger, "## Kernel Scheduler Conectado - FD: %d", cliente_fd);
            }
            else if (string_starts_with(mensaje, "CPU"))
            {
                log_info(logger, "## CPU %s Conectada", mensaje);
            }
            else
            {
                log_info(logger, "## Swap Conectado, %s", mensaje);
            }

            free(mensaje);
        }

        else if (cod_op == CREAR_PROCESO)
        {

            int pid = recibir_int(cliente_fd);
            char *path_relativo = recibir_string(cliente_fd);

            // concatenar base path + archivo
            char *path_completo = string_new();
            string_append(&path_completo, base_path);
            string_append(&path_completo, "/");
            string_append(&path_completo, path_relativo);

            crear_proceso(pid, path_completo);

            enviar_mensaje("OK", cliente_fd);

            free(path_relativo);
            free(path_completo);
        }

        else if (cod_op == OBTENER_INSTRUCCION)
        {

            int pid = recibir_int(cliente_fd);
            int pc = recibir_int(cliente_fd);

            char *instr = obtener_instruccion(pid, pc);

            enviar_mensaje(instr, cliente_fd);

            free(instr);
        }

        else if (cod_op == LEER_MEMORIA)
        {
            enviar_mensaje("OK", cliente_fd);
        }

        else if (cod_op == ESCRIBIR_MEMORIA)
        {
            enviar_mensaje("OK", cliente_fd);
        }

        else if (cod_op == CONSULTAR_ESPACIO_LIBRE) {
            // Requisito Mock Checkpoint 2: Devolver valor fijo y enviarlo de vuelta al Kernel Scheduler que lo preguntó
            int espacio_fijo_mock = 999999; 
            send(cliente_fd, &espacio_fijo_mock, sizeof(int), 0); 
        }

        else if (cod_op == -1)
        {
            log_warning(logger, "Cliente desconectado");
            break;
        }

        else
        {
            log_warning(logger, "Operación desconocida: %d", cod_op);
            break;
        }

        close(cliente_fd);
    }
}

int main(int argc, char *argv[]){
    // Iniciar Logger
    logger = log_create("KernelMemory.log", "KernelMemory", true, LOG_LEVEL_DEBUG);

    // Levantar Configuración
    t_config *config = config_create("kernelMemory.config");
    if (config == NULL)
    {
        log_error(logger, "No se pudo leer la configuración del scheduler");
        return EXIT_FAILURE;
    }

    char *puerto_memory = config_get_string_value(config, "PUERTO_MEMORY");
    base_path = config_get_string_value(config, "SCRIPTS_BASEPATH");
    instruction_delay = config_get_int_value(config, "INSTRUCTION_DELAY");

    programas = list_create();

    // Iniciar el Servidor del Kernel Memory
    int server_fd = iniciar_servidor(puerto_memory);
    log_info(logger, "Kernel Memory listo para recibir clientes en el puerto %s", puerto_memory);

    // Bucle principal para atender clientes
    while (1) {
        int* cliente_fd = malloc(sizeof(int));
        *cliente_fd = esperar_cliente(server_fd);

        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, (void*)atender_cliente, cliente_fd);
        pthread_detach(hilo_cliente); 
    }
    config_destroy(config);
    return 0;
}