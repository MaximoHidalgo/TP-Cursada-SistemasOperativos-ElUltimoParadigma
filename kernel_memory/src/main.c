#include <utils/conexiones.h>

void* conexion_memory(void* arg) {
    sleep(1);

    int conexion = crear_conexion("127.0.0.1", "6000");
    printf("Conectado a Memory!\n");

    enviar_mensaje("HOLA MEMORY", conexion);

    char* respuesta = recibir_mensaje(conexion);
    printf("Respuesta de Memory: %s\n", respuesta);

    free(respuesta);
}

int main(int argc, char* argv[]) {
    int server_fd=iniciar_servidor(6000);
    int cliente = esperar_cliente(server_fd);

    char* mensaje = recibir_mensaje(cliente);
    printf("Kernel dice: %s\n", mensaje);

    enviar_mensaje("OK KERNEL", cliente);
    return 0;
}
