# Auditoría del Módulo: CPU

Este documento recopila las comprobaciones y justificaciones del código fuente actual del módulo CPU contra las especificaciones de la consigna "Plug & Pray".

## 1. Lineamiento e Implementación Base

### Recepción del Identificador por Argumento
> *Al momento de ejecutar un CPU se debe indicar por argumento cuál es su “identificador” para poder identificarla de los demás.*
> *➜ ~ ./bin/cpu [Archivo Config] [Identificador]*

**Estado: Cumplido ✅**

**Justificación:**
En el archivo `cpu/src/cpu_utils.c` (dentro de la función `iniciar_contexto`), se recupera el identificador de la CPU a partir del tercer argumento (`argv[2]`):
```c
ctx->id_cpu = strdup(argv[2]);
char* path_config = argv[1];
```
Además, en `utils/src/utils/dominio/estructuras.c` (función `validar_argumentos`), se fuerza que la cantidad de argumentos sea estrictamente 3 (el path del binario + los 2 parámetros), abortando la ejecución con el mensaje de uso correcto si no se cumple:
`"Cantidad de argumentos incorrecta. Uso correcto: ./bin/modulo [Archivo Config] [Id]"`

---

### Ciclo de Instrucción (Fetch, Decode, Execute, Check Interrupt)
> *El módulo CPU es el encargado de interpretar y ejecutar las instrucciones recibidas [...] ejecutará un ciclo de instrucción simplificado que cuenta con los pasos: Fetch, Decode, Execute y Check Interrupt.*

**Estado: Cumplido ✅**

**Justificación:**
En el archivo `cpu/src/ciclo_instruccion.c`, la función `ejecutar_ciclo_de_instruccion()` modela explícitamente y en orden las 4 fases dentro de un bucle `while(!desalojar)`:

1. **Fetch**: Pide la instrucción a la memoria basándose en el PC. También emite el log obligatorio de la fase.
   ```c
   char* instruccion_texto = fetch(contexto, ctx);
   ```
2. **Decode**: Parsea la instrucción aislando su identificador de sus posibles parámetros.
   ```c
   t_instruccion* instruccion_decodificada = decode(instruccion_texto);
   ```
3. **Execute**: Busca la instrucción decodificada en la tabla de handlers (tabla de despacho) y la ejecuta.
   ```c
   desalojar = execute(instruccion_decodificada, contexto, ctx);
   ```
4. **Check Interrupt**: Si el proceso no fue desalojado por la instrucción actual, utiliza la API de sockets POSIX con el flag `MSG_PEEK | MSG_DONTWAIT` para leer el socket del Scheduler de manera no bloqueante y comprobar si llegó un aviso de fin de quantum.
   ```c
   if (!desalojar) {
       int hubo_interrupcion = chequear_interrupciones(ctx);
       if (hubo_interrupcion) {
           // ... desaloja
       }
   }
   ```

---

### Estrategia de Descubrimiento de Memory Sticks (Dinámico)
> *Las CPUs deberán conectarse al Kernel Scheduler, al Kernel Memory y a los diferentes Memory Sticks. Estos últimos se podrán ir conectando a lo largo de la ejecución, por lo que es importante que el grupo defina una estrategia entre el Kernel Memory y las CPUs para que éstas conozcan todos los Memory Sticks que existen cuando sea necesario.*

**Estado: Cumplido ✅ (Implementado Lazy Discovery)**

**Justificación y Estrategia del Grupo:**
Para evitar que el *Kernel Memory* esté *spameando* asincrónicamente a la CPU (lo cual complicaría el manejo de sockets en la CPU, ya que su hilo principal bloquea en la recepción del Kernel Scheduler o durante Fetchs), el grupo optó por una estrategia de descubrimiento "perezoso" o bajo demanda (*Lazy Discovery*):

1. Durante el arranque, la CPU recibe del *Kernel Memory* la tabla inicial de *Memory Sticks* y se conecta a los disponibles (vía `ENVIO_TABLA_STICKS`).
2. Si un *Memory Stick* nuevo se conecta al sistema, la CPU no se entera inmediatamente.
3. Cuando la MMU de la CPU intenta traducir y escribir/leer una dirección de memoria, primero busca en su tabla de sticks locales conocidos (`ctx->memory_sticks`).
4. **Cache Miss**: Si la dirección física no encaja en ningún stick conocido (es decir, `stick_target == NULL` en `mmu.c`), la CPU asume que podría haber un stick nuevo. 
5. En ese instante preciso, la CPU envía un `SOLICITUD_TABLA_STICKS` al Kernel Memory. 
6. El Kernel Memory responde devolviendo la tabla completa actualizada.
7. La función `procesar_tabla_de_sticks` de la CPU itera esta tabla. Si encuentra IDs de sticks que **ya existen** en su lista local, los ignora silenciosamente. Si encuentra IDs nuevos, se conecta a ellos, los agrega a su lista, e intenta realizar la operación de memoria una vez más.
8. Si tras la actualización el stick sigue sin existir, se levanta el error formal de *Segmentation Fault* por violación de límites del hardware.

