#include "registros.h"
#include <stddef.h>

typedef struct {
    char* nombre;
    size_t offset;
    int tamanio;
} t_registro_info;

static const t_registro_info tabla_registros[] = {
    {"AX", offsetof(t_registros, AX), 1},
    {"BX", offsetof(t_registros, BX), 1},
    {"CX", offsetof(t_registros, CX), 1},
    {"DX", offsetof(t_registros, DX), 1},
    {"EAX", offsetof(t_registros, EAX), 4},
    {"EBX", offsetof(t_registros, EBX), 4},
    {"ECX", offsetof(t_registros, ECX), 4},
    {"EDX", offsetof(t_registros, EDX), 4},
    {"SI", offsetof(t_registros, SI), 4},
    {"DI", offsetof(t_registros, DI), 4},
    {"PC", offsetof(t_registros, PC), 4},
};

static const int NUM_REGISTROS = sizeof(tabla_registros) / sizeof(tabla_registros[0]);

static const t_registro_info* buscar_registro(char* nombre_registro) {
    for (int i = 0; i < NUM_REGISTROS; i++) {
        if (string_equals_ignore_case(nombre_registro, tabla_registros[i].nombre)) {
            return &tabla_registros[i];
        }
    }
    return NULL;
}

static void* obtener_puntero_registro(t_contexto_ejecucion* contexto, const t_registro_info* info) {
    // 1. Obtenemos la dirección de memoria base donde arranca el struct de registros (con &)
    void* direccion_base_struct = (void*)&(contexto->registros);
    
    // 2. Nos movemos 'offset' bytes hacia adelante para encontrar la dirección exacta del registro buscado
    void* direccion_registro_exacto = direccion_base_struct + info->offset;
    
    return direccion_registro_exacto;
}

uint32_t leer_registro(t_contexto_ejecucion* contexto, char* nombre_registro) {
    const t_registro_info* info = buscar_registro(nombre_registro);
    
    if (info != NULL) {
        void* puntero_al_registro = obtener_puntero_registro(contexto, info);

        if (info->tamanio == 1) {
            return *(uint8_t*)puntero_al_registro;
        } else {
            return *(uint32_t*)puntero_al_registro;
        }
    }
    return 0;
}

int obtener_tamanio_registro(char* nombre_registro) {
    const t_registro_info* info = buscar_registro(nombre_registro);
    if (info != NULL) return info->tamanio;
    return 0;
}

void asignar_valor_a_registro(t_contexto_ejecucion* contexto, char* nombre_registro, uint32_t valor) {
    const t_registro_info* info = buscar_registro(nombre_registro);
    
    if (info != NULL) {
        void* puntero_al_registro = obtener_puntero_registro(contexto, info);

        if (info->tamanio == 1) {
            *(uint8_t*)puntero_al_registro = (uint8_t)valor;
        } else {
            *(uint32_t*)puntero_al_registro = valor;
        }
    }
}