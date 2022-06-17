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
#include <sys/mman.h>
#include <string.h>
#include <signal.h>

#define SHM_DATA "data_key"
#define SHM_SEMO "semophore_key"
#define SIZE 1024

pid_t chefs[6];
pid_t commonPusherPid;

sig_atomic_t usr2Signal = 0;
sig_atomic_t usr1Signal= 0;

// For avoiding zombie processes.
// Number of children started but not yet waited on.
static volatile int numLiveChild = 0;


//function declarations
void createSharedMem();
void closeSharedMem();
int countCharsInFile(int fd);
void sigchldHandler(int sig);
void handler(int signum) ;
int chefOperation0();
int chefOperation1();
int chefOperation2();
int chefOperation3();
int chefOperation4();
int chefOperation5();
void commonPusher();
char whichIngredientTaken(int i);
void printIngredients();


typedef struct{				//shared memory struct keeps unnamed semaphores and necessary variables

	char ingredients[2];	// it will contain WS,FW,SF according to inputFile

	sem_t wholesalerSem;
	sem_t ingredientsArrived;
	sem_t walnutSugar;
	sem_t flourWalnut;
	sem_t sugarFlour;
	sem_t milkFlour;
	sem_t milkWalnut;
	sem_t sugarMilk;

	int c0,c1,c2,c3,c4,c5;
	int signalArrived;

}shared_mem_struct;

shared_mem_struct *sharedMem;
int fd;



int main(int argc, char *argv[]) {

	//taking commandline arguments
	int option;
	char * inputFilePath;

	//./hw3unnamed -i inputFilePath

	while((option = getopt(argc, argv, ":i:")) != -1){ 
		switch(option){
		case 'i':
		    inputFilePath=optarg;
		    break;
		case '?': 
		    perror("Wrong input format! You should enter: ./hw3unnamed -i inputFilePath\n");
		    exit(EXIT_FAILURE);
		    break;
		}
    }

    if((optind!=3)){
		errno=EINVAL;
		perror("Wrong commandline arguments! You should enter:./hw3unnamed -i inputFilePath\n");
		exit(EXIT_FAILURE);
	}

	int totalNumOfDesserts=0; //her chef tatlı yapmayı tamamladıgında return edicek ve bu sayı artıcak.

	createSharedMem();


	//opening input file
	int fdInput;

	if ((fdInput = open(inputFilePath, O_RDONLY)) == -1){
	    perror("Failed to open input file in main.\n");
	    exit(EXIT_FAILURE);
	}

	int charCounts=countCharsInFile(fdInput); 	//count the chars from input file to create an array
	close(fdInput);
	
	char allFile[charCounts];
	char buf[1];
	int i=0,bytesread;
	
	int fdInput2;
	if ((fdInput2 = open(inputFilePath, O_RDONLY)) == -1){
	    perror("Failed to open input file in main.\n");
	    exit(EXIT_FAILURE);
	}
	while(1){
		while(((bytesread = read(fdInput2, buf, 1)) == -1) &&(errno == EINTR));
		if(bytesread <= 0)
			break;
		
		allFile[i]=buf[0];
		i++;
	}
	close(fdInput2);


	
	//Creating child processes as chefs
	for(int i=0;i<6;i++){
		
		if((chefs[i]=fork())<0){
			perror("Error on fork!");
			exit(EXIT_FAILURE);
		}
		else if(chefs[i]==0) {//child process
			chefs[i]=getpid();

			struct sigaction newactChef;
            newactChef.sa_handler = &handler;
            newactChef.sa_flags = 0;
            if((sigemptyset(&newactChef.sa_mask) == -1) || (sigaction(SIGUSR1, &newactChef, NULL) == -1)){
                perror("Failled to install SIGUSR1 signal handler");
                exit(EXIT_FAILURE);
            }

			if(i==0){
				return chefOperation0();
			}
			else if(i==1){
				return chefOperation1();
			}
			else if(i==2){
				return chefOperation2();
			}
			else if(i==3){
				return chefOperation3();
			}
			else if(i==4){
				return chefOperation4();
			}
			else if(i==5){
				return chefOperation5();
			}
		}	
	}

	int pusher;
	if((pusher=fork())<0){
			perror("Error on fork!");
			exit(EXIT_FAILURE);
	}
	else if(pusher==0){ //child
		struct sigaction newactPushers;
            newactPushers.sa_handler = &handler;
            newactPushers.sa_flags = 0;
            if((sigemptyset(&newactPushers.sa_mask) == -1) || (sigaction(SIGUSR1, &newactPushers, NULL) == -1)){
                perror("Failled to install SIGUSR1 signal handler");
                exit(EXIT_FAILURE);
            }
		commonPusherPid=getpid();
		commonPusher();
	}


	//read file contents two by two and assign sharedMem->ingredients
	//sempost ingredients semwait wholeSaler
	int m=0;
	while(m<charCounts){
		if(sem_wait(&(sharedMem->wholesalerSem))==-1){
			printf("error on sem_wait\n");
			perror("Error on sem_wait\n");
			exit(EXIT_FAILURE);
		}
		sharedMem->ingredients[0]=allFile[m];
		sharedMem->ingredients[1]=allFile[m+1];
		
		printf("The wholesaler (pid %d) delivers %c and %c.",getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1] );
		printIngredients();
		printf("The wholesaler (pid %d) is waiting for the dessert.",getpid() );
		printIngredients();

		if(sem_post(&(sharedMem->ingredientsArrived)) == -1){
			printf("error sempost\n");
			perror("Error on sem_post ingredientsArrived!\n");
			exit(EXIT_FAILURE);
		}
		
		m+=3;
	}

	// Parent comes here: wait for SIGCHLD until all children are dead.
	/*sigemptyset(&emptyMask);
	while(numLiveChild > 0)
	{
		if(sigsuspend(&emptyMask) == -1 && errno != EINTR)
		{
			//sem_wait(commonMutex);
			//char buf[SIZE];
			//sprintf(buf, "\nerrno = %d: %s sigsuspend\n\n", errno, strerror(errno));
			perror("Error on sigsuspend!");
			exit(EXIT_FAILURE);
		}
	}*/

	sleep(3);

	for(int i=0;i<6;i++){
		kill(chefs[i],SIGUSR1);
	}

	kill(pusher,SIGUSR1);

	int  k, status;
    size_t childPid;

    for (k = 0; k < 6; ++k) {
        childPid = waitpid(chefs[k], &status, 0);
         if (childPid == -1) {
            perror("wait error");
            exit(EXIT_FAILURE); 
        }
        totalNumOfDesserts += WEXITSTATUS(status);
    }
    
    childPid = waitpid(pusher, &status, 0);
     if (childPid == -1) {
        perror("wait error");
        exit(EXIT_FAILURE); 
    }
    
	printf("the wholesaler (pid %d) is done (total desserts: %d)\n",getpid(),totalNumOfDesserts );

	//close shared mem and exit
	closeSharedMem();


