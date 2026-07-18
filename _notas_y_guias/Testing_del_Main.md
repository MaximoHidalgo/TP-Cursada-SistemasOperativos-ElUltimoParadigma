# Buenas Prácticas: ¿Se debe testear la función `main`?

Al desarrollar en C (y en la mayoría de lenguajes bajo filosofías de Clean Code y TDD), una pregunta frecuente es: *¿Debo escribir pruebas unitarias para mi archivo `main.c`?*

La respuesta corta es: **Casi nunca.**

A continuación se explica la justificación arquitectónica detrás de esta regla, con ejemplos claros de cuándo evitarlo y las raras excepciones donde podría tener sentido.

---

## ❌ Cuándo NO se debe testear el `main.c`

Si aplicaste buenas prácticas de diseño y mantuviste tu `main.c` "delgado" (es decir, actuando solo como punto de entrada o *entrypoint*), testearlo unitariamente es un anti-patrón.

Razones principales para **no** testearlo:

### 1. Bucles Infinitos Bloqueantes
Los procesos servidores (como Kernel, CPU, Memoria) suelen tener un `while(1)` o un bloqueador (como un `sem_wait` infinito o un `epoll`). 
- **El problema**: Las herramientas de Unit Testing (como `cspecs` o `CUnit`) esperan que las funciones retornen para poder hacer los `asserts`. Si llamas al `main()`, el framework de testing se quedará "colgado" para siempre esperando que termine el loop.
- **El parche inútil**: Para salir del loop, te verías obligado a forzar errores inyectados por mocks (ej: que un socket retorne `-1`). Esto significa que estás testeando **el flujo de error** para poder salir, y no el caso de éxito de tu programa.

### 2. Exceso de Mocks y Tests Frágiles
Si el `main` solo llama a otras funciones (ej: `iniciar_contexto()`, `crear_servidor()`, `manejar_cliente()`), para testearlo de forma aislada tendrías que mockear (simular) **todas** esas dependencias.
- **El problema**: Terminarás con un test enorme que solo verifica que "el main llama a la función X y luego a la función Y". A esto se le llama un test acoplado a la implementación.
- **La consecuencia**: Si decides cambiar el orden de inicialización en el futuro, el programa seguirá funcionando perfectamente, pero tu test unitario se romperá, obligándote a reescribirlo.

### 3. Duplicación de Testing
Si extrajiste la lógica a un archivo de utilidades (ej: `cpu_utils.c`) y ya le hiciste pruebas exhaustivas a `iniciar_contexto` y `manejar_ejecucion_proceso`...
- **El problema**: Testear el `main` que simplemente llama a esas funciones es testear exactamente lo mismo dos veces. No estás verificando código nuevo.

---

## ✅ Cuándo SÍ podría tener sentido testearlo

Existen casos excepcionales (generalmente en programas tipo CLI o *Command Line Interfaces* de corta duración) donde testear el `main` (o partes de él) tiene sentido, aunque generalmente se recomienda extraer esa lógica a una función tipo `run_app(argc, argv)`.

Podría testearse unitariamente si:

### 1. Parseo Complejo de Argumentos
Si tu `main` se encarga de validar combinaciones complejas de argumentos por consola (`argv`), y por alguna razón no extrajiste esta lógica a una función `parse_args()`.
- **Ejemplo**: El `main` decide arrancar en modo seguro, modo verbose, o con un archivo de configuración distinto según lo que recibe en `argc` y `argv`. (Aún así, siempre es mejor extraer esa lógica a una función `validar_argumentos(argc, argv)` y testear esa función).

### 2. Retornos de Códigos de Error Críticos al SO
Si tu programa se utiliza en scripts de Bash que dependen fuertemente del código de salida (`return 0`, `return 1`, `exit(EXIT_FAILURE)`).
- **Ejemplo**: Un programa corto que no tiene bucle infinito, que hace una tarea puntual (ej: procesar un archivo y terminar) y que debe retornar un código de error específico si falla la lectura.

---

## 🎯 La Regla de Oro (Clean Architecture)

> **"La lógica de negocio se testea con Pruebas Unitarias. El Entrypoint y el ensamblado del sistema se testean con Pruebas de Integración."**

**Cómo estructurar tu código para evitar testear el `main`:**
1. **Mueve toda la lógica** a archivos separados (ej: `cpu_utils.c`, `inicializacion.c`).
2. Hazles **pruebas unitarias exhaustivas** a esos archivos.
3. Deja el `main.c` lo más tonto posible: 
   ```c
   // Ejemplo del "Main Perfecto" (Clean Code)
   #include "cpu_utils.h"

   int main(int argc, char* argv[]) {
       t_contexto* ctx = inicializar(argc, argv);
       if (!ctx) return EXIT_FAILURE;
       
       iniciar_servidor(ctx); // <- Entra al loop infinito
       
       destruir_contexto(ctx);
       return EXIT_SUCCESS;
   }
   ```
4. Comprueba que el `main.c` funciona correctamente levantando todo el sistema completo en una **Prueba de Integración** (E2E), conectándolo con los demás módulos reales.
