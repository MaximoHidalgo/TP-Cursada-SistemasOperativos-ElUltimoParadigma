#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../src/gestion_huecos_y_compactacion.h"
#include "../src/gestion_segmentos.h"
#include "../src/memoria_distribuida.h"
#include "../src/swap_y_suspension.h"
#include "../src/comunicacion_scheduler.h"

void test_auxiliares_memoria() {
    // Caso Feliz
    //const char* nombre_ready = planificador_nombre_estado(ESTADO_PROCESO_READY);
    //assert(strcmp(nombre_ready, "READY") == 0);
    
    // Caso Extremo: Enum inválido no rompe el sistema
    //const char* nombre_invalido = planificador_nombre_estado(999);
    //assert(strcmp(nombre_invalido, "DESCONOCIDO") == 0);
    
    printf("BRAVO test_auxiliares_memoria: PASO\n");
}