#include <readline/readline.h>
#include <stdlib.h>
#include <string.h>

/*
 * Wrapper de readline para poder mockear en tests
 */
char* io_readline(const char* prompt)
{
    return readline(prompt);
}