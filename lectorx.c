// C program sensor simulation
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h> 

//uso ./lectorx id comm
//ej ./sensorx 4
//kill -2 PID, para deterner correctamente el proceso


#define SHMSZ     4
int id,tipo,comm,interval,dato,min,max,noout;
int shmid,*shm;

void sig_handlerINT(int signo)
{
  if (signo == SIGINT){
    printf("lector de sensor %d stop \n",id);
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
    if (argc != 3)  
    { 
        printf("enter 2 arguments only eg.\"filename arg1 arg2 arg3!!\""); 
        return 0; 
    } 
  
    // Converting string type to integer type 
    // using function "atoi( argument)"  
    id = atoi(argv[1]);  
    comm = atoi(argv[2]); 

    if ((shmid = shmget(comm, SHMSZ,  0666)) < 0) {
        perror("shmget");
        return(1);
    }
    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        return(1);
    }
    printf("sensor %d connected data \n",id);
    while(1){
        if (*shm==-1)
	{
         printf("Sensor detenido\n");
	 close(shmid);
	 break;
        }
	printf("Dato recibido: %d \n",*shm);
        
   }
exit(1);
}


