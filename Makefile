.PHONY: all run stop clean fueraLogs debug release test

# Argumentos definidos para la prueba
PATH_PSEUDOCODIGO = PMP.prc

# Aca abajo se ponen los argumentos que se quieran usar como accesos rapidos
#PLANI_PRE_0.prc PLANI_MEM.prc MEMORIA_PRE_0.prc PCP.prc PHP.prc PMP.prc

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

# Prueba Base
# CONFIG_SCHEDULER = prueba_base.config
# CONFIG_MEMORY = prueba_base.config
# CONFIG_STICK = prueba_base.config

# Prueba PCP
# CONFIG_SCHEDULER = prueba_pcp.config
# CONFIG_MEMORY = prueba_pcp.config
# CONFIG_STICK = prueba_pcp.config

# Prueba Memoria
# CONFIG_SCHEDULER = prueba_mem.config
# CONFIG_MEMORY = prueba_mem.config
# CONFIG_STICK = prueba_mem_1.config # Y sus variantes 2, 3 y 4 (y ajustar TAM_STICK)

# Prueba PMP
# CONFIG_SCHEDULER = prueba_pmp.config
# CONFIG_MEMORY = prueba_pmp.config
# CONFIG_STICK = prueba_pmp_1.config # Y sus variantes 2, 3 y 4 (y ajustar TAM_STICK)

# Prueba PHP (Herencia de Prioridades)
# CONFIG_SCHEDULER = prueba_php.config
# CONFIG_MEMORY = prueba_php.config
# CONFIG_STICK = prueba_php_1.config # Y su variante 2 (y ajustar TAM_STICK)

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

# ---------------------------------------------------------
# REGLAS DE EJECUCIÓN AUTOMATIZADAS POR PRUEBA FINAL
# ---------------------------------------------------------

run-base:
	@echo "Levantando Prueba Base..."
	$(TERMINAL) "cd kernel_memory && ./bin/kernel_memory ./configs/prueba_base.config" &
	sleep 2
	$(TERMINAL) "cd kernel_scheduler && ./bin/kernel_scheduler ./configs/prueba_base.config PLANI_PRE_0.prc" &
	sleep 2
	$(TERMINAL) "cd swap && ./bin/swap ./configs/swap.config" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_base.config 256" &
	sleep 1
	$(TERMINAL) "cd cpu && ./bin/cpu ./configs/cpu.config CPU_1" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDIN" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDOUT" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config SLEEP" &

run-pcp:
	@echo "Levantando Prueba PCP..."
	$(TERMINAL) "cd kernel_memory && ./bin/kernel_memory ./configs/prueba_pcp.config" &
	sleep 2
	$(TERMINAL) "cd kernel_scheduler && ./bin/kernel_scheduler ./configs/prueba_pcp.config PCP.prc" &
	sleep 2
	$(TERMINAL) "cd swap && ./bin/swap ./configs/swap.config" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_pcp.config 256" &
	sleep 1
	$(TERMINAL) "cd cpu && ./bin/cpu ./configs/cpu.config CPU_1" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDIN" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDOUT" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config SLEEP" &

run-mem:
	@echo "Levantando Prueba Memoria..."
	$(TERMINAL) "cd kernel_memory && ./bin/kernel_memory ./configs/prueba_mem.config" &
	sleep 2
	$(TERMINAL) "cd kernel_scheduler && ./bin/kernel_scheduler ./configs/prueba_mem.config PLANI_MEM.prc" &
	sleep 2
	$(TERMINAL) "cd swap && ./bin/swap ./configs/swap.config" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_mem_1.config 16" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_mem_2.config 32" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_mem_3.config 64" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_mem_4.config 128" &
	sleep 1
	$(TERMINAL) "cd cpu && ./bin/cpu ./configs/cpu.config CPU_1" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDIN" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDOUT" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config SLEEP" &

run-pmp:
	@echo "Levantando Prueba PMP..."
	$(TERMINAL) "cd kernel_memory && ./bin/kernel_memory ./configs/prueba_pmp.config" &
	sleep 2
	$(TERMINAL) "cd kernel_scheduler && ./bin/kernel_scheduler ./configs/prueba_pmp.config PMP.prc" &
	sleep 2
	$(TERMINAL) "cd swap && ./bin/swap ./configs/swap.config" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_pmp_1.config 16" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_pmp_2.config 16" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_pmp_3.config 32" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_pmp_4.config 64" &
	sleep 1
	$(TERMINAL) "cd cpu && ./bin/cpu ./configs/cpu.config CPU_1" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDIN" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDOUT" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config SLEEP" &

run-php:
	@echo "Levantando Prueba PHP..."
	$(TERMINAL) "cd kernel_memory && ./bin/kernel_memory ./configs/prueba_php.config" &
	sleep 2
	$(TERMINAL) "cd kernel_scheduler && ./bin/kernel_scheduler ./configs/prueba_php.config PHP.prc" &
	sleep 2
	$(TERMINAL) "cd swap && ./bin/swap ./configs/swap.config" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_php_1.config 16" &
	$(TERMINAL) "cd memory_stick && ./bin/memory_stick ./configs/prueba_php_2.config 16" &
	sleep 1
	$(TERMINAL) "cd cpu && ./bin/cpu ./configs/cpu.config CPU_1" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDIN" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config STDOUT" &
	$(TERMINAL) "cd io && ./bin/io ./configs/io.config SLEEP" &