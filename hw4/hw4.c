#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>


pthread_t *threadSig=NULL;
sig_atomic_t sigintFlag=0;
int semid;
int N=0;
int C=0; 


void* supplierFunc(void * inputFilePath);
void* consumerFunc(void * ind);
void print_string(char string[]);
void print_error(char error_message[]);
void signalHandler(int sig);


int main(int argc, char *argv[]) {


	setbuf(stdout,NULL);  //for printing without buffering

	//taking commandline arguments
	int option;
	char * inputFilePath;
	
	// ./hw4 -C 10 -N 5 -F inputfilePath
	while((option = getopt(argc, argv, ":F:C:N:")) != -1){ 
		switch(option){
		case 'F':
		    inputFilePath=optarg;
		    break;
		case 'C':
		    C=atoi(optarg);
		    break;
		case 'N':
		    N=atoi(optarg);
		    break;
		case '?': 
		    perror("Wrong input format! You should enter: ./hw4 -C 10 -N 5 -F inputfilePath\n");
		    exit(EXIT_FAILURE);
		    break;
		}
    }

    //- C > 4 (integer) - N > 1 (integer)
    if(C<=4){
    	errno=EINVAL;
    	print_error ("Invalid C value error\n");   
        exit (EXIT_FAILURE);
    }

    if(N<=1){
    	errno=EINVAL;
    	print_error ("Invalid N value error\n");   
        exit (EXIT_FAILURE);
    }


    if((optind!=7)){
		errno=EINVAL;
		perror("Wrong commandline arguments! You should enter:./hw4 -C 10 -N 5 -F inputfilePath\n");
		exit(EXIT_FAILURE);
	}


	//System V semaphore 
    union semun  
    {
        int val;
        struct semid_ds *buf;
        ushort array [1];
    } sem_attr;

    semid = semget( IPC_PRIVATE,2, 0666 | IPC_CREAT);
    if (semid == -1){
        perror("semid semget() error");
        exit(EXIT_FAILURE);
    }

    sem_attr.val = 0;
    if (semctl( semid, 0,SETVAL, sem_attr) == -1){
        perror("error on semctl()");
        exit(EXIT_FAILURE);
    }
    if (semctl( semid, 1, SETVAL, sem_attr) == -1){
        perror("error on semctl()");
        exit(EXIT_FAILURE);
    }
	
	struct sigaction sa = {0};
    sa.sa_handler = signalHandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

	//creating supplier thread.SUPPLIER IS A DETACHED THREAD
	pthread_t supplier_thread;
	pthread_attr_t detachedThread;
    pthread_attr_init(&detachedThread);
    pthread_attr_setdetachstate(&detachedThread, PTHREAD_CREATE_DETACHED);

	if(pthread_create(&supplier_thread, &detachedThread, supplierFunc, inputFilePath) != 0) {
      perror("ERROR pthread_create ");
      exit(EXIT_FAILURE);
    }

    pthread_attr_destroy(&detachedThread);

    //creating consumer threads
    int *index;
    int i=0;
  	pthread_t consumer_threads[C];
  
	for(i = 0; i < C; i++) {
		index=&i;
		if(pthread_create(&consumer_threads[i], NULL, consumerFunc, index) != 0) {
		  perror("ERROR pthread_create consumers");
		  exit(EXIT_FAILURE);
		}
	}

	threadSig=consumer_threads;

	for(i = 0; i < C; i++) {
		if(pthread_join(consumer_threads[i],NULL) != 0) {
		  perror("ERROR pthread_create consumers");
		  exit(EXIT_FAILURE);
		}
	}

	//remove semaphores
	if (semctl (semid, 0, IPC_RMID) == -1) {
        perror ("semctl IPC_RMID");
        exit (1);
    }

	pthread_exit(0);
}


