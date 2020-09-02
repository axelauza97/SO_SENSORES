#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
int main(int argc, char *argv[])
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    struct Sensor
    {
        int id,tipo,comm,th,valorAct;
        pid_t pid;
    };
    
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
    char buffer[4096];
    int fds[2];
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
                    
                    pid_t cpid;
                    cpid = fork();
                    if (cpid == -1) {
                        perror("fork");
                        exit(EXIT_FAILURE);
                    }
                    
                    pipe(fds);  
                    if (cpid == 0) {            
                        /* Code executed by child */
                        close(1); //close the stdout
                        dup(fds[1]);          // redirect standard output to the pipe table;
                        close(fds[0]); close(fds[1]); //closed unused fids
                        comando[0] = "./lectorx"; 
                        comando[1] =  s.id; 
                        comando[2] =  s.comm;                        
                        execv(comando[0], comando);
                    }
                    else {                    /* Code executed by parent */
                        s.pid=cpid;
                    }
                    break;
                case 3:
                    s.th=atoi(p);
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
    for(int i=0;i<cntComp;i++){        
        printf("Sensor Comparativo id: %d , tipo: %d, threshold: %d\n",competitivo[i].id,competitivo[i].tipo,competitivo[i].th);
        pid_t w;  
        int status;
        w = waitpid(competitivo[i].pid, &status, WUNTRACED | WCONTINUED);
        fprintf(stderr,"Child competitivo finished \n");
    }
    
    for(int i=0;i<cntCoop;i++){        
        printf("Sensor Cooperativo id: %d , tipo: %d, threshold: %d\n",cooperativo[i].id,cooperativo[i].tipo,cooperativo[i].th);
        pid_t w;  
        int status;
        w = waitpid(cooperativo[i].pid, &status, WUNTRACED | WCONTINUED);
        fprintf(stderr,"Child cooperativofinished \n");
    }
    

    
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

