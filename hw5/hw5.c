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
#include <math.h>
#include <inttypes.h>


typedef struct {
	float real;
	float imaginary;
} complexNumber;

pthread_mutex_t mutex;
pthread_cond_t conditionVar;
int arrived=0;
int m;
pthread_t *threadSig=NULL;
sig_atomic_t sigintFlag=0;
int size;
int size2;

uint8_t** matrixA;
uint8_t** matrixB;
uint64_t** matrixC;
complexNumber** DFT;

//function declarations
void print_error(char error_message[]);
void convert(uint8_t** matrix, char* input, int number);
void print(uint8_t** matrix, int number);
void print_string(char string[]);
void* threadFunction(void * ind);
void printMatrix64(uint64_t **M, int side);
void printDFT(complexNumber** matrix ,size_t size);
void freeResources(int size);
void signalHandler(int sig);


// Main function creates variables, mutex, threads etc.
// Reads files, collects outputs
int main(int argc, char *argv[]) {

	clock_t start, end;
    double cpu_time_used;
     
     start = clock();
     /* Do the work. */


	//Example:./hw5 -i filePath1 -j filePath2 -o output -n 4 -m 2
	setbuf(stdout,NULL);  //for printing without buffering

	//taking commandline arguments
	int option;
	char * inputPath1;
	char * inputPath2;
	char * outputPath;
	int n;
	
	
	while((option = getopt(argc, argv, ":i:j:o:n:m:")) != -1){ 
		switch(option){
		case 'i':
		    inputPath1=optarg;
		    break;
		case 'j':
		    inputPath2=optarg;
		    break;
		case 'o':
		    outputPath=optarg;
		    break;
		case 'n':
		    n=atoi(optarg);
		    break;
		case 'm':
		    m=atoi(optarg);
		    break;
		case '?': 
		    perror("Wrong input format! You should enter: ./hw5 -i filePath1 -j filePath2 -o output -n 4 -m 2\n");
		    exit(EXIT_FAILURE);
		    break;
		}
    }

    //- n > 2 (integer) -m:even number
    if(n<=2){
    	errno=EINVAL;
    	print_error ("n must be bigger than 2!\n");   
        exit (EXIT_FAILURE);
    }

    if(m%2 !=0){
    	errno=EINVAL;
    	print_error ("m must be even number!\n");   
        exit (EXIT_FAILURE);
    }

    if((optind!=11)){
		errno=EINVAL;
		print_error("Wrong or missing commandline arguments! You should enter:/hw5 -i filePath1 -j filePath2 -o output -n 4 -m 2\n");
		exit(EXIT_FAILURE);
	}

	//reading two input files into memory
	size= pow(2,n);
	size2=size*size;

	// ıf number of threads given by user is bigger than column number, make them as column number and every thread calculates 1 column
	if(m>size){
		m=size;
	}

	// signal handling 
	struct sigaction sa = {0};
    sa.sa_handler = signalHandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
	
	// Open necesarry files
	int fdInput1,fdInput2,fdOut;
	if ((fdInput1 = open(inputPath1, O_RDONLY)) == -1){
	    perror("Failed to open input file in main.\n");
	    exit(EXIT_FAILURE);
	}

	if ((fdInput2 = open(inputPath2, O_RDONLY)) == -1){
	    perror("Failed to open input file in main.\n");
	    exit(EXIT_FAILURE);
	}

	if ((fdOut = open(outputPath, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1){
	    perror("Failed to open output file in main.\n");
	    exit(EXIT_FAILURE);
	}

	if(sigintFlag==1){
	    close(fdInput1);close(fdInput2);close(fdOut);
		exit(EXIT_FAILURE);
	}
	
	// For reading bytes from input files, It uses dynamic arrays.
	char* inputA;
	char* inputB;
	char* inputC;
	inputA = (char*) malloc(size2+1);
	
	int bytesread = 0;

	while(((bytesread = read(fdInput1, inputA, size2)) == -1) && (errno == EINTR));

	
	// If there are not sufficient characters in the files then it will print an error message and exit gracefully.
	if(bytesread != size2){
		
		fprintf (stderr, "\nerrno = %d: %s there are not sufficient characters in the inputPathA file\n\n", errno, strerror (errno));
		exit(1);
	}

	// These characters will be converted into its ASCII code integer equivalent in 2D array.
	matrixA = (uint8_t **)calloc(size, sizeof(uint8_t *));
	for(int i = 0; i < size; i++){ 
		matrixA[i] = (uint8_t *)calloc(size, sizeof(uint8_t));
		memset(matrixA[i],0,sizeof(uint8_t));
	}
	// Convert operation
	convert(matrixA, inputA, size);
	free(inputA);
	
	bytesread = 0;
	inputB = (char*) malloc(size2+1);
	
	while(((bytesread = read(fdInput2, inputB, size2)) == -1) && (errno == EINTR));
	
	if(bytesread != size2){
		fprintf (stderr, "\nerrno = %d: %s there are not sufficient characters in the inputPathB file\n\n", errno, strerror (errno));
		for(int i = 0; i < size; i++)
    		free(matrixA[i]);
		free(matrixA);
		exit(1);
	}

	// These characters will be converted into its ASCII code integer equivalent in 2D array.
	matrixB = (uint8_t **)calloc(size, sizeof(uint8_t *));
	for(int i = 0; i < size; i++){ 
		matrixB[i] = (uint8_t *)calloc(size, sizeof(uint8_t));
	}

	// Convert operation
	convert(matrixB, inputB, size);
	free(inputB);

	time_t t;
	time(&t);
	char message[200];
	sprintf(message,"[%.19s] Two matrices of size %dx%d have been read. The number of threads is %d\n",ctime(&t),size,size,m);
    print_string(message); 

    if(sigintFlag==1){
		for(int i = 0; i < size; i++)
    		free(matrixA[i]);
		free(matrixA);

		for(int i = 0; i < size; i++)
    		free(matrixB[i]);
		free(matrixB);
	    close(fdInput1);close(fdInput2);close(fdOut);
		exit(EXIT_FAILURE);
	}


	inputC = (char*) malloc(size2+1);
	matrixC = (uint64_t **)calloc(size, sizeof(uint64_t *));
	for(int i = 0; i < size; i++){ 
		matrixC[i] = (uint64_t *)calloc(size, sizeof(uint64_t));
	}
	for(int i=0;i<size;i++)
		for(int j=0; j<size;j++)
			matrixC[i][j]=0;
	

	free(inputC);


	//create DFT array dynamically
	DFT= (complexNumber**)calloc(size, sizeof(complexNumber *));
	for(int i = 0; i < size; i++){ 
		DFT[i] = (complexNumber *)calloc(size, sizeof(complexNumber));
	}

	
	close(fdInput1);
	close(fdInput2);

	if(sigintFlag==1){
		freeResources(size);
	    close(fdInput1);close(fdInput2);close(fdOut);
		exit(EXIT_FAILURE);
	}

	//mutex-condition variable parts

	// mutex initialization settings: from textbook
	pthread_mutexattr_t mutexAttr;
	int s;
    s = pthread_mutexattr_init(&mutexAttr);
    if (s != 0){
        perror("pthread_mutexattr_init");
        close(fdInput1);close(fdInput2);close(fdOut);
        freeResources(size);
        exit(EXIT_FAILURE);
    }
    s = pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_ERRORCHECK);
    if (s != 0){
        perror("pthread_mutexattr_settype");
        close(fdInput1);close(fdInput2);close(fdOut);
        freeResources(size);
        exit(EXIT_FAILURE);
    }
    s = pthread_mutex_init(&mutex, &mutexAttr);
    if (s != 0){
        perror("pthread_mutex_init");
        close(fdInput1);close(fdInput2);close(fdOut);
        freeResources(size);
        exit(EXIT_FAILURE);
    }
    s = pthread_mutexattr_destroy(&mutexAttr);
    if (s != 0){
        perror("pthread_mutexattr_destroy");
        close(fdInput1);close(fdInput2);close(fdOut);
        freeResources(size);
        exit(EXIT_FAILURE);
    }
    
    
    // conditionVarition variable initialization settings:
    s = pthread_cond_init(&conditionVar, NULL);
    if (s != 0){
        perror("pthread_condattr_init");
        close(fdInput1);close(fdInput2);close(fdOut);
        freeResources(size);
        exit(EXIT_FAILURE);
    }
    

	if(sigintFlag==1){
		freeResources(size);
	    close(fdInput1);close(fdInput2);close(fdOut);
		exit(EXIT_FAILURE);
	}

    //creating threads
    int i=0;
  	pthread_t m_threads[m];
  	int threadIndexs[m];
  
	for(i = 0; i < m; i++) {
		threadIndexs[i]=i;
		
		if(pthread_create(&m_threads[i], NULL, threadFunction, &threadIndexs[i]) != 0) {
		  perror("ERROR pthread_create ");
		  freeResources(size);
		  close(fdInput1);close(fdInput2);close(fdOut);
		  exit(EXIT_FAILURE);
		}
	}

	threadSig=m_threads;

	if(sigintFlag==1){
		freeResources(size);
		close(fdInput1);close(fdInput2);close(fdOut);
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < m; i++) {
		if(pthread_join(m_threads[i],NULL) != 0) {
		  perror("ERROR pthread_join");
		  freeResources(size);
		  exit(EXIT_FAILURE);
		}
	}

	if(sigintFlag==1){
		freeResources(size);
		close(fdInput1);close(fdInput2);close(fdOut);
		exit(EXIT_FAILURE);
	}

	// Write result DFT matrix to output file
	for (int i = 0; i<size; i++){
	    for (int j = 0; j< size; j++){
	    	
	        sprintf(message," %.3f + %.3f i ,",DFT[i][j].real,DFT[i][j].imaginary);
	        write(fdOut,message,strlen(message));
	       
	    }
    write(fdOut,"\n",sizeof("\n"));
	}

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    if(sigintFlag==1){
		freeResources(size);
		close(fdInput1);close(fdInput2);close(fdOut);
		exit(EXIT_FAILURE);
	}
	
	sprintf(message,"[%.19s] The process has written the output file. The total time spent is %f seconds.\n",ctime(&t),cpu_time_used);
    print_string(message); 


	close(fdOut);

	freeResources(size);
	pthread_exit(0);
}


/*Thread function that uses mutex lock and unlock to solve synchronization
 *Calculates C matrix with matrix multiplication of A and B matrixes
 *After all threads finished calculating C matrix for their parts,
 *They calculates fourier transform matrix
 */
void* threadFunction(void * ind){
	
	time_t t;
	time(&t);
	char message[200];
	int index = *((int *) ind);
	int startInner=(size/m)*index;  
	int endInner=startInner+(size/m);   

	if(size%m != 0 && index==m-1){
		startInner=(size/m)*index;
		endInner=size;
	}
	
	
	clock_t start1, end1;
    double cpu_time_used1;
     
     start1 = clock();
     /* Do the work. */
     // her thread:a b nin hesaplayıp c ye atılması
	for (int i=0; i < size; i++) {
        for (int j=startInner; j <endInner; j++) {

            for (int k = 0; k < size; k++) {
                matrixC[i][j] += matrixA[i][k] * matrixB[k][j];
            }
        }
    }
	end1 = clock();
	cpu_time_used1 = ((double) (end1 - start1)) / CLOCKS_PER_SEC;

	
    sprintf(message,"[%.19s] Thread %d has reached the rendezvous point in %f second.\n",ctime(&t),index,cpu_time_used1);
    print_string(message); 

    //synchronization barrier 
    pthread_mutex_lock(&mutex);
    ++arrived;
    while(arrived < m){
        pthread_cond_wait(&conditionVar,&mutex);
    }

    pthread_cond_broadcast(&conditionVar);
    pthread_mutex_unlock(&mutex);

    // c artık hazır
   
    sprintf(message,"[%.19s] Thread %d is advancing to the second part\n",ctime(&t),index);
    print_string(message); 
    
	clock_t start, end;
	double cpu_time_used;

	start = clock();
	 /* Do the work. */

    // calculating fourier transform
	for(int i=startInner;i<endInner;i++){
		for(int j=0;j<size;j++){
			float ak=0; 
			float bk=0;
			for(int ii=0;ii<size;ii++){
				for(int jj=0;jj<size;jj++){

					float x=-2.0*M_PI*i*ii/(float)size;
					float y=-2.0*M_PI*j*jj/(float)size;
					ak+=matrixC[ii][jj]*cos(x+y);
					bk+=matrixC[ii][jj]*1.0*sin(x+y);
				}
			}
			DFT[i][j].real=ak;
			DFT[i][j].imaginary=bk;

		}
	}
	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    sprintf(message,"[%.19s] Thread %d has finished the second part in %f second.\n",ctime(&t),index,cpu_time_used);
    print_string(message);

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


// Convert characters to its equivalent ASCII code integer .
void convert(uint8_t** matrix, char* input, int number){
	int i, j;
	int x = 0;

	for(i = 0; i < number; i++){
		for(j = 0; j < number; j++){
			matrix[i][j] = (int)input[x];
			x += 1;
		}
	}
}

// Prints matrixs A and B
void print(uint8_t** matrix, int number){
	int i, j;
	for(i = 0; i < number; i++){
		printf("\n");
		for(j = 0; j < number; j++){
			printf("%-7d ", matrix[i][j]);
		}
	}
	printf("\n");
}

// Prints C matrix to the screen, since C matrix is type of uint64, it has its own print method
void printMatrix64(uint64_t **M, int side){
    char str[128];
    for (int i = 0; i < side; ++i) {
        for (int j = 0; j < side; ++j) {
            sprintf(str, "%8lu ", M[i][j]);
            print_string(str);
        }
        printf("\n");
    }
}

// Prints DFT array to screen as matrix
void printDFT(complexNumber** matrix,size_t size){

	char str[128];
	for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < size; ++j) {
            sprintf(str, " r:%3f ", matrix[i][j].real);
            print_string(str);
            sprintf(str, " i:%3f ", matrix[i][j].imaginary);
            print_string(str);
        }
        printf("\n");
    }
}


// Function that frees all resources allocated dynamically
void freeResources(int size){

	int i;
	// free 2D matrixA array.
	for(i = 0; i < size; i++)
    	free(matrixA[i]);
	free(matrixA);

	// free 2D matrixB array.
	for(i = 0; i < size; i++)
    	free(matrixB[i]);
	free(matrixB);

	// free 2D matrixC array.
	for(i = 0; i < size; i++)
    	free(matrixC[i]);
	free(matrixC);

	// free 2D DFT array.
	for(i = 0; i < size; i++)
    	free(DFT[i]);
	free(DFT);
}

// signal handler for SIGINT
void signalHandler(int sig){
    if (sig==SIGINT && sigintFlag!=1){
        sigintFlag=1;
    } 
    sigintFlag=1;
    printf("\nCase CTRL-C: The program was terminated.\n");
}