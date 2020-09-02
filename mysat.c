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

#define SHMSZ     4

struct Sensor
    {
        int id,tipo,comm,th,activo,buffer,*valorAct,*valores;
        float intervalo;
        pid_t pid;
        pthread_t tid;
    };
void readFile(char **lineas,char *name);
void *sensor(void *param); /* the thread */
void *varianza(void *param); /* the thread */
void *suma_ponderada(void *param); /* the thread */
void *decision(void *param); /* the thread */
void sig_handlerINT(int signo);


int cntComp=0;
int cntCoop=0;
int deltaT=0;
float SCoop=0;
int alarma = 0;

struct Sensor *competitivo;
struct Sensor *cooperativo;
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
    competitivo = malloc(n * sizeof(struct Sensor));
    cooperativo = malloc(n * sizeof(struct Sensor));

    char **lineas;
    lineas = malloc(n * sizeof(char*));    

    readFile(lineas,argv[1]);
    
    char *tmp=malloc(255 * sizeof(char));
    for(int i=0;i<n;i++){
        strcpy(tmp,lineas[i]);
        char *p = strtok (tmp, ",");
        struct Sensor s;
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
                    pthread_attr_init(&attr);
                    pthread_create(&s.tid,&attr,sensor,(void *) &s);
                    break;
            }
            p = strtok (NULL, ",");
            y++;
        }     
        if(s.tipo < 5){
            competitivo[cntComp++]=s;
        }  
        else{
            cooperativo[cntCoop++]=s;
        }  
        free(p);   
        printf("-->Sensor id: %d , tipo: %d, intervalo: %f\n",s.id,s.tipo,s.intervalo);
    }
    printf("Creacion de hilos suma ponderada, varianza y decision");
    /* get the default attributes */
    pthread_attr_init(&attr);
    pthread_create(&tid1,&attr,varianza,NULL);
    pthread_create(&tid2,&attr,suma_ponderada,NULL);

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
    struct Sensor s = *((struct Sensor *)param);
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
    int x=0;
    while(1){
        usleep(deltaT);
        //printf("Thread Varianza Sensor id: %d , tipo: %d, intervalo: %f\n",competitivo[x].id,competitivo[x].tipo,competitivo[x].intervalo);
        
        //varianza = sumatoria( (xi-xmedia)*(x-xmedia) ) / (n-1)
    }   
    pthread_exit(0);
}

void *suma_ponderada(void *param){
    
    while(1){
        usleep(deltaT);
        int sumatoria=0;
        int nActivos=0;
        for(int x=0;x<cntCoop;x++){
            struct Sensor s=cooperativo[x];
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