return 0;

}

/*Creates shared memory segment and initializes its variables
 *initializes unnamed semaphores inside the shared memory
 */
void createSharedMem(){
	fd= shm_open(SHM_SEMO,O_CREAT | O_RDWR, 0666);
	if(fd<0){
		perror("Error on shm_open!");
		exit(EXIT_FAILURE);
	}

	if(ftruncate(fd,sizeof(shared_mem_struct))==-1 && (errno != EINTR)){
		perror("Error on ftruncate!");
		exit(EXIT_FAILURE);
	}

	sharedMem= (shared_mem_struct *)mmap(NULL,sizeof(shared_mem_struct), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(sharedMem==MAP_FAILED){
		perror("Error on mmap sharedMem!");
		exit(EXIT_FAILURE);
	}

	sharedMem->c0=0;
	sharedMem->c1=0;
	sharedMem->c2=0;
	sharedMem->c3=0;
	sharedMem->c4=0;
	sharedMem->c5=0;

	sharedMem->signalArrived=0;

	sharedMem->ingredients[0]='\0';
	sharedMem->ingredients[1]='\0';

	//initializing unnamed semaphores in the shared memory

	if(sem_init(&sharedMem->wholesalerSem, 1, 1) == -1){
		perror("Error sem_init wholesalerSem!\n");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&sharedMem->ingredientsArrived, 1, 0) == -1){
		perror("Error sem_init ingredientsArrived!\n");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&sharedMem->walnutSugar, 1, 0) == -1){
		perror("Error sem_init walnutSugar!\n");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&sharedMem->flourWalnut, 1, 0) == -1){
		perror("Error sem_init flourWalnut!\n");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&sharedMem->sugarFlour, 1, 0) == -1){
		perror("Error sem_init sugarFlour!\n");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&sharedMem->milkFlour, 1, 0) == -1){
		perror("Error sem_init milkFlour!\n");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&sharedMem->milkWalnut, 1, 0) == -1){
		perror("Error sem_init milkWalnut!\n");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&sharedMem->sugarMilk, 1, 0) == -1){
		perror("Error sem_init sugarMilk!\n");
		exit(EXIT_FAILURE);
	}
}

