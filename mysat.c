#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "csapp.h"

#define SHMSZ     4

typedef struct {
    int id,tipo,comm,th,activo,buffer,*valorAct,*valores;
    float intervalo;
    pid_t pid;
    pthread_t tid;
} Sensor;

typedef Sensor Element;

typedef struct {
    Element *content;
    int current_size;
    int top;
} Stack;

void initialize(Stack *stack);
void destroy(Stack *stack);
int is_empty(Stack *stack);
void push(Stack *stack, Element data);
Element pop(Stack *stack);

void readFile(char **lineas,char *name);
void *sensor(void *param); /* the thread */
void *varianza(void *param); /* the thread */
void *suma_ponderada(void *param); /* the thread */
void *decision(void *param); /* the thread */
void sig_handlerINT(int signo);

enum estados{EXIT=0,DESCONECTADO,ESCUCHANDO,CONECTADO,RESP_OK,ENVIA_DATOS} estado; 

int cntComp=0;
int cntCoop=0;
int deltaT=0;
float SCoop=0;
int alarma = 0;

Sensor *competitivo;
Sensor *cooperativo;
pthread_t tid1; /* the thread identifier */
pthread_t tid2; /* the thread identifier */
pthread_attr_t attr; /* set of attributes for the thread */

int main(int argc, char *argv[])
{
    if (argc != 4){ 
        printf("Invalid arguments only eg.\"filename nameFile delaT nSensores!!\""); 
        return 0; 
    } 
    int n=atoi(argv[3]);
    deltaT=atoi(argv[2]);
    competitivo = malloc(n * sizeof(Sensor));
    cooperativo = malloc(n * sizeof(Sensor));

    char **lineas;
    lineas = malloc(n * sizeof(char*));    

    readFile(lineas,argv[1]);
    
    char *tmp=malloc(255 * sizeof(char));
    Sensor s;
    for(int i=0;i<n;i++){
        strcpy(tmp,lineas[i]);
        printf("\nlinea %s",lineas[i]);
        char *p = strtok (tmp, ",");
        
        int y=0;
        while (p != NULL){
            switch (y){
                case 0:
                    s.id=atoi(p);
                    break;
                case 1:
                    s.tipo=atoi(p);
                      
                    break;
                case 2:
                    s.comm=atoi(p);
                    break;
                case 3:
                    s.intervalo=atoi(p);
                    break;
                case 4:
                    s.th=atoi(p);
                    
                    int shmid;
                    if ((shmid = shmget(s.comm, SHMSZ,  0666)) < 0) {
                        perror("shmget");
                        exit(1);
                    }
                    if ((s.valorAct = shmat(shmid, NULL, 0)) == (int *) -1) {
                        perror("shmat");
                        exit(1);
                    }
                    int buffer = (int)(deltaT / s.intervalo) - 1;
                    s.buffer = buffer;
                    s.valores = malloc(buffer * sizeof(int));
                    if(s.tipo < 5){
                        competitivo[cntComp++]=s;
                    }  
                    else{
                        cooperativo[cntCoop++]=s;
                    }
                    pthread_attr_init(&attr);
                    pthread_create(&s.tid,&attr,sensor,(void *) &s);
                    break;
            }
            p = strtok (NULL, ",");
            y++;
        }     
        
        free(p);   
        //printf("-->Sensor id: %d , tipo: %d, intervalo: %f %d\n",s.id,s.tipo,s.intervalo, s.comm);
    }
    printf("Creacion de hilos suma ponderada, varianza y decision\n");
    /* get the default attributes */
    pthread_attr_init(&attr);
    //pthread_create(&tid1,&attr,varianza,NULL);
    pthread_create(&tid2,&attr,suma_ponderada,NULL);

    /*for(int x=0;x<cntComp;x++){
        Sensor s=competitivo[x];
        printf("-->COMPE Sensor id: %d , tipo: %d, intervalo: %f %d\n",s.id,s.tipo,s.intervalo, s.comm);
    }
    for(int x=0;x<cntCoop;x++){
        Sensor s=cooperativo[x];
        printf("-->COOPE Sensor id: %d , tipo: %d, intervalo: %f %d\n",s.id,s.tipo,s.intervalo, s.comm);
    }*/

    signal(SIGINT, sig_handlerINT);
    while(1){
        usleep(deltaT);
        if(SCoop>0.7){
            alarma = 1;
        }
        if(alarma)
            printf("Alarma encendida");
    }    
}
void readFile(char **lineas,char *name){
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    fp = fopen(name, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
    getline(&line, &len, fp);

    int i =0;
    while ( getline(&line, &len, fp) != -1) {
        lineas[i] = malloc((len+1) * sizeof(char));
        strcpy(lineas[i++],line);
    }
    fclose(fp);
    free(line);
};

void sig_handlerINT(int signo){
  if (signo == SIGINT){
    printf("\nFinalizando programa \n");
    printf("Terminando Hilos de suma y varianza\n");
    pthread_cancel(tid1);
    pthread_cancel(tid2);
    printf("Terminando lectura de sensores competitivos\n");
    for(int i=0;i<cntComp;i++){        
        pid_t w;  
        int status;
        w = waitpid(competitivo[i].pid, &status, WUNTRACED);
        fprintf(stderr,"Child competitivo finished \n");
        shmdt(competitivo[i].valorAct);
    }
    printf("Terminando sensores cooperativos\n");
    for(int i=0;i<cntCoop;i++){        
        pid_t w;  
        int status;
        w = waitpid(cooperativo[i].pid, &status, WUNTRACED);
        fprintf(stderr,"Child cooperativo finished \n");
        shmdt(cooperativo[i].valorAct);
    }
  }
  exit(1);
}

void *sensor(void *param){
    Sensor s = *((Sensor *)param);
    int x = 0;
    while(1){
        /* ms to mr  se debe de multiplicar por 1000*/
        usleep(s.intervalo);
        s.valores[x++] = *s.valorAct;
        //printf("DATO ACTUAL del sensor %d es %d, activo: %d\n",s.id,*s.valorAct,s.activo);
        if(*s.valorAct==-1)
            s.activo=0;
        else
        {
            s.activo=1;
        }
        if(s.buffer+1==x){
            x=0;
        }
    }
}

void *varianza(void *param){
    Stack *tipo1,*tipo2,*tipo3,*tipo4;
    initialize(tipo1);
    initialize(tipo2);
    initialize(tipo3);
    initialize(tipo4);
    
    for(int x=0;x<cntComp;x++){
        Sensor s=competitivo[x];
        printf("Sendor 5 %d",s.comm);
        switch (s.tipo){
                case 1:
                    push(tipo1,s);
                    break;
                case 2:
                    push(tipo2,s);
                    break;
                case 3:
                    push(tipo3,s);
                    break;
                case 4:
                    push(tipo4,s);
                    break;
        }
    }
    
    
    /*while(1){
        usleep(deltaT);
        for(int x=0;x<cntComp;x++){
            Sensor s=competitivo[x];
            if(s.activo){
                for(int i=0;i<=s.buffer;i++){
                    printf("\n VALOR i %d = %d  y th %d\n",i,s.valores[i], s.th);
                    if(s.valores[i]>s.th){
                    }
                }      
            }
        }
    
        //printf("Thread Varianza Sensor id: %d , tipo: %d, intervalo: %f\n",competitivo[x].id,competitivo[x].tipo,competitivo[x].intervalo);
        
        //varianza = sumatoria( (xi-xmedia)*(x-xmedia) ) / (n-1)
    }   */
    pthread_exit(0);
}

void *suma_ponderada(void *param){
    
    while(1){
        usleep(deltaT);
        int sumatoria=0;
        int nActivos=0;
        for(int x=0;x<cntCoop;x++){
            Sensor s=cooperativo[x];
            if(s.activo){
                nActivos++;
                for(int i=0;i<=s.buffer;i++){
                    printf("\n VALOR i %d = %d  y th %d\n",i,s.valores[i], s.th);
                    if(s.valores[i]>s.th){
                        sumatoria=sumatoria+s.valores[i];
                    }
                }      
            }
        }
        SCoop=sumatoria/nActivos;
        printf("Suma ponderada %f",SCoop);   
    }
    pthread_exit(0);
}


void initialize(Stack *stack)
{
    Element *content;

    content = (Element *) calloc(2, sizeof(Element));

    if (content == NULL) 
    {
        fprintf(stderr, "Not enough memory to initialize stack\n");
        exit(1);
    }

    stack->content = content;
    stack->current_size = 2;
    stack->top = -1;
}
void destroy(Stack *stack)
{
    free(stack->content);

    stack->content = NULL;
    stack->current_size = 0;
    stack->top = -1;
}
int is_empty(Stack *stack)
{
    if (stack->top < 0)
        return 1;
    else
        return 0;
}
void push(Stack *stack, Element data)
{    
    stack->top = stack->top + 1;
    stack->content[stack->top] = data;
}
Element pop(Stack *stack)
{
    if (is_empty(stack) == 0)
    {
        Element data = stack->content[stack->top];
        stack->top = stack->top - 1;
        return data;
    }
    
    fprintf(stderr, "Stack is empty\n");
    exit(1);
}

/*
    int listenfd, connfd,n;
	char buf[MAXLINE];
	unsigned int clientlen;
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char *haddrp, *port;
    char opcion[2];
	long *tamanio_arch;
	rio_t rio;

	port = "8080";
	estado=DESCONECTADO;
	
	while (estado) {
		switch(estado){

		case(DESCONECTADO):
			listenfd = Open_listenfd(port);
			estado=ESCUCHANDO;
			break;

		case(ESCUCHANDO):
			clientlen = sizeof(clientaddr);
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
			// Determine the domain name and IP address of the client 
			hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
					sizeof(clientaddr.sin_addr.s_addr), AF_INET);
			haddrp = inet_ntoa(clientaddr.sin_addr);
			printf("server connected to %s (%s)\n", hp->h_name, haddrp);
			estado=CONECTADO;
			break;

		case(CONECTADO):
			Rio_readinitb(&rio, connfd);
			read(connfd,opcion,2);			
			estado=RESP_OK;
			break;
			
		case(RESP_OK):
            Rio_writen(connfd, "ok", 2*sizeof(char));
            estado=ENVIA_DATOS;
			break;

		case(ENVIA_DATOS):
			//ESCRIBIR
            while( (n=Rio_readn(fd1,buf,MAXLINE)) >0){
			    Rio_writen(connfd,buf,n);
            }
			Close(connfd);
			estado=ESCUCHANDO;
			//estado=EXIT;
			break;
	
		}	
	}
*/