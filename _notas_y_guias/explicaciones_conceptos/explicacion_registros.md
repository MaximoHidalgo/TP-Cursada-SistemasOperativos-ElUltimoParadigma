# Explicación de registros.c (Refactorización Data-Driven)

En C, a diferencia de lenguajes de más alto nivel como Java o C#, no existe la **Reflexión** (Reflection). Esto significa que el código compilado pierde los nombres de sus variables. No hay una función nativa en C que tome el string `"AX"` y sepa automáticamente que te referís a `contexto->registros.AX`. 

Por defecto, esto nos obliga a escribir interminables escaleras de `if-else` comparando strings manualmente para encontrar qué registro quiere modificar el código.

Para solucionar esto, aplicamos un diseño llamado **Data-Driven Design** (Diseño guiado por datos) utilizando la macro `offsetof`. 

## 1. La Tabla de Mapeo y `offsetof`

En lugar de programar lógica (`ifs`), programamos **datos**. Creamos una estructura y un arreglo que actúa como diccionario:

```c
typedef struct {
    char* nombre;      // Ej: "AX"
    size_t offset;     // Ej: 4 bytes (distancia desde el inicio del struct)
    int tamanio;       // Ej: 1 byte
} t_registro_info;

static const t_registro_info tabla_registros[] = {
    {"AX", offsetof(t_registros, AX), 1},
    {"EAX", offsetof(t_registros, EAX), 4},
    // ...
};
```

La macro `offsetof(t_registros, AX)` es mágica. Le dice al compilador: *"Si yo me paro en la dirección de memoria donde empieza un `struct t_registros`, ¿cuántos bytes me tengo que mover hacia adelante para llegar justo a la cajita que guarda `AX`?"*.
Esto nos da un número entero (el "offset" o desplazamiento).

## 2. Buscar el Registro

La función `buscar_registro` es la única que tiene un `for` y chequea nombres de strings:

```c
static const t_registro_info* buscar_registro(char* nombre_registro) {
    for (int i = 0; i < NUM_REGISTROS; i++) {
        if (string_equals_ignore_case(nombre_registro, tabla_registros[i].nombre)) {
            return &tabla_registros[i];
        }
    }
    return NULL; // Si el registro no existe
}
```
Si buscamos `"AX"`, esta función nos devuelve la fila entera de nuestra tabla con la metadata (su nombre, su desplazamiento en bytes y su tamaño).

## 3. La Matemática de Punteros (El Corazón de la Lógica)

Aquí es donde usamos la metadata que nos dio la tabla para leer o escribir dinámicamente sin un solo `if` gigante. Todo ocurre en esta función auxiliar:

```c
static void* obtener_puntero_registro(t_contexto_ejecucion* contexto, const t_registro_info* info) {
    // 1. Obtenemos la dirección de memoria base donde arranca el struct de registros (con &)
    void* direccion_base_struct = (void*)&(contexto->registros);
    
    // 2. Nos movemos 'offset' bytes hacia adelante para encontrar la dirección exacta del registro buscado
    void* direccion_registro_exacto = direccion_base_struct + info->offset;
    
    return direccion_registro_exacto;
}
```

**Paso a paso:**
1. Le decimos al compilador "dame la dirección numérica en memoria RAM donde empieza el struct entero". Usamos `(void*)` para que el compilador nos permita sumar bytes individuales crudos.
2. A esa dirección inicial, le sumamos el `offset` que nos devolvió la macro al principio.
3. El resultado es un puntero genérico (`void*`) que apunta exactamente al primer byte del registro solicitado (ej. `AX`).

## 4. Leer y Asignar

Tanto en `leer_registro` como en `asignar_valor_a_registro`, el proceso final es simplemente agarrar ese puntero exacto y leer o meter un valor ahí adentro.

```c
uint32_t leer_registro(t_contexto_ejecucion* contexto, char* nombre_registro) {
    const t_registro_info* info = buscar_registro(nombre_registro); // Buscamos en la tabla
    
    if (info != NULL) {
        void* puntero_al_registro = obtener_puntero_registro(contexto, info); // Matemática mágica
        
        // Dependiendo si el registro es de 1 byte (uint8) o 4 bytes (uint32)
        if (info->tamanio == 1) {
            return *(uint8_t*)puntero_al_registro; // Leemos 1 byte y lo devolvemos
        } else {
            return *(uint32_t*)puntero_al_registro; // Leemos 4 bytes y lo devolvemos
        }
    }
    return 0;
}
```

La sintaxis `*(uint8_t*)puntero` significa:
1. `(uint8_t*)`: *"Che C, este puntero genérico ahora tratalo como si fuera un puntero a un entero de 1 solo byte"*.
2. El asterisco inicial `*`: *"Ahora, traeme el valor real que hay metido adentro de esa dirección de memoria"*.

Y de esta forma, logramos escribir una vez, y si mañana agregan 50 registros más, nuestro código no crece, sólo agregamos 50 líneas pasivas de datos a nuestra tabla. ¡Puro Clean Code!