//Closes shared memory, unlinks it.
void closeSharedMem(){

	//make close shared memory operations here
	if(munmap(sharedMem, sizeof(shared_mem_struct)) == -1){
  		
		perror("Error on munmap\n");
		exit(EXIT_FAILURE);
  	}
	if(shm_unlink(SHM_SEMO) == -1){
  		
		perror("Error on shm_unlink!\n");
		exit(EXIT_FAILURE);
  	}

  	//destroy semaphores
	
	sem_destroy(&sharedMem->wholesalerSem);
	sem_destroy(&sharedMem->ingredientsArrived);
	sem_destroy(&sharedMem->walnutSugar);
	sem_destroy(&sharedMem->flourWalnut);
	sem_destroy(&sharedMem->sugarFlour);
	sem_destroy(&sharedMem->milkFlour);
	sem_destroy(&sharedMem->milkWalnut);
	sem_destroy(&sharedMem->sugarMilk);
}


//counts all chars from a file and returns as an integer
int countCharsInFile(int fd) {
    int chars = 0;
    int num_read;
    char c;
    while(1) {
        num_read = read(fd, &c, 1);
        if (num_read == -1) {
            
            perror("Could not read file! countCharsInFile function");
            exit(EXIT_FAILURE);
        } else if (num_read == 0) {
            break;
        } else chars++;
    }
    lseek(fd, 0, SEEK_SET);
    return chars;
}


// SSIGCHLD Handler
void sigchldHandler(int sig){
	// Keep errno
	int savedErrno = errno;

    // P1 catch the SIGCHLD signal and perform a synchronous wait for each of its children.
    // NOTE: The section for handling the SIGCHLD signal is taken from page 557-558 of the textbook.

	int status;
	pid_t cpid;

	while ((cpid = waitpid(-1, &status, WNOHANG)) > 0)
		numLiveChild--;

	if (cpid == -1 && errno != ECHILD)
	{
		char buf[SIZE];
		sprintf(buf, "\nerrno = %d: %s waitpid\n\n", errno, strerror(errno));
		perror("Error on waitpid!");
		exit(EXIT_FAILURE);

	}
    
    // Restore errno
    errno = savedErrno;
}

/*Handler for SIGUSR1 and SIGUSR2 signals*/
void handler(int signum) {
   if(signum==SIGUSR1){
      //puts("Handler caught SIGUSR1 signal.\n"); 
      usr1Signal=1;   
      sharedMem->signalArrived=1;            
   }
   if(signum==SIGUSR2){
      //puts("Handler caught SIGUSR2 signal.\n");  
      usr2Signal=1;    
      sharedMem->signalArrived=1;         
   } 
}


