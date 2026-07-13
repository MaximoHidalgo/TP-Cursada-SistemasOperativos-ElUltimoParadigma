#include <stdio.h>

void test_ejecutar_sleep(void);
void test_ejecutar_stdout(void);
void test_ejecutar_stdin(void);
void test_procesar_peticion_io_sleep(void);
void test_procesar_peticion_io_stdout(void);

int main(void)
{
    printf("=================================================\n");
    printf(":O Iniciando Tests Unitarios de IO\n");
    printf("=================================================\n\n");

    test_ejecutar_sleep();
    test_ejecutar_stdout();
    test_ejecutar_stdin();
    test_procesar_peticion_io_sleep();
    test_procesar_peticion_io_stdout();

    printf("\n=================================================\n");
    printf(">:) Todos los tests se ejecutaron.\n");
    printf("=================================================\n");

    return 0;
}