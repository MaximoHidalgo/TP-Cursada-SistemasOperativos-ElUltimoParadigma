.PHONY: all run stop clean fueraLogs debug release test

# Argumentos definidos para la prueba
PATH_PSEUDOCODIGO = MEMORIA_PRE_0.prc
ID_CPU = CPU_1
TAM_STICK = 1024
TIPO_IO = IO_STDIN

# Archivos de configuración a utilizar (relativos a la carpeta configs/ de cada módulo)
CONFIG_SCHEDULER = scheduler.config
CONFIG_MEMORY = kernelMemory.config
CONFIG_SWAP = swap.config
CONFIG_STICK = memory_stick.config
CONFIG_CPU = cpu.config
CONFIG_IO = io.config

# Variable con el comando para abrir una nueva terminal.
# Usamos xterm con -hold para que la ventana NO se cierre si el módulo crashea.
TERMINAL = xterm -hold -e bash -c


# Regla para compilar todos los módulos a la vez
all:
	@echo "Compilando todos los módulos..."
	$(MAKE) -C kernel_memory
	$(MAKE) -C kernel_scheduler
	$(MAKE) -C swap
	$(MAKE) -C memory_stick
	$(MAKE) -C cpu
	$(MAKE) -C io

# Regla para levantar todo en terminales separadas
run:
	@echo "1. Levantando Kernel Memory..."
	$(TERMINAL) "cd kernel_memory && ./bin/kernel_memory ./configs/$(CONFIG_MEMORY)" &
	sleep 2

	@echo "2. Levantando Kernel Scheduler..."
	$(TERMINAL) "cd kernel_scheduler && ./bin/kernel_scheduler ./configs/$(CONFIG_SCHEDULER) $(PATH_PSEUDOCODIGO)" &
	sleep 2

	@echo "3. Levantando SWAP y Memory Stick..."
	$(TERMINAL) "cd swap && ./bin/swap ./configs/$(CONFIG_SWAP)" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/$(CONFIG_STICK) $(TAM_STICK)" &
	sleep 1

	@echo "4. Levantando Clientes (CPU e IO)..."
	$(TERMINAL) "cd cpu && ./bin/cpu ./configs/$(CONFIG_CPU) $(ID_CPU)" &
	$(TERMINAL) "cd io && ./bin/io ./configs/$(CONFIG_IO) STDIN" &
	$(TERMINAL) "cd io && ./bin/io ./configs/$(CONFIG_IO) STDOUT" &
	$(TERMINAL) "cd io && ./bin/io ./configs/$(CONFIG_IO) SLEEP" &

	@echo "¡Se abrieron 6 terminales independientes!"

# Regla para detener todos los procesos fácilmente
stop:
	@echo "Deteniendo todos los módulos del TP..."
	-killall kernel_memory kernel_scheduler swap memory_stick cpu io
	-killall xterm

# Regla para limpiar binarios y objetos
clean:
	@echo "Limpiando archivos compilados de todos los módulos..."
	$(MAKE) -C kernel_memory clean
	$(MAKE) -C kernel_scheduler clean
	$(MAKE) -C swap clean
	$(MAKE) -C memory_stick clean
	$(MAKE) -C cpu clean
	$(MAKE) -C io clean
	@echo "¡Limpieza completa!"

# Regla explícita para compilar en modo DEBUG (para probar con Valgrind)
debug:
	@echo "Compilando todos los módulos en modo DEBUG..."
	$(MAKE) -C kernel_memory debug
	$(MAKE) -C kernel_scheduler debug
	$(MAKE) -C swap debug
	$(MAKE) -C memory_stick debug
	$(MAKE) -C cpu debug
	$(MAKE) -C io debug

# Regla explícita para compilar en modo RELEASE (para la entrega final)
release:
	@echo "Compilando todos los módulos en modo RELEASE..."
	$(MAKE) -C kernel_memory release
	$(MAKE) -C kernel_scheduler release
	$(MAKE) -C swap release
	$(MAKE) -C memory_stick release
	$(MAKE) -C cpu release
	$(MAKE) -C io release

# Regla para limpiar SOLO los archivos de log de todo el proyecto
fueraLogs:
	@echo "Limpiando solo los archivos de log..."
	-rm -f *.log */*.log
	@echo "¡Logs eliminados!"

# Regla para probar todos los modulos
test:
	@echo "Corriendo tests de todos los modulos..."
	$(MAKE) -C kernel_memory test
	$(MAKE) -C kernel_scheduler test
	$(MAKE) -C swap test
	$(MAKE) -C memory_stick test
	$(MAKE) -C cpu test
	$(MAKE) -C io test