#include "csapp.h"
#include <string.h>
#include<sys/types.h>
#include<sys/stat.h>

enum estados{EXIT=0,DESCONECTADO,CONECTADO,REC_RESP} estado; 
void entrada(int *opcion);
int main(){

    int clientfd;
	int n;
	char *port;
	char *host, buf[MAXLINE];
	char ok[3];
	rio_t rio;
    int *opcion;
    char opcionStr[2];

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
            opcion=malloc(sizeof(int)*1);
            entrada(opcion);
            if (*opcion==4){
                estado=EXIT;
                continue;
            }
            sprintf(opcionStr,"%d",*opcion);
			Rio_writen(clientfd, opcionStr, 2);
			Rio_readn(clientfd,ok,2);
			ok[3]='\0';	
			if(strcmp(ok,"ok")==0) {
			    estado=REC_RESP;
			}else{
			    printf("Error de conexion\n");
			}
			break;

		case(REC_RESP):
            while( (n=Rio_readn(clientfd,buf,MAXLINE)) >0)
            {   if(buf[0]!='o')     
                printf("--> %s\n",buf);
                Rio_writen(clientfd, "ok", 2*sizeof(char));
            }
            estado=DESCONECTADO;
            break;
        case(EXIT):
            exit(0);
            break;
	    }

    }
    exit(0);
}


void entrada(int *opcion){
    int flag=1;
    while(flag){
        printf("Opciones:\n\t1.- Datos que llegan en cada sensor\n");
        printf("\t2.- Reglas que se cumplen y estado de alarma\n");
        printf("\t3.- Ver informacion de diagnostico de sensores (activo/inactivo, pid, fecha de ultimo dato recibido)\n");
        printf("\t4.- Salir\n:\t");
        scanf("%d",opcion);
        switch (*opcion){
        case 1:
            printf("\nDatos que llegan en cada sensor\n");
            flag=0;
            break;
        case 2:
            printf("\nReglas que se cumplen y estado de alarma\n");
            flag=0;
            break;
        case 3:
            printf("\nVer informacion de diagnostico de sensores (activo/inactivo, pid, fecha de ultimo dato recibido)\n");
            flag=0;
            break;      
        case 4:
            printf("\nSalir");
            flag=0;
            break;
        }
    }
}