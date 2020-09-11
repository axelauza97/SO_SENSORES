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
    pthread_t tid;
    char timeinfo[30];
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


float getVarianza(node *cabecera);

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
float VarianzaComp=0;
int alarma = 0;
int Th=0;


Lista lcompetitivo,lcooperativo;

pthread_t tid1; /* the thread identifier */
pthread_t tid2; /* the thread identifier */
pthread_t tid3; /* the thread identifier */
pthread_attr_t attr; /* set of attributes for the thread */

int main(int argc, char *argv[])
{
    
    if (argc != 5){ 
        printf("Invalid arguments only eg.\"filename nameFile delaT nSensores Th!!\""); 
        return 0; 
    } 
    int n=atoi(argv[3]);
    Th = atoi(argv[4]);
    deltaT=atoi(argv[2]);

    char **lineas;
    lineas = malloc(n * sizeof(char*));    

    readFile(lineas,argv[1]);
    
    char *tmp=malloc(255 * sizeof(char));
    Sensor s;
    pthread_attr_init(&attr);
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
                        cntComp++;
                        addNode(&lcompetitivo,&s);
                        
                    }  
                    else{
                        cntCoop++;
                        addNode(&lcooperativo,&s);
                    }
                    pthread_create(&s.tid,&attr,sensor,(void *) &s);
                    sleep(1);//Tiempo para llenar buffer necesario de Varianza
                    break;
            }
            p = strtok (NULL, ",");
            y++;
        }     
        free(p);   
    }
    printf("Creacion de hilos suma ponderada, varianza y decision\n");
    /* get the default attributes */
    pthread_create(&tid1,&attr,varianza,&lcompetitivo);
    pthread_create(&tid2,&attr,suma_ponderada,&lcooperativo);
    pthread_create(&tid3,&attr,decision,NULL);
    signal(SIGINT, sig_handlerINT);
        
    int listenfd, connfd;
	char buf[255];
	unsigned int clientlen;
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char *haddrp, *port;
    char opcion[2];
	rio_t rio;
    node *ptr;

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
            if(strcmp(opcion,"1")==0){
                //Datos que llegan en cada sensor
                ptr = lcompetitivo.cabecera;
                while(ptr != NULL) {
                    s=ptr->s;
                    snprintf(buf, sizeof(buf), "Sensor competitivo id %d y valor %d", s.id,*s.valorAct);
                    Rio_writen(connfd,buf,MAXLINE);
                    Rio_readn(connfd,buf,2);
                    ptr = ptr->siguiente;
                }
                ptr = lcooperativo.cabecera;
                while(ptr != NULL) {
                    s=ptr->s;
                    snprintf(buf, sizeof(buf), "Sensor cooperativo id %d y valor %d", s.id,*s.valorAct);
                    Rio_writen(connfd,buf,MAXLINE);
                    Rio_readn(connfd,buf,2);
                    ptr = ptr->siguiente;
                }
                Rio_writen(connfd, "ok", 2*sizeof(char));
            }
            if(strcmp(opcion,"2")==0){
                //Reglas que se cumplen y estado de alarma
                if(alarma)
                    snprintf(buf, sizeof(buf), "Alarma encendida");
                else{
                    snprintf(buf, sizeof(buf), "Alarma apagada");
                }
                Rio_writen(connfd,buf,MAXLINE);
                Rio_readn(connfd,buf,2);
                if(SCoop>0.7){
                    snprintf(buf, sizeof(buf), "Se cumple Suma Ponderada %f",SCoop);
                }
                else{
                    snprintf(buf, sizeof(buf), "No se cumple Suma Ponderada %f",SCoop);
                }
                Rio_writen(connfd,buf,MAXLINE);
                Rio_readn(connfd,buf,2);
                if(VarianzaComp>Th){
                    snprintf(buf, sizeof(buf), "Se cumple Varianza %f > %d Th",VarianzaComp,Th);
                }
                else{
                    snprintf(buf, sizeof(buf), "No cumple Varianza %f < %d Th",VarianzaComp,Th);
                }
                Rio_writen(connfd,buf,MAXLINE);
                Rio_readn(connfd,buf,2);
                Rio_writen(connfd, "ok", 2*sizeof(char));
            }
            if(strcmp(opcion,"3")==0){
                //Ver informacion de diagnostico de sensores (activo/inactivo, pid, fecha de ultimo dato recibido)
                ptr = lcompetitivo.cabecera;
                while(ptr != NULL) {
                    s=ptr->s;
                    if(*s.valorAct==-1)
                        snprintf(buf, sizeof(buf), "Sensor competitivo id %d, activo False, tid %ld, fecha %s ", s.id,s.tid,s.timeinfo);
                    else
                    {
                        snprintf(buf, sizeof(buf), "Sensor competitivo id %d, activo True, tid %ld, fecha %s ", s.id,s.tid,s.timeinfo);
                    }
                        
                    Rio_writen(connfd,buf,MAXLINE);
                    Rio_readn(connfd,buf,2);
                    ptr = ptr->siguiente;
                }
                ptr = lcooperativo.cabecera;
                while(ptr != NULL) {
                    s=ptr->s;
                    if(*s.valorAct==-1)
                        snprintf(buf, sizeof(buf), "Sensor Cooperativo id %d, activo False, tid %ld, fecha %s", s.id,s.tid,s.timeinfo);
                    else
                    {
                        snprintf(buf, sizeof(buf), "Sensor Cooperativo id %d, activo True, tid %ld, fecha %s", s.id,s.tid,s.timeinfo);
                    }
                    Rio_writen(connfd,buf,MAXLINE);
                    Rio_readn(connfd,buf,2);
                    ptr = ptr->siguiente;
                }
                Rio_writen(connfd, "ok", 2*sizeof(char));
            }
			Close(connfd);
			estado=ESCUCHANDO;
			//estado=EXIT;
			break;
        case(EXIT):
            exit(0);
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
    
    int shmid;
    Sensor s;
    node *ptr;
    ptr = lcompetitivo.cabecera;
    while(ptr != NULL) {
        s=ptr->s;                
        shmdt(s.valorAct);
        shmid = shmget(s.id+s.comm,sizeof(int)*s.buffer,0666);
        shmdt(shmat(shmid,NULL,0)); 
        pthread_cancel(s.tid);
        ptr = ptr->siguiente;
    }
    printf("Terminando sensores cooperativos\n");
    ptr = lcooperativo.cabecera;
    while(ptr != NULL) {  
        s=ptr->s;       
        shmdt(s.valorAct);
        shmid = shmget(s.id+s.comm,sizeof(int)*s.buffer,0666);
        shmdt(shmat(shmid,NULL,0));
        pthread_cancel(s.tid);
        ptr = ptr->siguiente;
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

    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    while(1){
        /* ms to mr  se debe de multiplicar por 1000*/
        usleep(s.intervalo);
        valores[x++] = *s.valorAct;
        
        snprintf(s.timeinfo, sizeof(s.timeinfo), "%s", asctime (timeinfo));
        //printf("FECHA %s",s.timeinfo);
        //printf("DATO ACTUAL del sensor %d  %d  es %d, activo: %d\n",s.id,s.comm,*s.valorAct,s.activo);
        if(*s.valorAct==-1){
            s.activo=0;
        }
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
        if(SCoop>0.7 || VarianzaComp>Th){
            alarma=1;
        }
        else{
            alarma=0;
        }
        //if(alarma)
          //  printf("Alarma encendida");
    }
}

