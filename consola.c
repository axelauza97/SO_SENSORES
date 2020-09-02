#include "csapp.h"
#include<sys/types.h>
#include<sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

enum estados{EXIT=0,DESCONECTADO,CONECTADO,REC_RESP} estado; 
int entrada();
int main(){

    int clientfd;
	int n;
	char *port;
	char *host, buf[MAXLINE];
	char ok[3];
	rio_t rio;

    host = "127.0.0.1";
	port = "8080";

	estado=DESCONECTADO;

    while(estado){
		switch(estado){
		
		case(DESCONECTADO):
			clientfd = Open_clientfd(host, port);
			Rio_readinitb(&rio, clientfd);
			estado=CONECTADO;
			break;

		case(CONECTADO):
            int opcion = entrada();
            if (opcion==4){
                estado=EXIT;
                continue;
            }
			Rio_writen(clientfd, opcion, 1);
			Rio_readn(clientfd,ok,2);
			ok[3]='\0';	
			if(strcmp(ok,"ok")==0) {
			    estado=REC_RESP;
			}else{
			    printf("Error de conexion\n");
			}
			break;

		case(REC_RESP):			
			while( (n=Rio_readn(clientfd,buf,MAXLINE)) >0)	{	
                printf("%s",buf);
	        }
            estado=CONECTADO;
            break;
	}

}
int entrada(){
    int opcion = 0;
    printf("Opciones:\n\t1.- Datos que llegan en cada sensor\n");
    printf("\t2.- Reglas que se cumplen y estado de alarma\n");
    printf("\t3.- Ver informacion de diagnostico de sensores (activo/inactivo, pid, fecha de ultimo dato recibido)\n");
    printf("\t4.- Salir\n");
    while(1){
        switch (opcion){
            case 1:
                printf("Datos que llegan en cada sensor\n");
                break;
            case 2:
                printf("Reglas que se cumplen y estado de alarma\n");
                break;
            case 3:
                printf("Ver informacion de diagnostico de sensores (activo/inactivo, pid, fecha de ultimo dato recibido)\n");
                break;
            default:
                printf("Opciones:\n\t1.- Datos que llegan en cada sensor\n");
                printf("\t2.- Reglas que se cumplen y estado de alarma\n");
                printf("\t3.- Ver informacion de diagnostico de sensores (activo/inactivo, pid, fecha de ultimo dato recibido)\n");
                printf("\t4.- Salir\n");
                continue;
                break;
            }
    }
    return opcion;
}