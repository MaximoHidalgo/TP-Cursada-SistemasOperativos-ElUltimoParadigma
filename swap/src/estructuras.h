#ifndef ESTRUCTURAS_SWAP_H_
#define ESTRUCTURAS_SWAP_H_

#include <commons/log.h>
#include <commons/config.h>
#include <stdio.h>

typedef struct {
    char* ip_memory;
    char* puerto_memory;
    char* swap_file_path;
    int swap_file_size;
    int block_size;
    FILE* swap_file;
    t_log* logger;
    t_config* config;
    int socket_kernel_memory;
} t_swap_context;

#endif /* ESTRUCTURAS_SWAP_H_ */