void *varianza(void *param){
    Lista *competitivo = ((Lista *)param);
    Lista tipo1,tipo2,tipo3,tipo4;
    tipo1.cabecera=NULL;    
    tipo2.cabecera=NULL;
    tipo3.cabecera=NULL;
    tipo4.cabecera=NULL;
    
    Sensor s;
    node *ptr;
    ptr = competitivo->cabecera;
    while(ptr != NULL) {
        s=ptr->s;
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
        ptr = ptr->siguiente;
    }
    float varianza=-1;
    float varianzaTmp=0;
    while(1){
        usleep(deltaT);
        if(tipo1.cabecera!=NULL){
            varianzaTmp=getVarianza(tipo1.cabecera);
            if(varianza==-1 || varianza>varianzaTmp)
                varianza=varianzaTmp;
        }
        
        if(tipo2.cabecera!=NULL){
            varianzaTmp=getVarianza(tipo2.cabecera);
            if(varianza==-1 || varianza>varianzaTmp)
                varianza=varianzaTmp;
        }
        
        if(tipo3.cabecera!=NULL){
            varianzaTmp=getVarianza(tipo3.cabecera);
            if(varianza==-1 || varianza>varianzaTmp)
                varianza=varianzaTmp;
        }
        
        if(tipo4.cabecera!=NULL){
            varianzaTmp=getVarianza(tipo4.cabecera);
            if(varianza==-1 || varianza>varianzaTmp)
                varianza=varianzaTmp;
        }
        
        VarianzaComp=varianza;
        varianza=-1;
    }   
    pthread_exit(0);
}

float getVarianza(node *cabecera){
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
    return varianza;
}

void *suma_ponderada(void *param){
    Lista *lcooperativo = ((Lista *)param);
    int sumatoria=0;
    int nActivos=0;
    node *ptr;
    Sensor s;
    ptr=lcooperativo->cabecera;
    while(1){
        usleep(deltaT);
        sumatoria=0;
        nActivos=0;
        while(ptr != NULL) {
            s=ptr->s;
            if(*s.valorAct!=-1){
                nActivos++;
                //printf("\n VALOR %d y th %d\n",*s.valorAct, s.id);
                if(*s.valorAct>s.th){
                    sumatoria=sumatoria+1;
                }      
            }
            ptr = ptr->siguiente;
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