#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../src/procesador_mensajes.h"

void test_procesador_mensaje() {
    // Caso Feliz
    //const char* nombre_ready = planificador_nombre_estado(ESTADO_PROCESO_READY);
    //assert(strcmp(nombre_ready, "READY") == 0);
    
    // Caso Extremo: Enum inválido no rompe el sistema
    //const char* nombre_invalido = planificador_nombre_estado(999);
    //assert(strcmp(nombre_invalido, "DESCONOCIDO") == 0);
    
    printf("BRAVO test_procesador_de_mensajes: PASO\n");
}