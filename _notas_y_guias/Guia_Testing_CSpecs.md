# Guía de Testing con CSpecs (BDD)

Esta guía explica cómo configurar y escribir pruebas unitarias utilizando el framework oficial adoptado por la UTN: **CSpecs**. 

## 1. Condiciones Previas y Configuración

### A. Instalar la librería CSpecs
CSpecs no viene incluido en las Commons de la materia. Debes instalarlo manualmente en tu sistema operativo:
```bash
cd /ruta/donde/quieras/descargarlo
git clone https://github.com/mumuki/cspec.git
cd cspec
make install
```

### B. Crear la carpeta de tests
Asegurate de tener una carpeta `tests/` dentro de tu módulo (ej: `kernel_memory/tests/`), ya que el `Makefile` buscará los archivos allí.

### C. Modificar el Makefile
1. Abrí el `Makefile` de tu módulo.
2. Asegurate de que la regla de compilación del test tenga el flag `-lcspecs` al final:
   ```make
   $(TEST): $(TEST_OBJS) $(DEPS) | $(dir $(TEST))
       $(CC) $(CFLAGS) -o "$@" $^ ... $(LIBS:%=-l%) -lcspecs
   ```
3. (Opcional) Para que `make test` ejecute el test automáticamente, agregá `./$(TEST)` a la regla:
   ```make
   .PHONY: test
   test: CFLAGS = $(CDEBUG)
   test: $(TEST)
       ./$(TEST)
   ```

---

## 2. Cómo Armar Correctamente un Test

Crea un archivo dentro de la carpeta `tests/` (por ejemplo: `tests/test_cspecs.c`).

### Detalles Críticos de CSpecs
1. **El orden del Include:** `<cspecs/cspec.h>` debe ser **SIEMPRE EL ÚLTIMO INCLUDE** del archivo. CSpecs define la palabra clave `end`, lo que rompería librerías como `commons/string.h` si se incluyen después.
2. **Cierre de bloques:** Todos los bloques `describe`, `it`, `before`, y `after` **no** se cierran con `}`. Se cierran usando la palabra clave `end` (o `} end`).
3. **El linter de VSCode:** IntelliSense no entiende la sintaxis de CSpecs y marcará todo en rojo. Se soluciona con un bloque de `#ifdef __INTELLISENSE__`.

### Estructura Base del Archivo

```c
#include <string.h>

// 1. Truco para poder testear funciones dentro de main.c
#define main main_del_modulo
#include "../src/main.c"
#undef main

// 2. cspec.h SIEMPRE AL FINAL
#include <cspecs/cspec.h>

// 3. Hack para que VSCode no marque falsos errores de sintaxis
#ifdef __INTELLISENSE__
    #undef context
    #define context(name) void cspec_context_##name()
    #undef describe
    #define describe(desc) 
    #undef before
    #define before 
    #undef after
    #define after 
    #undef it
    #define it(desc) 
    #undef end
    #define end 
#endif

// 4. Inicio de los tests
context(test_del_modulo) {
    describe("Probando la inicializacion") {
        
        // Variables compartidas entre los tests (it)
        t_memory_context* ctx;

        before {
            // Se ejecuta ANTES de cada 'it'
            char* lista_args[2] = {"./prog", "configs/kernelMemory.config"};
            ctx = iniciar_contexto(2, lista_args);
        } end

        after {
            // Se ejecuta DESPUÉS de cada 'it'
            if (ctx != NULL) {
                // limpiar_contexto(ctx);
            }
        } end

        it("El contexto no debe ser nulo") {
            should_ptr(ctx) not be null;
            should_ptr(ctx->logger) not be null;
        } end

        it("El delay debe ser 500 y las listas vacias") {
            should_int(ctx->instruction_delay) be equal to(500);
            should_bool(list_is_empty(ctx->programas)) be equal to(true);
            should_string(ctx->allocation_strategy) be equal to("BEST");
        } end

    } end
}
```

### Ejecución
Ejecutá:
```bash
make test
```
Verás un reporte en consola detallando cuántos tests pasaron (en verde) y cuáles fallaron (en rojo), con explicaciones claras de qué valor se esperaba y cuál se obtuvo.
