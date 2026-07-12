#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../src/main.h"

void test_principal_km() {
    
    //iniciar contexto de memoria
    int cant_args = 2;
    //0->nombre ejecutable
    //1->primer argumento
    char* lista_args[1]; 
    lista_args[1] = "kernelMemory.config";
    //archivo config
    t_memory_context* ctx_prueba = iniciar_contexto_km(cant_args, lista_args);

    t_log* log_Prueba = log_create("KernelMemory.log", "KernelMemory", true, LOG_LEVEL_INFO);

     // Caso Feliz
    assert(ctx_prueba->logger == log_Prueba);
    assert(list_is_empty(ctx_prueba->programas));
    assert(ctx_prueba->instruction_delay == 500);
    assert(strcmp(ctx_prueba->base_path,"../utils/scripts") == 0);
    assert(ctx_prueba->espacio_memoria == NULL);
    assert(ctx_prueba->memoria_total == 0);
    assert(ctx_prueba->segment_max_size == 128);
    assert(strcmp(ctx_prueba->allocation_strategy,"BEST") == 0);
    assert(list_is_empty(ctx_prueba->huecos_libres));
    assert(list_is_empty(ctx_prueba->segmentos_globales));
    assert(list_is_empty(ctx_prueba->memory_sticks));
    assert(ctx_prueba->swap_socket == -1);
    assert(ctx_prueba->swap_bitmap == NULL);
    assert(ctx_prueba->bitarray_data == NULL);
    
    // Caso Extremo: Enum inválido no rompe el sistema
    //const char* nombre_invalido = planificador_nombre_estado(999);
    //assert(strcmp(nombre_invalido, "DESCONOCIDO") == 0);
    
    printf("BRAVO test_principal_km: PASO\n");
}