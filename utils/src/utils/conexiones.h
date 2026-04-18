#ifndef CONEXIONES_H
#define CONEXIONES_H

int iniciar_servidor(char* puerto);
int esperar_cliente(int server_fd);
int crear_conexion(char* ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket);
char* recibir_mensaje(int socket);

#endif