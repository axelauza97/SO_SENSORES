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
#include <math.h>
#include "csapp.h"

#define SHMSZ     4

typedef struct {
    int id,tipo,comm,th,activo,buffer,*valorAct;
    float intervalo;
    pid_t pid;
    pthread_t tid;
} Sensor;

typedef struct nodo_sensor {
	Sensor s;
	struct nodo_sensor* siguiente;
} node;

typedef struct {
    node *cabecera;
} Lista;

void addNode(Lista *l,Sensor *s);
void recorrer_lista(node *cabecera);

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
    pthread_create(&tid1,&attr,varianza,competitivo);
    //pthread_create(&tid2,&attr,suma_ponderada,cooperativo);
    /*
    char name[5];
    int **buffers = malloc(cntCoop * sizeof(int *) -1);
    key_t key;
    int shmid;
    for(int x=0;x<cntCoop;x++){
        sprintf(name, "%d", cooperativo[x].id);
        strcat(name,"key");
        key = ftok(name,65); 

        shmid = shmget(s.id+s.comm,sizeof(int)*cooperativo[x].buffer,0666|IPC_CREAT); 

        buffers[x] = (int*) shmat(shmid,NULL,0);
        
    }
    
    int *valoresBuffer;
    while(1){
        usleep(deltaT);
        for(int x=0;x<cntCoop;x++){
            valoresBuffer=buffers[x];
            if(cooperativo[x].activo){
                for(int i=0;i<=cooperativo[x].buffer;i++){
                    printf("\n VALOR i %d = %d  y th %d\n",i,valoresBuffer[i], cooperativo[x].id);
                }      
            }
        }
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
    // shmget returns an identifier in shmid 
    int shmid = shmget(s.id+s.comm, sizeof(int)*s.buffer ,0666|IPC_CREAT); 
    
    // shmat to attach to shared memory 
    int *valores = (int*) shmat(shmid,NULL,0);
    while(1){
        /* ms to mr  se debe de multiplicar por 1000*/
        usleep(s.intervalo);
        valores[x++] = *s.valorAct;
        //printf("DATO ACTUAL del sensor %d  %d  es %d, activo: %d\n",s.id,s.comm,*s.valorAct,s.activo);
        if(*s.valorAct==-1)
            s.activo=0;
        else{
            s.activo=1;
        }
        if(s.buffer+1==x){
            x=0;
        }
    }
}

void *varianza(void *param){
    Sensor *competitivo = ((Sensor *)param);
    Lista tipo1,tipo2,tipo3,tipo4;
    tipo1.cabecera=NULL;    
    tipo2.cabecera=NULL;
    tipo3.cabecera=NULL;
    tipo4.cabecera=NULL;
    
    Sensor s;

    for(int x=0;x<cntComp;x++){
        s=competitivo[x];
        switch (s.tipo){
            case 1:
                addNode(&tipo1,&s);
                printf("entra");
                break;
            case 2:
                addNode(&tipo2,&s);
                break;
            case 3:
                addNode(&tipo3,&s);
                break;
            case 4:
                addNode(&tipo4,&s);
                break;
        }
    }
    node *ptr;
    int xmedia=0;
    float varianza=0;
    int shmid, *buffer;
    while(1){
        usleep(deltaT);
        
        ptr = tipo1.cabecera;
        
        while(ptr != NULL) {
            shmid = shmget(ptr->s.id+ptr->s.comm,sizeof(int)*ptr->s.buffer,0666|IPC_CREAT); 
            buffer = (int*) shmat(shmid,NULL,0);
            for(int i=0;i<=ptr->s.buffer;i++){
                printf("Valor %d",buffer[i]);
                xmedia=xmedia+buffer[i];
            } 
            for(int i=0;i<=ptr->s.buffer;i++){
                varianza=(buffer[i]-xmedia)*(buffer[i]-xmedia);
            } 
            varianza=varianza/(ptr->s.buffer-1);
            printf("Varianza %f\n",varianza);
            varianza=0;
            xmedia=0;
            ptr = ptr->siguiente;
        }
        
    }   

    
	ptr = tipo2.cabecera;

	while(ptr != NULL) {
		printf("%d\n",ptr->s.id);
		ptr = ptr->siguiente;
	}
    
	ptr = tipo3.cabecera;

	while(ptr != NULL) {
		printf("%d\n",ptr->s.id);
		ptr = ptr->siguiente;
	}
    
	ptr = tipo4.cabecera;

	while(ptr != NULL) {
		printf("%d\n",ptr->s.id);
		ptr = ptr->siguiente;
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
    Sensor *cooperativo = ((Sensor *)param);
    
    int **buffers = malloc(cntCoop * sizeof(int *) -1);
    int shmid;
    for(int x=0;x<cntCoop;x++){
        shmid = shmget(cooperativo[x].id+cooperativo[x].comm,sizeof(int)*cooperativo[x].buffer,0666|IPC_CREAT); 

        buffers[x] = (int*) shmat(shmid,NULL,0);
        
    }

    int sumatoria=0;
    int nActivos=0;
    int *valoresBuffer;
    while(1){
        usleep(deltaT);
        sumatoria=0;
        nActivos=0;
        for(int x=0;x<cntCoop;x++){
            valoresBuffer=buffers[x];
            if(cooperativo[x].activo){
                nActivos++;
                for(int i=0;i<=cooperativo[x].buffer;i++){
                    printf("\n VALOR i %d = %d  y th %d\n",i,valoresBuffer[i], cooperativo[x].id);
                    if(valoresBuffer[i]>cooperativo[x].th){
                        sumatoria=sumatoria+valoresBuffer[i];
                    }
                }      
            }
        }
        SCoop=sumatoria/nActivos;
        printf("Suma ponderada %f",SCoop);   
    }
    pthread_exit(0);
}
void recorrer_lista(node *cabecera)
{
	node *ptr;
	ptr = cabecera;

	while(ptr != NULL) {
		printf("%d\n",ptr->s.id);
		ptr = ptr->siguiente;
	}
}
void addNode(Lista *l,Sensor *s){
    node *enlace= (node *) malloc(sizeof(node));
    enlace->s= *s;
    enlace->siguiente = l->cabecera;
    l->cabecera = enlace;
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
