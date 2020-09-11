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


void getVarianza(node *cabecera);

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
pthread_t tid3; /* the thread identifier */
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
                    //sleep(1);
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
    //pthread_create(&tid1,&attr,varianza,competitivo);
    //pthread_create(&tid2,&attr,suma_ponderada,cooperativo);
    //pthread_create(&tid3,&attr,decision,NULL);
    signal(SIGINT, sig_handlerINT);
    /*int i = 0;
    int shmid,*valores;
    while (1)
    {
        usleep(deltaT);
    for(int x=0;x<cntComp;x++){
        s=competitivo[x];
        i=0;
        // shmget returns an identifier in shmid 
        shmid = shmget(s.id+s.comm, sizeof(int)*s.buffer ,0666|IPC_CREAT); 
        
        // shmat to attach to shared memory 
        valores = (int*) shmat(shmid,NULL,0);
        for(int y=0;y<=s.buffer;y++){
            printf("Valor y = %d b = %d  sen %d\n",y,valores[y],x);
        } 
    }
    }*/    
    int listenfd, connfd;
	char buf[255];
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
            printf("opcion %s",opcion);		
			estado=RESP_OK;
			break;
			
		case(RESP_OK):
            Rio_writen(connfd, "ok", 2*sizeof(char));
            estado=ENVIA_DATOS;
			break;

		case(ENVIA_DATOS):
			//ESCRIBIR
            if(strcmp(opcion,"1")==0){
                //Datos que llegan en cada sensor
                for(int x=0;x<cntComp;x++){
                    s=competitivo[x];
                    snprintf(buf, sizeof(buf), "Sensor competitivo id %d y valor %d", s.id,*s.valorAct);
                    Rio_writen(connfd,buf,MAXLINE);
                    printf("escribio");    
                    Rio_readn(connfd,buf,2);
                    printf("leyo");
                }
                for(int x=0;x<cntCoop;x++){
                    s=cooperativo[x];
                    snprintf(buf, sizeof(buf), "Sensor cooperativo id %d y valor %d", s.id,*s.valorAct);
                    Rio_writen(connfd,buf,MAXLINE);
                    printf("escribio");    
                    Rio_readn(connfd,buf,2);
                    printf("leyo");
                }
                Rio_writen(connfd, "ok", 2*sizeof(char));
                printf("escribio");
            }
            if(strcmp(opcion,"2")==0){
                //Reglas que se cumplen y estado de alarma
                
            }
            if(strcmp(opcion,"3")==0){
                printf("ESCRIBO 3");
            }
            /*while( (n=Rio_readn(fd1,buf,MAXLINE)) >0){
			    Rio_writen(connfd,buf,n);
            }*/
			Close(connfd);
			estado=ESCUCHANDO;
			//estado=EXIT;
			break;
	
		}	
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
    pthread_cancel(tid3);
    printf("Terminando lectura de sensores competitivos\n");
    pthread_t tid;
    int shmid;
    for(int i=0;i<cntComp;i++){        
        tid=competitivo[i].tid;
        shmdt(competitivo[i].valorAct);
        shmid = shmget(competitivo[i].id+competitivo[i].comm,sizeof(int)*competitivo[i].buffer,0666);
        shmdt(shmat(shmid,NULL,0)); 
    }
    printf("Terminando sensores cooperativos\n");
    for(int i=0;i<cntCoop;i++){    
        tid=cooperativo[i].tid;    
        shmdt(cooperativo[i].valorAct);
        shmid = shmget(cooperativo[i].id+cooperativo[i].comm,sizeof(int)*cooperativo[i].buffer,0666);
        shmdt(shmat(shmid,NULL,0));
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

void *decision(void *param){
    while(1){
        usleep(deltaT);
        if(SCoop>0.7){
            alarma = 1;
        }
        if(alarma)
            printf("Alarma encendida");
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
        getVarianza(tipo1.cabecera);
        getVarianza(tipo2.cabecera);
        getVarianza(tipo3.cabecera);
        getVarianza(tipo4.cabecera);
    }   
    pthread_exit(0);
}

void getVarianza(node *cabecera){
    node *ptr;
    float xmedia=0;
    float varianza=-1;
    float varianzaTmp=0;
    int shmid, *buffer;
    ptr = cabecera;
    while(ptr != NULL) {
        shmid = shmget(ptr->s.id+ptr->s.comm,sizeof(int)*ptr->s.buffer,0666); 
        buffer = (int*) shmat(shmid,NULL,0);
        //obtencion media
        for(int i=0;i<=ptr->s.buffer;i++){
            //printf("Valor i = %d b = %d\n",i,buffer[i]);
            xmedia=xmedia+buffer[i];
        } 
        xmedia=xmedia/(ptr->s.buffer);;
        //obtencion varianza
        for(int i=0;i<=ptr->s.buffer;i++){
            varianzaTmp=(buffer[i]-xmedia)*(buffer[i]-xmedia);
        } 
        varianzaTmp=varianzaTmp/(ptr->s.buffer-1);
        //printf("Varianza %f\n",varianzaTmp);
        if(varianza==-1 || varianza>varianzaTmp)
            varianza=varianzaTmp;
        varianzaTmp=0;
        xmedia=0;
        ptr = ptr->siguiente;
    }
}

void *suma_ponderada(void *param){
    Sensor *cooperativo = ((Sensor *)param);
    
    int **buffers = malloc(cntCoop * sizeof(int *) -1);
    int shmid;
    for(int x=0;x<cntCoop;x++){
        shmid = shmget(cooperativo[x].id+cooperativo[x].comm,sizeof(int)*cooperativo[x].buffer,0666); 
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
                //printf("\n VALOR %d y th %d\n",*cooperativo[x].valorAct, cooperativo[x].id);
                if(*cooperativo[x].valorAct>cooperativo[x].th){
                    sumatoria=sumatoria+1;
                }      
            }
        }
        if (nActivos==0){
            SCoop=0;
        }else
            SCoop=sumatoria/nActivos;
        //printf("Suma ponderada %f",SCoop);   
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