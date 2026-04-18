#include <utils/conexiones.h>
#include <utils/utils_server.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
char* PUERTO_SCHEDULER = "8000";
char* IP_MEMORY = "127.0.0.1";
char* PUERTO_MEMORY ="8001";

int main(int argc, char* argv[]) {

    int server_fd=iniciar_servidor(PUERTO_MEMORY);
    while(1){
        int cliente = esperar_cliente(server_fd);

        char* mensaje = guardar_mensaje(cliente);
        printf("Kernel dice: %s\n", mensaje);

        enviar_mensaje("OK KERNEL", cliente);
    }
    return 0;
}

/*
void* conexion_memory(void* arg) {
    sleep(1);

    int conexion = crear_conexion("127.0.0.1", "6000");
    printf("Conectado a Memory!\n");

    enviar_mensaje("HOLA MEMORY", conexion);

    char* respuesta = recibir_mensaje(conexion);
    printf("Respuesta de Memory: %s\n", respuesta);

    free(respuesta);
}
*/

