#ifndef REGISTROS_H_
#define REGISTROS_H_

#include <stdint.h>
#include <commons/string.h>
#include <utils/dominio/estructuras.h>

uint32_t leer_registro(t_contexto_ejecucion* contexto, char* nombre_registro);
int obtener_tamanio_registro(char* nombre_registro);
void asignar_valor_a_registro(t_contexto_ejecucion* contexto, char* nombre_registro, uint32_t valor);

#endif // REGISTROS_H_
