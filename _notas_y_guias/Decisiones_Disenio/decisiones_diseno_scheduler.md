# Decisiones de Diseño: Modelo de 7 Estados

Este documento justifica las decisiones arquitectónicas tomadas por el grupo para la implementación del **Kernel Scheduler**, cumpliendo con los requisitos del TP de gestionar correctamente un modelo de **7 estados de planificación de procesos** (NEW, READY, EXEC, BLOCK, SUSP_READY, SUSP_BLOCK, EXIT).

## 1. Representación de Estados en Memoria

Para representar fehacientemente en qué momento de su ciclo de vida se encuentra cada proceso, se decidió utilizar una enumeración tipada (`t_estado_proceso`) dentro del PCB (`pcb.h`). Esto garantiza la seguridad de tipos y facilita la lectura del código (Clean Code).

**Los 7 estados están implementados así:**
```c
typedef enum {
    ESTADO_PROCESO_NEW,
    ESTADO_PROCESO_READY,
    ESTADO_PROCESO_EXEC,
    ESTADO_PROCESO_BLOCK,
    ESTADO_PROCESO_EXIT,
    ESTADO_PROCESO_SUSP_READY,
    ESTADO_PROCESO_SUSP_BLOCK
} t_estado_proceso;
```

## 2. Gestión de Colas Estructuradas

En lugar de tener una gran lista general que haya que iterar constantemente buscando el estado de cada proceso, la decisión de diseño principal fue utilizar **estructuras de datos segregadas por estado** en `t_estratega_planificacion` y en los recursos:

*   **Estado NEW**: Utiliza una cola FIFO dedicada (`t_queue* cola_new`). El planificador de largo plazo simplemente hace `queue_pop` cuando se evalúa la admisión al sistema.
*   **Estado READY**: Dado que se soportan diferentes algoritmos (Colas Multinivel, Round Robin, FIFO), el estado `READY` se implementa dinámicamente mediante el atributo `t_list* colas`, donde cada elemento puede ser una cola con un nivel de prioridad específico.
*   **Estado EXEC**: Representado a través de un único puntero `t_proceso_planificable* proceso_exec` (y el posterior despacho mediante envío a la lista de `sockets_cpu`). Al existir solo una instancia del TP actual (o limitadas CPUs), tener el puntero directo permite una validación en tiempo constante $O(1)$ para saber quién ocupa los recursos.
*   **Estado BLOCK**: Para modelar procesos esperando eventos de manera independiente, se delegó la cola de bloqueados a cada recurso (IO y Mutex). Cada `t_interfaz_io` y `t_mutex` tiene su propia `t_queue* cola_bloqueados`. Esto evita el *starvation* falso y asegura que cuando un I/O termina, se desbloquea exactamente el proceso correspondiente sin afectar al resto.
*   **Estados de Suspensión (SUSP_READY y SUSP_BLOCK)**: Se crearon listas dedicadas `t_list* cola_susp_ready` y `t_list* cola_susp_blocked` en el estratega. Se utilizó `t_list` en vez de `t_queue` aquí porque la consigna requería lógica de ordenación por prioridad al "desuspender", utilizando la función custom `comparador_procesos_suspendidos`.

## 3. Transiciones e Interacción con Kernel Memory

Las transiciones entre estados se gestionan mediante funciones específicas que mutan tanto el atributo `estado` del PCB, como su ubicación en la memoria del Scheduler. 

### Bloqueo y Suspensión
- Cuando un proceso pide memoria (`MEM_ALLOC`) y no hay suficiente, transiciona a `BLOCK` (y queda esperando).
- Como mecanismo para evitar deadlocks por memoria (planificación de mediano plazo), se evalúan los procesos `BLOCK` para ser suspendidos. Aquellos seleccionados transicionan a `SUSP_BLOCK` y se envían a Memoria para que sean desalojados a Swap, liberando así espacio para los procesos en `READY`.

### Despertar y Desuspender
- Si una operación IO finaliza y el proceso estaba en `BLOCK`, transiciona a `READY`.
- Si estaba en `SUSP_BLOCK`, el proceso solo conoce que el evento terminó, así que transiciona a `SUSP_READY` y queda aguardando que el Planificador de Mediano Plazo lo devuelva a Memoria Principal (cargándolo nuevamente a `READY`).

## 4. Planificación de Corto Plazo

