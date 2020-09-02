#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// -lpthread para comilar
struct Sensor
    {
        int id,tipo,comm,th,valorAct;
        int fdsPipe[2];
        pid_t pid;
        pthread_t tid;
    };
void *runner(void *param); /* the thread */


int main(int argc, char *argv[])
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    
    
    struct Sensor *competitivo;
    struct Sensor *cooperativo;

    int cntComp=0;
    int cntCoop=0;
    char **lineas;

    int n=3;
    
    
    competitivo = malloc(n * sizeof(struct Sensor));
    cooperativo = malloc(n * sizeof(struct Sensor));
    lineas = malloc(n * sizeof(char*));    

    fp = fopen(argv[1], "r");
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
    

    //Asignacion de sensores a listas
    char *comando[4];
    
    
    char *tmp=malloc(255 * sizeof(char));
    for(int i=0;i<n;i++){
        strcpy(tmp,lineas[i]);
        char *p = strtok (tmp, ",");
        struct Sensor s;
        int fds[2];
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
                    s.th=atoi(p);
                    pid_t cpid;
                    cpid = fork();
                    if (cpid == -1) {
                        perror("fork");
                        exit(EXIT_FAILURE);
                    }
                    pipe(fds);
                    if (cpid == 0) {       
                          
                        printf("Creando hijos");     
                        /* Code executed by child */
                        close(1); //close the stdout
                        dup(fds[1]);          // redirect standard output to the pipe table;
                        close(fds[0]); close(fds[1]); //closed unused fids
                        
                        comando[0] = "./lectorx"; 
                        char strId[25];
                        sprintf(strId,"%d",s.id);
                        char strComm[25];
                        sprintf(strComm,"%d",s.comm);
                        comando[1] =  strId; 
                        comando[2] =  strComm;  
                        comando[3]=0;
                        printf("error %d",execv(comando[0], comando));
                        exit(0);
                        
                    }
                    else {                    /* Code executed by parent */
                        close(fds[1]);
                        printf("LEYENDO DE %d",fds[0]);
                        s.pid=cpid;
                        s.fdsPipe[0]=fds[0];
                        s.fdsPipe[1]=fds[1];
                        // create the thread 
                        pthread_t tid; /* the thread identifier */
                        pthread_attr_t attr; /* set of attributes for the thread */
                        /* get the default attributes */
                        pthread_attr_init(&attr);
                        pthread_create(&tid,&attr,runner,(void *) &s);
                        s.tid=tid;
                    }
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
        printf("-->Sensor id: %d , tipo: %d, threshold: %d\n",s.id,s.tipo,s.th);
    }
    printf("HELLO");
    sleep(10);
    for(int i=0;i<cntComp;i++){        
        printf("Sensor competitivo id: %d , tipo: %d, threshold: %d, valor actual %d\n",
        competitivo[i].id,competitivo[i].tipo,competitivo[i].th,competitivo[i].valorAct);
        pid_t w;  
        int status;
        pthread_cancel(competitivo[i].tid);
        w = waitpid(competitivo[i].pid, &status, WUNTRACED | WCONTINUED);
        fprintf(stderr,"Child competitivo finished \n");
        
    }
    
    for(int i=0;i<cntCoop;i++){        
        printf("Sensor Cooperativo id: %d , tipo: %d, threshold: %d, valor actual %d\n",
        cooperativo[i].id,cooperativo[i].tipo,cooperativo[i].th,cooperativo[i].valorAct);
        pid_t w;  
        int status;
        pthread_cancel(cooperativo[i].tid);
        w = waitpid(cooperativo[i].pid, &status, WUNTRACED | WCONTINUED);
        fprintf(stderr,"Child cooperativo finished \n");
    }
    

    
}

/**
 * The thread will begin control in this function
 */
void *runner(void *param) 
{
    struct Sensor s = *((struct Sensor *)param);
    
    //close(s.fdsPipe[1]);
    char * line = malloc(255*sizeof(char));
    char *buffer = calloc(sizeof(char),1);
    printf("ERROR %d",read(s.fdsPipe[0], buffer, 1));
    printf("hola");
    while (1){
        while ((read(s.fdsPipe[0], buffer, 1)) > 0){
            printf("Sensor competitivo id: %d , tipo: %d, threshold: %d, valor actual %d\n",
            s.id,s.tipo,s.th,s.valorAct);
            printf("Caract %s\n", buffer);
            if (strcmp(buffer,"\n")==0){
                strcat(line,"\0");
                char *p = strtok (line, " ");
                int y=0;
                while (p != NULL){
                    switch (y)
                    {
                    case (2):
                        s.valorAct=atoi(p);
                        printf("%d",s.valorAct);
                        break;
                    }
                    y++;
                    p = strtok (NULL, " ");
                }
                line = malloc(255*sizeof(char));
            }
            else
            {
                strcat(line,buffer);
            }            
        }
        printf("MIra la Linea %s\n", line);
    }
    pthread_exit(0);


}


/*#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
	int fds[2];
    char *argva[4];
	char foo[4096];
    pipe(fds);                         // create pipe
    if (fork() == 0) { //WRITE
		close(1); //close the stdout
		dup(fds[1]);          // redirect standard output to the pipe table;
		close(fds[0]); close(fds[1]); //closed unused fids
		//argva[0] = "/bin/top"; argva[1] =  0;
        argva[0] = "./lectorx"; argva[1] =  "1"; argva[2] =  "1000";
		execv(argva[0], argva);
	   
       	exit(0);
		
	} else { //read father
		close(fds[1]); 
		while(1){
			int nbytes = read(fds[0], foo, sizeof(foo));
			printf("%s", foo);
		}
		
		exit(0);
       
    }
}*/