//makes chef0 operations
int chefOperation0(){

	int result=0;
	printf("chef1 (pid %d) is waiting for walnuts and sugar. Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
	for(;;){
		if(sem_wait(&(sharedMem->walnutSugar))== -1){
			if(sharedMem->signalArrived != 0){
				printf("chef1 (pid %d) is exiting. Ingredients in the array: %c %c\n"
				 		,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
				return result;
			}
			printf("error on sem_wait at chef0\n");
			perror("Error sem_wait at chef func\n");
			exit(EXIT_FAILURE);
		}
		result++;
		
		//has taken ... to show chef take ingredient from data structure and that place is empty now.
		printf("chef1 (pid %d) has taken the %c.",getpid(),whichIngredientTaken(0));
		printIngredients();
		printf("chef1(pid %d) has taken the %c.",getpid(),whichIngredientTaken(1));
		printIngredients();
		printf("chef1 (pid %d) is preparing the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("chef1 (pid %d) has delivered the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("the wholesaler (pid %d) has obtained the dessert and left.Ingredients in the array: %c %c\n"
				,getppid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);

		if(sem_post(&(sharedMem->wholesalerSem))== -1){
			perror("Error on sem_post at chef func\n");
		}
	}
}


//makes chef1 operations
int chefOperation1(){
	int result=0;
	printf("chef2 (pid %d) is waiting for flour and walnuts. Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
	for(;;){
		
		if(sem_wait(&(sharedMem->flourWalnut))== -1){
			if(sharedMem->signalArrived != 0){
				printf("chef2 (pid %d) is exiting. Ingredients in the array: %c %c\n"
				 		,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
				return result;
			}
			perror("Error sem_wait at chef func\n");
			exit(EXIT_FAILURE);
		}
		result++;
		printf("chef2 (pid %d) has taken the %c.",getpid(),whichIngredientTaken(0));
		printIngredients();
		printf("chef2(pid %d) has taken the %c.",getpid(),whichIngredientTaken(1));
		printIngredients();
		printf("chef2 (pid %d) is preparing the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("chef2 (pid %d) has delivered the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("the wholesaler (pid %d) has obtained the dessert and left.Ingredients in the array: %c %c\n"
				,getppid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);

		if(sem_post(&(sharedMem->wholesalerSem))== -1){
			perror("Error on sem_post at chef func\n");
		}
	}
}


//makes chef2 operations
int chefOperation2(){
	int result=0;
	printf("chef3 (pid %d) is waiting for sugar and flour. Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
	for(;;){
		
		if(sem_wait(&(sharedMem->sugarFlour))== -1){
			if(sharedMem->signalArrived != 0){
				printf("chef3 (pid %d) is exiting. Ingredients in the array: %c %c\n"
				 		,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
				return result;
			}
			perror("Error sem_wait at chef func\n");
			exit(EXIT_FAILURE);
		}
		result++;
		printf("chef3 (pid %d) has taken the %c.",getpid(),whichIngredientTaken(0));
		printIngredients();
		printf("chef3(pid %d) has taken the %c.",getpid(),whichIngredientTaken(1));
		printIngredients();
		printf("chef3 (pid %d) is preparing the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("chef3 (pid %d) has delivered the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("the wholesaler (pid %d) has obtained the dessert and left.Ingredients in the array: %c %c\n"
				,getppid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);

		if(sem_post(&(sharedMem->wholesalerSem))== -1){
			perror("Error on sem_post at chef func\n");
		}
	}
}

//makes chef3 operations
int chefOperation3(){
	int result=0;
	printf("chef4 (pid %d) is waiting for milk and flour. Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
	for(;;){
		
		if(sem_wait(&(sharedMem->milkFlour))== -1){
			if(sharedMem->signalArrived != 0){
				printf("chef4 (pid %d) is exiting. Ingredients in the array: %c %c\n"
				 		,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
				return result;
			}
			perror("Error sem_wait at chef func\n");
			exit(EXIT_FAILURE);
		}
		result++;
		printf("chef4 (pid %d) has taken the %c.",getpid(),whichIngredientTaken(0));
		printIngredients();
		printf("chef4(pid %d) has taken the %c.",getpid(),whichIngredientTaken(1));
		printIngredients();
		printf("chef4 (pid %d) is preparing the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("chef4 (pid %d) has delivered the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("the wholesaler (pid %d) has obtained the dessert and left.Ingredients in the array: %c %c\n"
				,getppid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);

		if(sem_post(&(sharedMem->wholesalerSem))== -1){
			perror("Error on sem_post at chef func\n");
		}
	}
}


//makes chef4 operations
int chefOperation4(){
	int result=0;
	printf("chef5 (pid %d) is waiting for milk and walnuts. Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
	for(;;){
		
		if(sem_wait(&(sharedMem->milkWalnut))== -1){
			if(sharedMem->signalArrived != 0){
				printf("chef5 (pid %d) is exiting. Ingredients in the array: %c %c\n"
				 		,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
				return result;
			}
			perror("Error sem_wait at chef func\n");
			exit(EXIT_FAILURE);
		}
		result++;
		
		printf("chef5 (pid %d) has taken the %c.",getpid(),whichIngredientTaken(0));
		printIngredients();
		printf("chef5(pid %d) has taken the %c.",getpid(),whichIngredientTaken(1));
		printIngredients();
		printf("chef5 (pid %d) is preparing the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("chef5 (pid %d) has delivered the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("the wholesaler (pid %d) has obtained the dessert and left.Ingredients in the array: %c %c\n"
				,getppid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);

		if(sem_post(&(sharedMem->wholesalerSem))== -1){
			perror("Error on sem_post at chef func\n");
		}
	}
}

//makes chef5 operations
int chefOperation5(){
	int result=0;
	printf("chef6 (pid %d) is waiting for sugar and milk. Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
	for(;;){
		
		if(sem_wait(&(sharedMem->sugarMilk))== -1){
			if(sharedMem->signalArrived != 0){
				printf("chef6 (pid %d) is exiting. Ingredients in the array: %c %c\n"
				 		,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
				return result;
			}
			perror("Error sem_wait at chef func\n");
			exit(EXIT_FAILURE);
		}
		result++;

		printf("chef6 (pid %d) has taken the %c.",getpid(),whichIngredientTaken(0));
		printIngredients();
		printf("chef6(pid %d) has taken the %c.",getpid(),whichIngredientTaken(1));
		printIngredients();

		printf("chef6 (pid %d) is preparing the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("chef6 (pid %d) has delivered the dessert.Ingredients in the array: %c %c\n"
				,getpid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);
		printf("the wholesaler (pid %d) has obtained the dessert and left.Ingredients in the array: %c %c\n"
				,getppid(),sharedMem->ingredients[0],sharedMem->ingredients[1]);

		if(sem_post(&(sharedMem->wholesalerSem))== -1){
			perror("Error on sem_post at chef func\n");
		}
	}
}


//makes common pusher's operations
void commonPusher(){
	
	for(;;){
		if(sem_wait(&(sharedMem->ingredientsArrived))==-1){
			if(sharedMem->signalArrived != 0){
				
				exit(EXIT_FAILURE);
			}
				perror("Error on sem_wait at pusher!\n");
				exit(EXIT_FAILURE);
		}

		
		if(sharedMem->ingredients[0]=='W' && sharedMem->ingredients[1]=='S'){
			if(sem_post(&(sharedMem->walnutSugar))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='F' && sharedMem->ingredients[1]=='W'){
			if(sem_post(&(sharedMem->flourWalnut))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='S' && sharedMem->ingredients[1]=='F'){
			if(sem_post(&(sharedMem->sugarFlour))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='M' && sharedMem->ingredients[1]=='F'){
			if(sem_post(&(sharedMem->milkFlour))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='M' && sharedMem->ingredients[1]=='W'){
			if(sem_post(&(sharedMem->milkWalnut))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='S' && sharedMem->ingredients[1]=='M'){
			if(sem_post(&(sharedMem->sugarMilk))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}

		////reverse
		if(sharedMem->ingredients[0]=='S' && sharedMem->ingredients[1]=='W'){
			if(sem_post(&(sharedMem->walnutSugar))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='W' && sharedMem->ingredients[1]=='F'){
			if(sem_post(&(sharedMem->flourWalnut))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='F' && sharedMem->ingredients[1]=='S'){
			if(sem_post(&(sharedMem->sugarFlour))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='F' && sharedMem->ingredients[1]=='M'){
			if(sem_post(&(sharedMem->milkFlour))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='W' && sharedMem->ingredients[1]=='M'){
			if(sem_post(&(sharedMem->milkWalnut))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}
		if(sharedMem->ingredients[0]=='M' && sharedMem->ingredients[1]=='S'){
			if(sem_post(&(sharedMem->sugarMilk))){
				perror("Error sem_post\n");
				exit(EXIT_FAILURE);
			}
		}

	}
	
}

//Function to clean datastructure after chef has taken ingredients
char whichIngredientTaken(int i){
	char value=sharedMem->ingredients[i];
	sharedMem->ingredients[i]='\0';
	return value;
}


void printIngredients(){
	printf("Ingredients in the array: %c %c\n",sharedMem->ingredients[0],sharedMem->ingredients[1]);
}