//supplier thread function
void* supplierFunc(void * inputFilePath){
	
	time_t t;
	time(&t);
	struct sembuf asem;

	asem .sem_num = 0;
	asem .sem_op = 1;
	asem .sem_flg = 0;

	int fdInput;
	if ((fdInput = open(inputFilePath, O_RDONLY)) == -1){
	    perror("Failed to open input file in main.\n");
	    exit(EXIT_FAILURE);
	}

	char buf[1];
	int i=0,bytesread;
	
	
	while(1){
		while(((bytesread = read(fdInput, buf, 1)) == -1) &&(errno == EINTR));
		if(bytesread <= 0)
			break;
		if(buf[0]=='1'){
			asem.sem_num= 0;  //semaphore that represents 1s
			printf("[%19.s] Supplier: read from input a '1'. Current amounts %d x '1', %d x '2'.\n"
					,ctime(&t),semctl(semid,0,GETVAL),semctl(semid,1,GETVAL));

			/* post operation */
			asem .sem_op = 1;                     
			if (semop (semid, &asem, 1) == -1) {  
			    print_error ("supplierFunc | semop error\n");   
			    exit (EXIT_FAILURE);                 
        	}  

        	printf("[%19.s] Supplier: delivered a '1'. Current amounts %d x '1', %d x '2'.\n"
					,ctime(&t),semctl(semid,0,GETVAL),semctl(semid,1,GETVAL));
		}
		if(buf[0]=='2'){
			asem.sem_num=1;	//semaphore that represents 2s
			printf("[%19.s] Supplier: read from input a '2'. Current amounts %d x '1', %d x '2'.\n"
					,ctime(&t),semctl(semid,0,GETVAL),semctl(semid,1,GETVAL));

			/* post operation */
			asem .sem_op = 1;                     
			if (semop (semid, &asem, 1) == -1) {  
			    print_error ("supplierFunc | semop error\n");   
			    exit (EXIT_FAILURE);                 
        	} 

        	printf("[%19.s]Supplier: delivered a '2'. Current amounts %d x '1', %d x '2'.\n"
					,ctime(&t),semctl(semid,0,GETVAL),semctl(semid,1,GETVAL));
		}
		i++;
	}

	close(fdInput);
	printf("[%19.s]The Supplier has left.\n",ctime(&t));
	//pthread_exit(0);
	return NULL;
}


//consumer thread function
void* consumerFunc(void * ind){
	
	time_t t;
	time(&t);
	char message[200];
	int index = *((int *) ind);

	struct sembuf asem[2];

	asem [0].sem_num = 0;	//represents 1s
	asem [0].sem_op = -1;
	asem [0].sem_flg = 0;

	asem [1].sem_num = 1;	//represents 2s
	asem [1].sem_op = -1;
	asem [1].sem_flg = 0;

	
	for(int i=0;i<N;i++){

		//before consuming
		sprintf(message,"[%.19s] Consumer-%d at iteration %d (waiting). Current amounts: %d x '1', %d x '2'\n",
			ctime(&t),index,i,semctl(semid,0,GETVAL),semctl(semid,1,GETVAL));

		print_string(message);

		//wait operation                 
        if (semop (semid, asem, 2) == -1) {  
	        print_error ("consumerFunc| semop error\n");   
            exit (EXIT_FAILURE);                 
        }  

        //after consuming
        sprintf(message,"[%.19s] Consumer-%d at iteration %d (consumed). Post-consumption amounts: %d x '1', %d x '2'\n"
        	,ctime(&t),index,i,semctl(semid,0,GETVAL),semctl(semid,1,GETVAL));

        print_string(message);

	}
	sprintf(message,"[%.19s]Consumer-%d has left.\n",ctime(&t),index);
	print_string(message);
	//pthread_exit(0);
	return NULL;

}

// Prints string in the STDOUT 
void print_string(char string[]){
    if(write(STDOUT_FILENO,string,strlen(string))<0){
        perror("print_string | write error\n");
        exit(EXIT_FAILURE);
    }
}

//Prints error in the STDERR
void print_error(char error_message[]){
    if(write(STDERR_FILENO,error_message,strlen(error_message))<0){
        exit(EXIT_FAILURE);
    }
}

// signal handler for SIGINT
void signalHandler(int sig){
    if (sig==SIGINT && sigintFlag!=1){
        sigintFlag=1;
		if (threadSig!=NULL){
			for (int i = 0; i < C; i++)
				pthread_kill(threadSig[i],SIGINT);
		}
    } 
    printf("\nCase CTRL-C: The program was terminated.\n");
}