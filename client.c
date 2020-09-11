#include "csapp.h"
#include <string.h>
#include<sys/types.h>
#include<sys/stat.h>
enum estados{EXIT=0,DESCONECTADO,CONECTADO,REC_RESP} estado; 
int main(int argc, char **argv)
{
	int clientfd;
	int fd1,n;
	char *port;
	char *host, buf[MAXLINE];
	char ok[3];
	rio_t rio;
	
	if (argc != 4) {
		fprintf(stderr, "usage: %s <host> <port> <archivo>\n", argv[3]);
		exit(0);
	}
	host = argv[1];
	port = argv[2];
	estado=DESCONECTADO;
	while(estado){
		switch(estado){
		
		case(DESCONECTADO):
			clientfd = Open_clientfd(host, port);
			Rio_readinitb(&rio, clientfd);
			
			estado=CONECTADO;
			break;
		case(CONECTADO):
			
			Rio_writen(clientfd, argv[3], strlen(argv[3]));
			Rio_readn(clientfd,ok,2);
			ok[3]='\0';	
			if(strcmp(ok,"ok")==0) {
			estado=REC_RESP;
			}else{
			printf("NO EXISTE %s ARCHIVO\n", argv[3]);
			estado=DESCONECTADO;
			}
			break;

		case(REC_RESP):			
			fd1=Open (argv[3], O_CREAT|O_WRONLY,0777);
			while( (n=Rio_readn(clientfd,buf,MAXLINE)) >0)	{		
				Rio_writen(fd1, buf, n);}
			Close(fd1);
			estado=EXIT;
			break;
		
	
	}
	}
	exit(0);
}
