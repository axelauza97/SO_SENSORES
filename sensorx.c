// C program sensor simulation
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h> 

//uso ./sensorx id tipo comm interval min max out
//ej ./sensorx 1 1 2345 100 30 45 1  
//sesnor id 1  de tipo 2 con puerto 2345 envia datos cada 100 ms y lso datos estan en el rango [30 45] no se imprime en pantalla

//ej ./sensorx 1 1 2345 100 30 45 1 & para ejecutar en background 
//kill -2 PID, para deterner correctamente el proceso


#define SHMSZ     4
int id,tipo,comm,interval,dato,min,max,noout;
int shmid,*shm;

void sig_handlerINT(int signo)
{
  if (signo == SIGINT){
    printf("sensor %d stop \n",id);
    *shm=-1;
    close(shmid);
  }
  exit(1);
}
  
// Taking argument as command line 
int main(int argc, char *argv[])  
{ 
    if (signal(SIGINT, sig_handlerINT) == SIG_ERR)
	printf("\ncan't catch SIGINT\n");
    // Checking if number of argument is 
    // equal to 4 or not. 
    if (argc != 8)  
    { 
        printf("enter 7 arguments only eg.\"filename arg1 arg2 arg3!!\""); 
        return 0; 
    } 
  
    // Converting string type to integer type 
    // using function "atoi( argument)"  
    id = atoi(argv[1]);  
    tipo = atoi(argv[2]); 
    comm = atoi(argv[3]); 
    interval= atoi(argv[4]); 
    min = atoi(argv[5]); 
    max= atoi(argv[6]); 
    noout=atoi(argv[7]); 

    if ((shmid = shmget(comm, SHMSZ, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        return(1);
    }
    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        return(1);
    }
    printf("sensor %d sendind data \n",id);
    while(1){
	usleep(interval);
	*shm= (int) (rand() % (max - min + 1)) + min; 
        if (noout==0) printf("Dato enviado: %d \n",*shm);
   }
}


