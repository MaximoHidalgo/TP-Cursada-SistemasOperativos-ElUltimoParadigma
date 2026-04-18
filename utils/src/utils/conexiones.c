#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include "conexiones.h"
#include <sys/socket.h>

void enviar_mensaje(char* mensaje, int socket){
    int tamanio = strlen(mensaje) + 1;
    send(socket, &tamanio, sizeof(int), 0);
    send(socket, mensaje, tamanio, 0);
}

char* guardar_mensaje(int socket){
    int tamanio;
    recv(socket, &tamanio, sizeof(int), MSG_WAITALL);

    char* buffer = malloc(tamanio);
    recv(socket, buffer, tamanio, MSG_WAITALL);

    return buffer;
}


/*
int iniciar_servidor(char* puerto) {
    int socket_servidor;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, puerto, &hints, &servinfo);

    socket_servidor = socket(servinfo->ai_family,
                             servinfo->ai_socktype,
                             servinfo->ai_protocol);

    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    listen(socket_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);

    return socket_servidor;
}


int esperar_cliente(int server_fd) {
    return accept(server_fd, NULL, NULL);
}
*/


/*
int crear_conexion(char* ip, char* puerto) {
    struct addrinfo hints, *servinfo;
    int socket_cliente;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(ip, puerto, &hints, &servinfo);

    socket_cliente = socket(servinfo->ai_family,
                            servinfo->ai_socktype,
                            servinfo->ai_protocol);

    connect(socket_cliente, servinfo->ai_addr, servinfo->ai_addrlen);

    freeaddrinfo(servinfo);

    return socket_cliente;
}*/
