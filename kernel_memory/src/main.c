#include <utils/utils_server.h>
#include <utils/utils_client.h>
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

        recibir_mensaje(cliente);

        enviar_mensaje("OK KERNEL", cliente);
    }
    return 0;
}
