#include "registros.h"

uint32_t leer_registro(t_contexto_ejecucion* contexto, char* nombre_registro) {
    if (string_equals_ignore_case(nombre_registro, "PC")) return contexto->registros.PC;
    if (string_equals_ignore_case(nombre_registro, "AX")) return contexto->registros.AX;
    if (string_equals_ignore_case(nombre_registro, "EAX")) return contexto->registros.EAX;
    if (string_equals_ignore_case(nombre_registro, "BX")) return contexto->registros.BX;
    if (string_equals_ignore_case(nombre_registro, "EBX")) return contexto->registros.EBX;
    if (string_equals_ignore_case(nombre_registro, "CX")) return contexto->registros.CX;
    if (string_equals_ignore_case(nombre_registro, "ECX")) return contexto->registros.ECX;
    if (string_equals_ignore_case(nombre_registro, "DX")) return contexto->registros.DX;
    if (string_equals_ignore_case(nombre_registro, "EDX")) return contexto->registros.EDX;
    if (string_equals_ignore_case(nombre_registro, "SI")) return contexto->registros.SI;
    if (string_equals_ignore_case(nombre_registro, "DI")) return contexto->registros.DI;
    return 0;
}

int obtener_tamanio_registro(char* nombre_registro) {
    if (string_equals_ignore_case(nombre_registro, "AX") || string_equals_ignore_case(nombre_registro, "BX") || 
        string_equals_ignore_case(nombre_registro, "CX") || string_equals_ignore_case(nombre_registro, "DX")) return 1;
    if (string_equals_ignore_case(nombre_registro, "PC") || string_equals_ignore_case(nombre_registro, "EAX") || 
        string_equals_ignore_case(nombre_registro, "EBX") || string_equals_ignore_case(nombre_registro, "ECX") || 
        string_equals_ignore_case(nombre_registro, "EDX") || string_equals_ignore_case(nombre_registro, "SI") || 
        string_equals_ignore_case(nombre_registro, "DI")) return 4;
    return 0;
}

void asignar_valor_a_registro(t_contexto_ejecucion* contexto, char* nombre_registro, uint32_t valor) {
    if (string_equals_ignore_case(nombre_registro, "PC")) {
        contexto->registros.PC = valor;
    } else if (string_equals_ignore_case(nombre_registro, "AX")) {
        contexto->registros.AX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_registro, "EAX")) {
        contexto->registros.EAX = valor;
    } else if (string_equals_ignore_case(nombre_registro, "BX")) {
        contexto->registros.BX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_registro, "EBX")) {
        contexto->registros.EBX = valor;
    } else if (string_equals_ignore_case(nombre_registro, "CX")) {
        contexto->registros.CX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_registro, "ECX")) {
        contexto->registros.ECX = valor;
    } else if (string_equals_ignore_case(nombre_registro, "DX")) {
        contexto->registros.DX = (uint8_t)valor;
    } else if (string_equals_ignore_case(nombre_registro, "EDX")) {
        contexto->registros.EDX = valor;
    } else if (string_equals_ignore_case(nombre_registro, "SI")) {
        contexto->registros.SI = valor;
    } else if (string_equals_ignore_case(nombre_registro, "DI")) {
        contexto->registros.DI = valor;
    }
}
