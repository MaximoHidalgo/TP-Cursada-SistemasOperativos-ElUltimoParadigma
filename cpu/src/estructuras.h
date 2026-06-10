#ifndef ESTRUCTURAS_H_
#define ESTRUCTURAS_H_

#include <stdint.h>
#include <errno.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include <utils/sockets/sockets.h>
#include <utils/protocolo/protocolo.h>
#include <utils/dominio/estructuras.h>
#include <utils/dominio/opcodes.h>

typedef struct {
    char* identificador;
    char* parametro1;//Ej: "AX", "MUTEX_1", "25000"
    char* parametro2;//Ej: "6", "BX" 
    char* parametro3;//Por las dudas
} t_instruccion;

t_contexto_ejecucion* pedir_contexto_a_memoria(int pid_recibido, int conexion_memory, t_log* logger);
void enviar_contexto_a_memoria(t_contexto_ejecucion* contexto, int conexion_memory);
char* fetch(t_contexto_ejecucion* contexto, int conexion_memory, t_log* logger);
t_instruccion* decode(char* instruccion_texto);
int execute(t_instruccion* instruccion, t_contexto_ejecucion* contexto, t_conexiones_cpu* conexiones, t_log* logger);
void asignar_valor_a_registro(t_contexto_ejecucion* contexto, char* nombre_registro, uint32_t valor);
int chequear_interrupciones(int socket_scheduler, t_log* logger);



#endif //ESTRUCTURAS_H_