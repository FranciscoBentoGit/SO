#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <semaphore.h>
#include "fs.h"  
#include "lib/hash.h" 

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

extern int numberBuckets;

// Struct and file variables
tecnicofs* fs;
FILE *input,*output;

// Thread and semaphore variables
pthread_mutex_t commandLock; // For the command vector access and iNumber obtainment
sem_t produce, consume;

int numberThreads;

// Thread macro 
#ifdef MUTEX 
    int compileOption = 0; // Variable used to differentiate chosen compilation option from others
#elif RWLOCK 
    int compileOption = 1; // Variable used to differentiate chosen compilation option from others
#else 
    int compileOption = -1; // Variable used to differentiate chosen compilation option from others
#endif
#define MUTEX_COMMAND_LOCK(commandLock) operationStatus = pthread_mutex_lock(&commandLock)
#define MUTEX_COMMAND_UNLOCK(commandLock) operationStatus = pthread_mutex_unlock(&commandLock)
#define SEM_WAIT(sem) operationStatus = sem_wait(&sem)
#define SEM_POST(sem) operationStatus = sem_post(&sem)
#define ASSERT_CHECK assert(operationStatus == 0) // Verifies that a specific operation executes succesfully 

//Command variables
char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
char inputFile[MAX_INPUT_SIZE];
char outputFile[MAX_INPUT_SIZE];
int producerPtr = 0, consumerPtr = 0;

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc == 5) {
        strcpy(inputFile, argv[1]);
        strcpy(outputFile, argv[2]);
        if (compileOption != -1) {
            numberThreads = atoi(argv[3]);
        } else {
            numberThreads = 1;
        }
        numberBuckets = atoi(argv[4]);
    } else if (argc == 4 && compileOption == -1) {
        strcpy(inputFile, argv[1]);
        strcpy(outputFile, argv[2]);
        numberThreads = 1;
        numberBuckets = atoi(argv[3]); 
    } else {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
}

int insertCommand(char* data) { // producer
    if (strlen(data) <= MAX_INPUT_SIZE) {
        SEM_WAIT(produce);
        ASSERT_CHECK;
        strcpy(inputCommands[producerPtr++%MAX_COMMANDS], data);
        SEM_POST(consume);
        ASSERT_CHECK;
        return 1;
    }
    return 0;
}

char* removeCommand() { // consumer
    SEM_WAIT(consume);
    ASSERT_CHECK;
    MUTEX_COMMAND_LOCK(commandLock);
    ASSERT_CHECK; 
    return inputCommands[consumerPtr++%MAX_COMMANDS];  
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void processInput(){
    char line[MAX_INPUT_SIZE];
    while (fgets(line, sizeof(line)/sizeof(char), input)) { 
        char token;
        char name[MAX_INPUT_SIZE];
        char rename[MAX_INPUT_SIZE];
        int numTokens = sscanf(line, "%c %s %s", &token, name, rename);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
            case 'l':
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line)) {
                    break;
                }                    
            case 'r':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line)) {
                    break;
                }
                return;
            case '#':
                break;
            default: { /* error */
                errorParse();
            }
        }
    }
    for (int i = 0; i < numberThreads; i++) {
        insertCommand("f"); 
    }
}


