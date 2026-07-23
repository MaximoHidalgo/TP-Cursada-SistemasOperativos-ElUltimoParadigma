# Guía de Testing con Assert Estándar de C

Esta guía explica cómo configurar y escribir pruebas unitarias utilizando únicamente la librería estándar de C (`assert.h`), sin dependencias externas.

## 1. Condiciones Previas y Configuración

Antes de escribir tu primer test, debes preparar tu entorno y tu `Makefile`:

### A. Crear la carpeta de tests
El `Makefile` de la cátedra está configurado para buscar archivos `.c` dentro de una carpeta llamada `tests/`.
- Crea la carpeta `tests/` en la raíz de tu módulo (ej: dentro de `kernel_memory/`).

### B. Modificar el Makefile
Dado que el Makefile viene preparado por defecto para usar la librería `cspecs`, debemos remover ese flag para que compile con asserts estándar:

1. Abrí el `Makefile`.
2. Buscá la regla de enlazado (linkeo) de los tests, que usualmente se ve así:
   ```make
   $(TEST): $(TEST_OBJS) $(DEPS) | $(dir $(TEST))
       $(CC) $(CFLAGS) -o "$@" $^ $(IDIRS:%=-I%) $(LIBDIRS:%=-L%) $(RUNDIRS:%=-Wl,-rpath,%) $(LIBS:%=-l%) -lcspecs
   ```
3. **Borrá** el `-lcspecs` del final.
4. (Opcional pero recomendado) Para que `make test` ejecute automáticamente el binario luego de compilarlo, buscá la regla `test:` y agregá `./$(TEST)`:
   ```make
   .PHONY: test
   test: CFLAGS = $(CDEBUG)
   test: $(TEST)
       ./$(TEST)
   ```

---

## 2. Cómo Armar Correctamente un Test

Crea un archivo dentro de la carpeta `tests/` (por ejemplo: `tests/test_main.c`).

### El problema del `main()` duplicado
El `Makefile` excluye `src/main.c` durante los tests para que tu archivo de test pueda tener su propio `int main()`. Para poder probar funciones que están dentro de `main.c`, usamos un truco del preprocesador de C:

```c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// 1. Truco para incluir main.c sin que choque nuestro main()
#define main main_del_modulo
#include "../src/main.c"
#undef main

// 2. Función de test
void test_ejemplo_contexto() {
    int cant_args = 2;
    char* lista_args[2] = {"./programa", "configs/kernelMemory.config"};
    
    t_memory_context* ctx = iniciar_contexto(cant_args, lista_args);

    // 3. Verificaciones con assert
    assert(ctx != NULL);
    assert(ctx->instruction_delay == 500);
    assert(list_is_empty(ctx->programas));
    
    printf("¡Todos los tests pasaron exitosamente!\n");
}

// 4. Entry-point del test
int main() {
    test_ejemplo_contexto();
    return 0;
}
```

### Ejecución
Parado en la carpeta de tu módulo, simplemente ejecutá:
```bash
make test
```
Si el programa finaliza sin errores y ves tu `printf`, ¡significa que todos los `assert` dieron verdadero! Si un assert falla, el programa abortará e imprimirá en qué línea ocurrió el error.