- **Ignorar Prioridades en FIFO y RR**: Siguiendo la restricción de que "en los algoritmos de FIFO y RR la prioridad no tendrá injerencia alguna", la admisión se diseñó para interceptar el PCB y sobreescribir su prioridad a `0` si el algoritmo global no es CMN. Esto garantiza que todos los procesos compitan en igualdad de condiciones en la única cola existente.
- **Rechazo *Early Exit* en Colas Multinivel**: Si el algoritmo es CMN y un proceso llega con una prioridad fuera del rango definido (`list_size`), no se lo planifica ni se lo intenta adaptar. Se optó por una estrategia de *Early Exit* (salida temprana) durante la admisión: se loggea el error, se marca al proceso como `EXIT`, se libera el PCB y se notifica inmediatamente a la Memoria (`FINALIZAR_PROCESO`) para que libere cualquier estructura reservada, evitando que el proceso zombi ocupe recursos.
- **Desalojo Activo**: Se evalúa `planificador_debe_desalojar` en dos puntos clave de inyección a READY: la admisión de nuevos procesos (`NEW -> READY`) y el despertar tras IO/Bloqueo (`BLOCK -> READY`). Esto permite interrumpir proactivamente a la CPU (OpCode `INTERRUPCION_DESALOJO`) si el nuevo proceso tiene mayor prioridad (menor número).

## 5. Planificación de Mediano Plazo

- **Suspensión por Timeout (BLOCK -> SUSP_BLOCK)**: Para evitar bloqueos (espera pasiva) en el módulo que frenen la escucha de nuevos clientes, la transición se implementó creando un hilo independiente (`suspension_timer_thread`) por cada proceso que se bloquea. Este hilo ejecuta un `usleep` no bloqueante para el resto del Scheduler. Si al despertar el proceso sigue en `BLOCK`, se le asigna un *timestamp* (`instante_suspension`) capturado con `gettimeofday` para futuros desempates y se emite la orden de suspensión a la Memoria.
- **Desuspender (Memoria Expandida/Liberada)**: La lógica de des-suspensión recorre la lista de suspendidos evaluando espacio sin compactar mediante una consulta (`CONSULTAR_ESPACIO_LIBRE`) al Kernel Memory. Esto encapsula la lógica de evaluación espacial estrictamente en la Memoria. En caso de tener misma prioridad, el proceso se desempata correctamente evaluando su `instante_suspension` (el que lleve mayor tiempo suspendido).

## 6. Planificación de Largo Plazo y Gestión de Fallos (BSOD)

- **Admisión (NEW -> READY)**: Debido a que los procesos inician sin restricciones de memoria, la transición de `NEW` a `READY` se implementó de forma directa mediante la función `planificador_admitir_a_ready()`, extrayendo de la `cola_new` sin evaluaciones complejas.
- **Corrupción de Memoria (BSOD)**: Cuando el Kernel Scheduler recibe el evento de `CORRUPCION_MEMORIA` emitido por el Kernel Memory, se decidió implementar una finalización en cascada (graceful shutdown forzado). A través de la función `planificador_finalizar_todo_por_bsod()`, se barren todas las colas de manera controlada y se finalizan todos los procesos asignándoles el estado `ESTADO_PROCESO_EXIT`, dejando rastro en los logs para su posterior análisis. Finalmente, se loggea explícitamente la caída del Scheduler y se detiene la ejecución del módulo, liberando todos los contextos de red y memoria (`limpiar_contexto()`).

## 7. Refactorizaciones y Clean Code (Hardening)

- **Estandarización de Logs Automatizados**: Para cumplir con las expresiones regulares de evaluación automática de la cátedra, se refactorizó la emisión de logs en el Scheduler. En lugar de ensuciar los handlers con *strings* hardcodeados, se utilizaron funciones auxiliares puras (ej. `obtener_nombre_syscall`) para traducir de manera declarativa los OpCodes a texto legible.
- **Robustez en Parsing de Configuraciones**: Se reemplazó el parseo anidado manual por llamadas directas a utilidades nativas (`log_level_from_string`) y helpers customizados (`config_get_bool_compatible`). Esto abstrae el parseo de strings (y el manejo de *case-insensitiveness*) dejando la estructura `main` puramente orquestacional.
- **Orquestación Estricta de IO a Memoria (STDIN/STDOUT)**: Se garantizó que los módulos IO se comporten como "periféricos tontos". No poseen conexión directa por socket a Memoria. Ante peticiones STDIN/STDOUT, el Scheduler funge como nexo orquestador exclusivo (recibe la solicitud de IO, despacha a Memoria, recibe la respuesta de Memoria, y luego reenvía a IO). Esto protege el encapsulamiento y el paradigma cliente-servidor de microservicios.


## Conclusión

El diseño segregado en múltiples colas asociadas y punteros en el estratega garantiza el cumplimiento nativo de los 7 estados. Esto simplifica la semántica de desalojos, bloqueos y minimiza la complejidad algorítmica a la hora de buscar el próximo proceso a despachar, cumpliendo totalmente con la robustez que la cátedra exige.
