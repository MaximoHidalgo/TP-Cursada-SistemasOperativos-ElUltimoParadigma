# Notas sobre scheduler_dispatcher

## Contexto general
Este archivo contiene el dispatcher del Scheduler, donde se reciben y manejan los distintos opcodes que llegan desde la CPU y desde Kernel Memory.

En particular, la función `handler_desalojo_compactacion` se usa cuando Kernel Memory solicita compactación y la CPU debe liberar el proceso que estaba ejecutando.

## Nota importante sobre `handler_desalojo_compactacion`
En esta función el scheduler toma el proceso actualmente en `EXEC` y lo vuelve a poner en estado `READY`.
El TP pide que, en este caso, el proceso desalojado quede al principio de la cola Ready, porque se trata de un proceso que fue interrumpido por el evento de compactación.

El comentario en tu código explica bien por qué se eligió esta solución:
- `t_queue` de commons solo permite `queue_push`, que agrega elementos al final.
- el código rompe el encapsulamiento de `t_queue` accediendo a su lista interna `elements`.
- luego usa `list_add_in_index(..., 0, desalojado)` para insertar el proceso en el índice 0.

Esto es un hack consciente: no es una operación estándar de la biblioteca, pero sirve para cumplir el requisito del enunciado.

## Por qué se hizo así
- El enunciado del TP indica que el proceso desalojado por compactación debe ir al principio de la cola Ready.
- No hay función de Commons para insertarlo al frente.
- Por eso se manipula directamente la lista interna de `t_queue`.

## Recomendación
Mantener este comentario como documentación del diseño. Explica que se tomó una decisión deliberada, no un error, y que se prioriza el cumplimiento del enunciado sobre la pureza del encapsulamiento.
