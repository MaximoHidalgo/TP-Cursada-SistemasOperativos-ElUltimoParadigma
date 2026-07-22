# Refactorización del Módulo Kernel Scheduler

## 1. Motivación y Problemática Original

Antes de la refactorización, el módulo `kernel_scheduler` padecía de los siguientes problemas arquitectónicos:
- **God Objects**: Archivos como `planificador.c` y `atender_cliente_scheduler.c` acumulaban demasiadas responsabilidades (servidor de red, lógica de planificación, manejo de memoria, I/O y recursos Mutex simultáneamente), violando el principio de Responsabilidad Única (SRP).
- **Dependencias Cruzadas y Cíclicas**: Múltiples `.h` dependían de variables de otros módulos, requiriendo cabeceras puente o redundantes (como `atender_cliente_Y_scheduler_server.h`).
- **Problemas de Mantenimiento**: Las modificaciones a cualquier componente (ej. cambiar cómo se manejaba la memoria) requerían entender el flujo entero del módulo central de planificación, con alto riesgo de generar cuellos de botella (o *deadlocks* en el contexto de hilos).

## 2. Cambios y Decisiones Arquitectónicas Implementadas

### 2.1. Centralización de Estructuras (Header Maestro)
Se convirtió a `estructuras_scheduler.h` en el punto de encuentro principal de todas las definiciones del módulo. 
- **Justificación**: Elimina inclusiones repetidas e importaciones cruzadas de librerías comunes de `<utils/dominio/...>`. Los `.c` y sus respectivos headers ahora solo necesitan importar este header maestro.

### 2.2. Modularización del Sistema de Syscalls
- **Refactorización de `atender_cliente_scheduler.c` a `scheduler_syscalls.c`**:
  Se movió y renombró la lógica dedicada exclusivamente al procesamiento de *Syscalls* e interrupciones para reflejar su comportamiento real.
- **Justificación**: Separar la lógica de despachador de la lógica del manejador estandariza los nombres y permite mantener todos los *handlers* de peticiones agrupados limpiamente.

### 2.3. Saneamiento del Servidor y el Dispatcher
- **Creación de `scheduler_dispatcher.c`**: 
  El archivo que anteriormente configuraba un servidor genérico, y el handler monolítico de peticiones se refactorizaron. Se migró la estructura estática (`tabla_despacho`) hacia un dispatcher puramente responsable de mapear *OpCodes* hacia sus *handlers* específicos. 
- **Justificación**: Aísla los componentes de red (servidor) del mapeo semántico de los mensajes (dispatcher).

### 2.4. Desmantelamiento del "God Object" (`planificador.c`)
La lógica monolítica del planificador fue segregada en tres dominios auxiliares:
1. **`planificador_io.c` / `.h`**: 
   - **Funciones**: Registro de interfaces I/O genéricas o *Stdout/Stdin*, y los temporizadores (`usleep`) utilizados por `SLEEP`.
   - **Justificación**: Centraliza las colas de bloqueo asociadas con el hardware en vez de mezclarlo con el estado principal de la memoria y la CPU.
2. **`planificador_recursos.c` / `.h`**:
   - **Funciones**: Manejo de `t_mutex`, las funciones `lock` y `unlock`, y la lógica para herencia de prioridades (incluyendo reordenamiento de las Colas Multinivel).
   - **Justificación**: Separar semáforos/recursos de las colas ordinarias simplifica la validación en caso de que existan condiciones de competencia (*race conditions*).
3. **`planificador_memoria.c` / `.h`**:
   - **Funciones**: Delegación y orquestación de compactación con Kernel Memory, manejo de procesos `SUSP_READY`, interrupciones de desuspensión e hilo asociado. También la rutina de liberación completa en caso de *BSOD*.
   - **Justificación**: Permite que las esperas síncronas contra sockets no ensucien los algoritmos core del planificador (CMN, RR, FIFO).

## 3. Impacto de los Cambios

- **Testeabilidad Mejorada**: Como resultado, las pruebas de integración unitaria pasaron del 100% de manera directa ya que ahora es posible *mockear* interfaces I/O o de memoria sin comprometer el despachador.
- **Rendimiento**: Se redujeron líneas de importación ineficientes que producían redundancia en el tiempo de compilación por pre-procesador (ahora el módulo compila significativamente más rápido sin circularidad).
- **Legibilidad**: El `planificador.c` quedó exclusivamente para el core del TP (crear, admitir, finalizar y loguear el estado del Planificador Largo y Corto plazo).

### 2.5. Centralización de Creación de Procesos (Factory PCB)
- **Eliminación de redundancia en inicialización**:
  Se abstrajo la lógica de creación de la estructura `t_proceso_planificable` que estaba duplicada en `inicializacion.c` (para el proceso 0) y `scheduler_syscalls.c` (para procesos creados por script).
