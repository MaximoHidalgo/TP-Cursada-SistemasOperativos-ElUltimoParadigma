#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <pthread.h>
#include "colas.h"
//#include "estructuras_scheduler.h" //agregado para implemencion de ciertas funciones
#include <stdlib.h>
#include <string.h>
#include <commons/string.h>
#include <unistd.h>

typedef enum {
    ALGORITMO_PLANIFICACION_CMN, //El de colas multinivel
    ALGORITMO_PLANIFICACION_FIFO, //El de first in first out
    ALGORITMO_PLANIFICACION_RR, //El de Round Robin
    ALGORITMO_PLANIFICACION_INVALIDO
} t_algoritmo_planificacion;

typedef struct {
    t_algoritmo_planificacion algoritmo;
    t_queue* cola_new;
    t_list* cola_susp_ready;//atributo agregado para tratar a los suspendidos ready
    t_list* cola_susp_blocked; // atributo agregado para tratar a los suspendidos bloqueados
    t_list* colas;
    t_proceso_planificable* proceso_exec;
    bool desalojo_entre_colas;
    int quantum_rr;
    t_dictionary* colas_io;      
    t_dictionary* mutex_creados; 
    pthread_mutex_t mutex_colas;
    t_log* logger;
    //int socket_cpu; //comentado porque no me permite trabajar con varias cpus
    t_list* sockets_cpu; //<--atributo agregado para soportar varias cpus
    int socket_memoria;
    pthread_mutex_t mutex_socket_memoria;
    int suspension_timeout;
} t_estratega_planificacion;

typedef struct {
    char* nombre;
    int tomado;
    uint32_t pid_duenio;
    t_proceso_planificable* pcb_duenio; // <--- NUEVO CAMPO AGREGADO: Razon: para corregir esto de la herencia de prioridades, cambiando su prioridad de forma directa
    t_queue* cola_bloqueados;
    pthread_mutex_t mutex_semaforo;
} t_mutex;

typedef struct {
    t_proceso_planificable* proceso;
    char* param1;
    char* param2;
} t_solicitud_io;

typedef struct {
    char* nombre;
    int socket_fd;
    t_solicitud_io* solicitud_actual;
    t_queue* cola_bloqueados; // Cola de t_solicitud_io*
    pthread_mutex_t mutex_interfaz;
} t_interfaz_io;

bool comparador_procesos_suspendidos(void* a, void* b); //funcion agregada para evaluar cual desuspender

t_estratega_planificacion* crear_estratega_planificacion(t_config* config, t_log* logger);
void destruir_estratega_planificacion(t_estratega_planificacion* estratega);
void destruir_mutex(void* element);

// void despachar_proceso_a_cpu(t_estratega_planificacion* estratega, int socket_cpu);
void planificador_registrar_io(t_estratega_planificacion* estratega, char* nombre_interfaz, int socket_fd);
bool planificador_es_io_registrado(t_estratega_planificacion* estratega, int socket_fd);
t_interfaz_io* buscar_interfaz_por_socket(t_estratega_planificacion* estratega, int socket_fd);
t_interfaz_io* buscar_interfaz_por_nombre(t_estratega_planificacion* estratega, char* nombre);
void planificador_atender_peticion_io(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* interfaz);
void planificador_io_terminada(t_estratega_planificacion* estratega, int socket_fd);

void planificador_mutex_create(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso);
int planificador_mutex_lock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso);
int planificador_mutex_unlock(t_estratega_planificacion* estratega, t_proceso_planificable* pcb, char* recurso);

bool planificador_encolar(t_estratega_planificacion* estratega, t_proceso_planificable* proceso);
t_proceso_planificable* planificador_obtener_siguiente(t_estratega_planificacion* estratega);
bool planificador_reencolar_por_quantum(t_estratega_planificacion* estratega, t_proceso_planificable* proceso);
bool planificador_debe_desalojar(t_estratega_planificacion* estratega, int cola_en_ejecucion, int cola_nuevo_proceso);
int planificador_quantum_para(t_estratega_planificacion* estratega, int cola_prioridad);
bool planificador_encolar_new(t_estratega_planificacion* estratega, t_proceso_planificable* proceso);
t_proceso_planificable* planificador_admitir_a_ready(t_estratega_planificacion* estratega);
t_proceso_planificable* planificador_despachar_siguiente(t_estratega_planificacion* estratega);
void planificador_detener_ejecucion(t_estratega_planificacion* estratega, int pid);
void planificador_liberar_exec(t_estratega_planificacion* estratega, int pid);
bool planificador_iniciar_quantum_si_corresponde(t_estratega_planificacion* estratega, t_proceso_planificable* proceso, int socket_cpu);

const char* planificador_nombre_algoritmo(t_algoritmo_planificacion algoritmo);
void planificador_finalizar_todo_por_bsod(t_estratega_planificacion*);
void planificador_intentar_desuspender(t_estratega_planificacion* estratega);
void planificador_enviar_interrupcion(t_estratega_planificacion* estratega, int opcode_interrupcion);
int esperar_respuesta_memoria(int socket_memoria, t_estratega_planificacion* estratega);
int planificador_orquestar_compactacion_sincronica(t_estratega_planificacion* estratega);
#endif