void* applyCommands(){    
    while(1){
        const char* command = removeCommand();
        if (strcmp(command,"f") == 0) { // Whenever a thread reads a command 'f', the calling thread breaks from the while
            MUTEX_COMMAND_UNLOCK(commandLock);
            ASSERT_CHECK;
            SEM_POST(produce);
            ASSERT_CHECK;
            break;
        }
        char token;
        char name[MAX_INPUT_SIZE];
        char rename[MAX_INPUT_SIZE];
        sscanf(command, "%c %s %s", &token, name, rename);
        int searchResult, iNumber;
        int bucketIndex = hash(name, numberBuckets);
        switch (token) {
            case 'c':
                iNumber = obtainNewInumber(fs);
                MUTEX_COMMAND_UNLOCK(commandLock);
                ASSERT_CHECK;
                SEM_POST(produce);
                ASSERT_CHECK;
                create(fs, name, iNumber, bucketIndex);
                break;
            case 'l':
                MUTEX_COMMAND_UNLOCK(commandLock);
                ASSERT_CHECK;
                SEM_POST(produce);
                ASSERT_CHECK;
                searchResult = lookup(fs, name, bucketIndex);
                if(!searchResult)
                    fprintf(stdout,"%s not found\n", name);
                else
                    fprintf(stdout,"%s found with inumber %d\n", name, searchResult);
                break;
            case 'd':
                MUTEX_COMMAND_UNLOCK(commandLock);
                ASSERT_CHECK;
                SEM_POST(produce);
                ASSERT_CHECK;
                delete(fs, name, bucketIndex);
                break;
            case 'r':
                MUTEX_COMMAND_UNLOCK(commandLock);
                ASSERT_CHECK;
                SEM_POST(produce);
                ASSERT_CHECK;
                renameNode(fs, name, rename, bucketIndex);
                break;
            default: { /* error */
                MUTEX_COMMAND_UNLOCK(commandLock);
                ASSERT_CHECK;
                SEM_POST(produce);
                ASSERT_CHECK;
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int i = 0, err;
    struct timeval start, end; // gettimeofday struct variables
    srand(time(NULL)); // Seeding the random number generator used by the function rand
    
    parseArgs(argc, argv); // Reads the 3 arguments (Input,Output,Threads) from file
    if (numberBuckets <= 0) {
        fprintf(stderr, "Error: Please use atleast one bucket.\n");
        exit(EXIT_FAILURE);
    }
    fs = new_tecnicofs();

    // File opening 
    input = fopen(argv[1],"r");
    output = fopen(argv[2],"w");
    if (input == NULL) {
        perror(argv[1]);    
        exit(EXIT_FAILURE);
    }
    if (output == NULL) {
        perror(argv[2]);
        exit(EXIT_FAILURE);
    }

    if (sem_init(&produce, 0, MAX_COMMANDS) != 0 || sem_init(&consume, 0, 0) != 0) {
            fprintf(stderr, "Error: Couldn't initialize semaphore\n");
            exit(EXIT_FAILURE);
        }
    // Thread handling
    pthread_t tid[numberThreads]; // Thread declaration according to compilation arguments received
    if (compileOption == -1) {
        if ((err = gettimeofday(&start, NULL) != 0)) { 
            fprintf(stderr, "Error:Couldn't gettimeofday\n"); 
            exit(EXIT_FAILURE);
        }
        err = pthread_create(&(tid[0]), NULL, applyCommands, NULL);
        if (err != 0) {
            fprintf(stderr, "Error: Couldn't create thread\n");
            exit(EXIT_FAILURE);
        }
        processInput(); 
        err = pthread_join(tid[0], NULL);
        if (err != 0) {
            fprintf(stderr, "Error: Couldn't join thread\n");
            exit(EXIT_FAILURE);
        }
    } else {
        if (numberThreads <= 0) {
            fprintf(stderr, "Error: Please use a thread number higher than 0 for this specific compiling option\n");
            exit(EXIT_FAILURE);
        }
        if (pthread_mutex_init(&commandLock, NULL) != 0) {
            fprintf(stderr, "Error: Couldn't initialize mutex\n");
            exit(EXIT_FAILURE);
        }   
        if ((err = gettimeofday(&start, NULL) != 0)) { 
            fprintf(stderr, "Error: Couldn't gettimeofday\n"); 
            exit(EXIT_FAILURE);
        }   
        while (i < numberThreads) {
            err = pthread_create(&(tid[i]), NULL, applyCommands, NULL);
            if (err != 0) {
                fprintf(stderr, "Error: Couldn't create thread\n");
                exit(EXIT_FAILURE);
            }
            i++;
        }
        processInput(); 
        for (i = 0; i < numberThreads; i++) {
            err = pthread_join(tid[i], NULL);
            if (err != 0) {
                fprintf(stderr, "Error: Couldn't join thread\n");
                exit(EXIT_FAILURE);
            }
        }
        if (pthread_mutex_destroy(&commandLock) != 0) {
            fprintf(stderr, "Error: Couldn't destroy mutex\n");
            exit(EXIT_FAILURE);
        } // Destroying the lock that takes care of the command vector and the iNumber obtainment
    }

    if (sem_destroy(&produce) != 0 || sem_destroy(&consume) != 0) {
            fprintf(stderr, "Error: Couldn't destroy semaphore\n");
            exit(EXIT_FAILURE);
        }

    fclose(input);   
    // Output handling
    print_tecnicofs_tree(output, fs);
    fclose(output);
    free_tecnicofs(fs);

    // Time handling
    if ((err = gettimeofday(&end, NULL) != 0)) { // Finish clock and print time with four decimal places.
        fprintf(stderr, "Error:Couldn't gettimeofday\n");
        exit(EXIT_FAILURE);
    } 
    printf("TecnicoFS completed in %.4f seconds.\n", (((double)(end.tv_sec) + (double)(end.tv_usec / 1000000.0)) - ((double)(start.tv_sec) + (double)(start.tv_usec / 1000000.0))));
    
    exit(EXIT_SUCCESS);
}
