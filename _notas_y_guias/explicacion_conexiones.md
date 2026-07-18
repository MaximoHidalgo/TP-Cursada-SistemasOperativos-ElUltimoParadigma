# Arquitectura de Conexiones del Sistema - "El Último Paradigma"

Este documento detalla la naturaleza de la comunicación (sincrónica vs asincrónica) entre los distintos módulos del sistema, basándose en la arquitectura del proyecto y en el código fuente actual.

## 1. Conceptos Básicos

- **Comunicación Sincrónica (Bloqueante):** El módulo emisor realiza una petición y se queda **bloqueado** esperando la respuesta antes de continuar con su ejecución. Es un modelo clásico de "Petición -> Respuesta".
- **Comunicación Asincrónica (No Bloqueante / Por Eventos):** El módulo emisor envía un mensaje y sigue con su trabajo, o bien un módulo recibe un mensaje/notificación inesperada en cualquier momento (un evento o interrupción) y debe reaccionar a ella sin haberla solicitado de antemano.

---

## 2. Análisis por Conexión (Módulo a Módulo)

### 🟢 CPU -> Kernel Scheduler
- **Conexión:** CPU es cliente del Kernel Scheduler.
- **Naturaleza:** **Mixta** (Principalmente Sincrónica con Interrupciones Asincrónicas).
  - **Sincrónica:** Cuando la CPU termina su ráfaga (fin de quantum, syscall, etc.), envía el contexto de ejecución al Scheduler y se queda bloqueada esperando que este le despache el próximo proceso a ejecutar.
  - **Asincrónica:** El Scheduler puede enviarle a la CPU una **Interrupción de Desalojo** en cualquier momento (por herencia de prioridades o algoritmos expropiativos). La CPU debe recibir este evento de forma asincrónica, detener su ciclo de instrucción y devolver el contexto.

### 🟢 CPU -> Kernel Memory
- **Conexión:** CPU es cliente de Kernel Memory.
- **Naturaleza:** **Sincrónica**.
  - Durante el ciclo de instrucción (ej. `MOV_IN`, `MOV_OUT`), la CPU realiza una petición de lectura/escritura a la Memoria y se bloquea completamente esperando que la Memoria resuelva la dirección física y le devuelva el dato o la confirmación.

### 🟤 IO -> Kernel Scheduler
- **Conexión:** Las interfaces de IO son clientes del Scheduler.
- **Naturaleza:** **Sincrónica orientada a eventos**.
  - El módulo de IO se conecta al Scheduler y queda a la espera. Cuando un proceso hace una petición bloqueante (ej. `SLEEP`), el Scheduler le delega el trabajo al módulo IO. El IO realiza su trabajo simulado y responde al Scheduler.

### 🔵 Kernel Scheduler -> Kernel Memory
- **Conexión:** El Kernel Scheduler es cliente de Kernel Memory.
- **Naturaleza:** **Híbrida (Asincrónica Bidireccional)**.
  - **Sincrónica (Flujo normal):** El Scheduler realiza peticiones como `SYSCALL_MEM_ALLOC` o intenta desuspender un proceso, y se bloquea esperando un código de respuesta u OK de la Memoria.
  - **Asincrónica (Eventos):** La Memoria puede enviar el evento `MEMORIA_EXPANDIDA` al Scheduler en cualquier momento. Al ocurrir esto mientras el Scheduler espera una respuesta sincrónica, se requiere drenar el buffer del socket (patrón *Drain the Buffer* implementado en `esperar_respuesta_memoria`) para evitar desincronizaciones del protocolo.

### 🟠 Memory Stick -> Kernel Memory
- **Conexión:** Memory Stick se conecta a Kernel Memory.
- **Naturaleza:** **Asincrónica (Notificación Plug & Play)**.
  - Al inicializarse un nuevo módulo Memory Stick, este se conecta a la Memoria y le notifica sobre su existencia y tamaño disponible de forma inmediata y asincrónica, lo que dispara internamente el evento de expansión.

### ⚪ Kernel Memory -> SWAP
- **Conexión:** Kernel Memory actúa interactúa con el sistema de SWAP (archivos).
- **Naturaleza:** **Sincrónica**.
  - Cuando la Memoria Principal necesita restaurar segmentos suspendidos (ej. en la función `restaurar_segmentos`), envía una solicitud de `LECTURA` bloque por bloque al módulo SWAP y se queda bloqueada (`recibir_operacion`) esperando el buffer de datos correspondiente al bloque solicitado desde el disco.

---

### Resumen Arquitectónico
- **Sincronismo puro:** `Pedir dato -> Esperar dato` (CPU a Memoria, Memoria a SWAP).
- **Asincronismo puro:** `Notificar evento -> Seguir operando` (Memory Stick a Memoria).
- **Asincronismo por Interrupción:** `Señal inesperada -> Detener operación actual` (Scheduler a CPU, Memoria a Scheduler).