Esta estrategia asegura que la CPU siempre pueda acceder a los Memory Sticks necesarios *"cuando sea necesario"*, optimizando los mensajes de red.

---

## 2. Ejecución y Memoria

### Espera de PID y Petición de Contexto
> *Una vez establecidas todas las conexiones, la CPU se quedará a la espera de recibir un PID de parte del Kernel Scheduler. Una vez recibido, la CPU deberá solicitarle al Kernel Memory el contexto de ejecución y comenzar el primer ciclo de instrucción.*

**Estado: Cumplido ✅**

**Justificación:**
El hilo principal del programa delega su control a la función `atender_kernel_scheduler` (en `cpu_utils.c`), la cual se bloquea iterativamente esperando un mensaje en el socket del *Kernel Scheduler*:
```c
while (1) {
    log_info(ctx->logger, "Esperando un nuevo PID del Kernel Scheduler...");
    int process_ID = recibir_operacion(ctx->conexiones->scheduler);
    // ...
```
Cuando llega un `PID_A_EJECUTAR`, se desencadena la función `manejar_ejecucion_proceso`, que inmediatamente pide a la Memoria el contexto y, si llega correctamente, arranca el ciclo:
```c
t_contexto_ejecucion* contexto = pedir_contexto_a_memoria(pid_recibido, ctx);
if (contexto != NULL) {
    ejecutar_ciclo_de_instruccion(contexto, ctx);
    free(contexto);
}
```

### Simulación de MMU y Traducción de Direcciones
> *A la hora de ejecutar instrucciones que requieran interactuar directamente con la Memoria, tendrá que traducir las direcciones lógicas (propias del proceso) a direcciones físicas (propias de la memoria). Para ello simulará la existencia de una MMU.*

**Estado: Cumplido ✅**

**Justificación:**
La CPU cuenta con su propio módulo interno en `cpu/src/mmu.c`. Al ejecutar instrucciones de memoria (como `MOV_IN`, `MOV_OUT`, o `COPY_MEM` en `instrucciones.c`), se utiliza la dirección lógica (proveniente de un registro como `DI` o `SI`) y se invoca a la función `mmu_traducir`:
```c
if (mmu_traducir(ctx, contexto, dir_logica, tam, ctx->tam_max_segmento, &dir_fisica) == -1) {
    // Manejo de Error: Segmentation Fault
} else {
    // Acceso mediante la dirección física
}
```
Internamente, `mmu_traducir` busca a qué segmento pertenece la dirección haciendo una división por el `tam_max_segmento`, revisa la tabla de segmentos del contexto actual e incluye validaciones estrictas de tamaño y existencia del segmento antes de arrojar la base física sumada al desplazamiento. Todo se comporta exactamente igual a un componente de hardware MMU estándar.

---

## 3. Arquitectura Interna de CPU

### Registros de CPU
> *En la implementación de nuestra CPU, se utilizará una serie de registros para poder modelar la operatoria de una CPU real simplificada [...]. En la siguiente tabla está el detalle de los registros que deberá tener nuestra CPU, en la cual estará detallado el tamaño del mismo y qué tipo de dato se recomienda para su implementación.*

**Estado: Cumplido ✅**

**Justificación:**
El archivo `utils/src/utils/dominio/estructuras.h` contiene la definición estricta de la estructura `t_registros`, coincidiendo exactamente en nombres, tipos de dato y tamaños con la tabla de la consigna:
- `PC` (Program Counter): `uint32_t` (4 bytes)
- `AX`, `BX`, `CX`, `DX`: `uint8_t` (1 byte c/u)
- `EAX`, `EBX`, `ECX`, `EDX`: `uint32_t` (4 bytes c/u)
- `SI`, `DI`: `uint32_t` (4 bytes c/u)

Además, las funciones de acceso dinámico ubicadas en `cpu/src/registros.c` (`leer_registro`, `asignar_valor_a_registro`, y `obtener_tamanio_registro`) se encargan de enrutar las lecturas y castear las asignaciones a `(uint8_t)` en los casos correspondientes de 1 byte (truncando automáticamente los excesos).
Se verificó y se **agregó la compatibilidad explícita del registro `PC`** dentro del despachador dinámico por si alguna instrucción en la tabla de ejecución (como un hipotético `SET PC 5`) intenta modificarlo directamente a través del motor genérico de registros, previniendo así un bug oculto.

> [!NOTE] 
> Todo el flujo fue re-validado ejecutando la suite de testing unitario (`make test`) y probando la ejecución de interconexión full-stack (`make all && make run && make stop`), sin detectarse regresiones ni memory leaks.
