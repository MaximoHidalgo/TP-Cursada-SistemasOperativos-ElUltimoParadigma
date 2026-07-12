# Libraries
LIBS=utils commons pthread readline m

# Custom libraries' paths
SHARED_LIBPATHS=
STATIC_LIBPATHS=../utils

# Compiler flags
CDEBUG=-g -Wall -DDEBUG -fdiagnostics-color=always -std=gnu99 -D_POSIX_C_SOURCE=200112L
CRELEASE=-O3 -Wall -DNDEBUG -std=gnu99 -D_DEFAULT_SOURCE #version modificada
#CRELEASE=-O3 -Wall -DNDEBUG -std=gnu99 -D_POSIX_C_SOURCE=200112L | version sin modificar

# Source files (*.c) to be excluded from tests compilation
TEST_EXCLUDE=src/main.c
