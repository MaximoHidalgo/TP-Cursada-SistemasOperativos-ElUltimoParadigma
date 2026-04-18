#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Tus headers nuevos
#include "scheduler.h"
#include "conexiones.h"

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
}

// =======================
// MAIN
// =======================
int main(int argc, char* argv[]) {

    printf("Iniciando Kernel Scheduler...\n");

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

    return 0;
}