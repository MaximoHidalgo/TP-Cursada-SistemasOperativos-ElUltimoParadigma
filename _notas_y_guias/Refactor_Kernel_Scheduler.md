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
