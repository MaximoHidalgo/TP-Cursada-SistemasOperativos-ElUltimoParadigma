#ifndef CPU_H
#define CPU_H

#include "utils/utils_client.h"
#include "utils/utils_server.h"
#include "ciclo_instruccion.h"

#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_PATH "cpu.log"
#define PROCESS_NAME "cpu"

typedef struct
{
    char *ip_memoria;
    char *puerto_memoria;
    char *puerto_escucha_dispatch;
    char *puerto_escucha_interrupt;
    int cantidad_entradas_tlb;
    char *algoritmo_tlb;
} t_cpu_config;



#endif //CPU_H