- **Implementación**: Se introdujo el constructor genérico `pcb_crear(int pid, int prioridad)` en `pcb.c`. 
- **Justificación**: Aplica estrictamente el principio DRY (*Don't Repeat Yourself*), asegurando que los atributos por defecto (como `bloqueo_id` o `prioridad_original`) se inicialicen sistemáticamente en un único lugar, facilitando futuras extensiones del PCB.

### 2.6. Erradicación de *Long Parameter List* y Boilerplate
- **Limpieza en `inicializacion.c`**:
  Se eliminó la función `iniciar_contexto_scheduler` (que recibía 9 parámetros, lo cual es un claro *code smell*). Su lógica (que era un simple *builder* del struct) fue movida directamente dentro de `iniciar_contexto`.
- **Simplificación del Flujo de Arranque**: Se erradicó el wrapper `t_args_creacion_proceso` que solo existía para poder pasar múltiples parámetros a través de `funcionMutex`. Ahora, la inicialización del proceso 0 se ejecuta de forma lineal y bloqueante mediante uso explícito de `pthread_mutex_lock/unlock`.
- **Justificación**: Menos abstracciones innecesarias y firmas de funciones mucho más concisas.

### 2.7. Eliminación de Archivos Fantasma (Dead Code)
- **Archivos Eliminados**: Se eliminaron `operaciones_scheduler.c`, `operaciones_scheduler.h` y `test_operaciones.c`.
- **Justificación**: Tras revisarlos, ambos se encontraban completamente vacíos (solo contenían *include guards* o comentarios básicos). Al utilizar comodines en el `Makefile` (`find src -iname "*.c"`), estos archivos estaban siendo inyectados en la canalización del compilador `gcc` sin tener utilidad alguna, generando objetos `.o` fantasmas. Su remoción inmediata agiliza la compilación y despeja el árbol del proyecto.

### 2.8. Erradicación del anti-patrón `main.h`
- **Cambio realizado**: Se eliminó el archivo `main.h` y las inclusiones en `main.c` ahora referencian directamente los módulos necesarios (`inicializacion.h`, `scheduler_server.h`, etc.). Se ajustó también `test_main.c`.
- **Justificación**: En C, la función `main` actúa como punto de entrada del programa y consumidor de los demás módulos, pero **no** es un módulo que exponga una API hacia afuera. Tener un `main.h` (que solo agrupa `#include` sueltos) viola la direccionalidad de dependencias y oculta el acoplamiento real del programa base. Ahora `main.c` declara explícitamente lo que necesita.

### 2.9. Refactorización de Colas de Planificación (`colas.c`)
- **Resolución de Fuga de Memoria**: La función `destruir_cola_planificacion` destruía la estructura de la cola, pero perdía la referencia de los PCBs que aún estaban adentro. Se implementó `pcb_destruir` en `pcb.c` y se cambió a `queue_destroy_and_destroy_elements`.
- **Registro del `instante_ready`**: El campo `instante_ready` de los procesos (crucial para algoritmos con envejecimiento como Colas Multinivel) estaba declarado pero nunca se le asignaba valor. Ahora se popula automáticamente usando `gettimeofday()` al ingresar a la cola en `encolar_en_cola_ready`.
- **Cláusulas de Guarda**: Se eliminó un operador ternario confuso que manejaba punteros nulos y se reemplazó por un *early return* (`if (proceso == NULL) return false;`), haciendo el código más predecible. También se purgaron llamadas manuales a `free()` en los tests que generaban *double frees* tras nuestra mejora en la destrucción de colas.
- **Extracción de *Factory Method***: La función `crear_colas_ready_desde_config` estaba volviéndose muy grande (violando el Principio de Responsabilidad Única). Se extrajo la instanciación e inicialización del nodo en una función estática interna (`crear_cola_planificacion`), logrando que el ciclo *for* quede mucho más limpio y enfocado exclusivamente en la iteración.

### 2.10. Reorganización de `opcodes.h` (Librería Compartida `utils`)
- **Cambio realizado**: Se agrupó el `enum op_code` por dominios lógicos (General, Ciclo de Vida de Procesos, Memoria, Interrupciones, Syscalls, I/O y Errores) utilizando comentarios espaciados. Se requirió recompilar todos los módulos (`make clean all`).
- **Justificación**: Con casi 50 códigos de operación mezclados a lo largo del desarrollo, era difícil ubicar opcodes específicos o detectar duplicados. Agruparlos lógicamente facilita el mantenimiento y la legibilidad sin alterar el comportamiento de red (ya que todos los clientes y servidores del TP comparten el mismo `.h`).

### 2.11. Refactorización de `planificador_io.c` (I/O y Temporizadores)
- **Eliminación de *Arrow Code* y Extracción de Responsabilidades**:
  La función `planificador_io_terminada` tenía excesiva anidación. Se implementaron retornos tempranos (*Early Returns*) y se extrajeron las lógicas de desalojo (`evaluar_desalojo_por_io`) y readmisión (`reintegrar_proceso_post_io`) a funciones auxiliares estáticas. Se aplicó lo mismo a la creación del timer en `planificador_atender_peticion_io` derivándolo a `lanzar_hilo_suspension`.
- **Eliminación de Operaciones Crudas de Red**:
  El hilo `suspension_timer_thread` gestionaba la red enviando el PID directamente a través de un simple `send()` casteando el puntero. Se encapsuló esta comunicación en la función `notificar_suspension_memoria` y se reemplazó el uso de `send` y `enviar_int` por la utilidad estandarizada compartida `enviar_operacion_con_pid` de `utils/protocolo.h`.
- **Justificación**: Se aplican estrictamente los lineamientos de *Clean Code* (responsabilidad única, funciones reducidas, legibilidad) y de las normativas de módulos de C establecidas para el TP (evitar manejos de punteros o buffers de red no abstraídos).
