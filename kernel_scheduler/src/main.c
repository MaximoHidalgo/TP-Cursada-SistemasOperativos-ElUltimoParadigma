#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <utils/conexiones.h>
#include <utils/utils_server.h>
#include <utils/scheduler.h>
char* PUERTO_SCHEDULER = "8000";
char* IP_MEMORY = "127.0.0.1";
char* PUERTO_MEMORY ="8001";


int main(int argc, char* argv[]) {

    int cliente;

    printf("Iniciando Kernel Scheduler...\n");

    int server_fd = iniciar_servidor(PUERTO_SCHEDULER);

    while (1) {
        cliente = esperar_cliente(server_fd);
        printf("CPU conectada!\n");

        // TODO: manejar CPU
    }

    int conexion = crear_conexion(IP_MEMORY, PUERTO_MEMORY);
    printf("Conectado a Kernel Memory!\n");
    
    while (1) {
		int cod_op = recibir_operacion(cliente);
		switch (cod_op) {
		case MENSAJE:
			recibir_mensaje(cliente);
			break;
		}
	}
    
    /*
    // Inicializar estructuras
    iniciar_colas();

    // Crear hilos
    pthread_t hilo_cpu;
    pthread_t hilo_memory;

    pthread_create(&hilo_cpu, NULL, servidor_cpu, NULL);
    pthread_create(&hilo_memory, NULL, conexion_memory, NULL);

    // Crear procesos de prueba
    proceso_t* p1 = malloc(sizeof(proceso_t));
    p1->pid = 1;
    p1->prioridad = 0;
    pasar_a_ready(p1);
    
// Iniciar planificación
    planificar();
    */    

    return 0;
}

/*
// =======================
// THREAD SERVIDOR CPU
// =======================
void* servidor_cpu(void* arg) {
    int server_fd = iniciar_servidor("5000");

    while (1) {
        int cliente = esperar_cliente(server_fd);
        printf("CPU conectada!\n");

        // TODO: manejar CPU
    }
}

// =======================
// CONEXIÓN A MEMORY
// =======================
void* conexion_memory(void* arg) {
    sleep(1); // esperar que memory esté levantado

    int conexion = crear_conexion("127.0.0.1", "6000");
    printf("Conectado a Kernel Memory!\n");

    // TODO: comunicación con memory
}*/