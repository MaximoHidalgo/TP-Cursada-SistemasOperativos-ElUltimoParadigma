# Documentación: Implementación de Desalojos (Prioridad y Compactación)

Este documento detalla los cambios realizados en el sistema para soportar el desalojo de procesos de la CPU debido a dos motivos principales:
1. **Llegada de un proceso de mayor prioridad a Ready** (Interrupción por Desalojo).
2. **Necesidad de Compactación en Memoria** (Interrupción por Compactación).

## 1. Modificación de OpCodes (`utils/src/utils/dominio/opcodes.h`)

Se agregaron tres nuevos códigos de operación (OpCodes) para permitir la comunicación entre la CPU, el Scheduler y la Memoria en estos nuevos escenarios:

```c
	INTERRUPCION_DESALOJO,
	INTERRUPCION_COMPACTACION,
	DESALOJO_COMPACTACION
```

## 2. Correcciones de Seguridad en la CPU (`cpu/src/ciclo_instruccion.c`)

El código base que había sido sugerido inicialmente contenía una **Condición de Carrera (Race Condition)** crítica en el manejo del buffer del socket. 

### ¿Qué se corrigió?
El código sugerido consumía el payload de la interrupción sin leer el PID. Si una interrupción por `FIN_DE_QUANTUM` sufría un retraso en la red y llegaba justo cuando la CPU acababa de planificar un proceso *nuevo*, la CPU hubiera desalojado al proceso incorrecto (ya que no verificaba a quién iba dirigida la interrupción).

**Solución aplicada:**
Se mantuvo intacta la validación original de la función `manejar_interrupcion_quantum`, la cual desempaqueta el PID y valida `if (pid_interrumpido == pid_actual)`. Solamente modificamos su retorno para que ahora devuelva el OpCode exacto (`INTERRUPCION_QUANTUM`) en lugar de un booleano, permitiendo diferenciar el motivo de la interrupción.

### Refactorización del Ciclo de Instrucción
Se adaptó el código para que escuche todos los motivos y notifique al Scheduler apropiadamente. Además, tras una indicación posterior, el encadenamiento de `if-else` fue refactorizado a un `switch` mucho más limpio:

```c
                switch (motivo_interrupcion) {
                    case INTERRUPCION_QUANTUM:
                        log_info(ctx->logger, "## Interrupción por Fin de Quantum recibida para PID %d", contexto->pid);
                        notificar_desalojo_a_scheduler(contexto->pid, FIN_DE_QUANTUM, ctx);
                        break;
                    case INTERRUPCION_DESALOJO:
                        notificar_desalojo_a_scheduler(contexto->pid, INTERRUPCION_DESALOJO, ctx);
                        break;
                    case INTERRUPCION_COMPACTACION:
                        log_info(ctx->logger, "## Interrupción por Compactación recibida para PID %d", contexto->pid);
                        notificar_desalojo_a_scheduler(contexto->pid, DESALOJO_COMPACTACION, ctx);
                        break;
                }
```

## 3. Integración en el Kernel Scheduler (`kernel_scheduler/src/atender_cliente_scheduler.c`)

El Scheduler no estaba preparado para recibir las devoluciones de ejecución bajo estos nuevos códigos, ni tampoco para solicitar interrupciones por compactación.

### ¿Qué se agregó?
1. **Envío asíncrono de interrupción:** Cuando el Kernel recibe un `SOLICITUD_COMPACTACION` de la Memoria (durante un `MEM_ALLOC`), si hay un proceso corriendo en la CPU, ahora el Kernel le envía de inmediato el opcode `INTERRUPCION_COMPACTACION` a la CPU a través de su socket.
2. **Nuevos Handlers de Recepción:** Se añadieron dos funciones al Kernel para atajar a la CPU cuando se detiene:
   - `procesar_desalojo_por_prioridad`: Encola el proceso en Ready nuevamente (tras ser interrumpido por un proceso de mayor prioridad).
   - `procesar_desalojo_por_compactacion`: Encola el proceso en Ready, pero adicionalmente **le avisa a la Memoria mediante `RESPUESTA_COMPACTACION` que la CPU ya se detuvo por completo**, dándole luz verde para proceder a mover bloques sin miedo a pisar información.

## Conclusión
El sistema ahora soporta desalojos dinámicos garantizando la integridad de los datos. Al detener asíncronamente a la CPU antes de que Memoria proceda a compactar, y validando estrictamente los PIDs de los paquetes de red recibidos, se evitan comportamientos indefinidos y "segmentation faults" lógicos que abundan típicamente en este trabajo práctico.
