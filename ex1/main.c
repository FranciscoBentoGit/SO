#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include "fs.h"   

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

// Thread variables
pthread_mutex_t commandLock; // For the command vector access and iNumber obtainment
pthread_mutex_t treeLock; // For the create, lookup and delete operations (using mutex)
pthread_rwlock_t treeRWLock; // For the create, lookup and delete operations (using rwlock)

int operationStatus; // Global variable intended for assert operations

// Thread macro 

#define MUTEX_COMMAND_LOCK(commandLock) operationStatus = pthread_mutex_lock(&commandLock)
#define MUTEX_COMMAND_UNLOCK(commandLock) operationStatus = pthread_mutex_unlock(&commandLock)
#define ASSERT_CHECK assert(operationStatus == 0) // Verifies that a specific operation executes succesfully

#ifdef MUTEX
    int compileOption = 0; // Variable used to differentiate chosen compilation option from others
    #define MUTEX_TREE_LOCK(treeLock) operationStatus = pthread_mutex_lock(&treeLock)
    #define MUTEX_TREE_UNLOCK(treeLock) operationStatus = pthread_mutex_unlock(&treeLock)
    #define RWLOCK_RDLOCK(treeRWLock)
    #define RWLOCK_WRLOCK(treeRWLock)
    #define RWLOCK_UNLOCK(treeRWLock)
#elif RWLOCK
    int compileOption = 1; // Variable used to differentiate chosen compilation option from others
    #define RWLOCK_RDLOCK(treeRWLock) operationStatus = pthread_rwlock_rdlock(&treeRWLock)
    #define RWLOCK_WRLOCK(treeRWLock) operationStatus = pthread_rwlock_wrlock(&treeRWLock)
    #define RWLOCK_UNLOCK(treeRWLock) operationStatus = pthread_rwlock_unlock(&treeRWLock)
    #define MUTEX_TREE_LOCK(treeLock)
    #define MUTEX_TREE_UNLOCK(treeLock)
#else 
    int compileOption = -1; // Variable used to differentiate chosen compilation option from others
    #define RWLOCK_RDLOCK(treeRWLock)
    #define RWLOCK_WRLOCK(treeRWLock)
    #define RWLOCK_UNLOCK(treeRWLock)
    #define MUTEX_TREE_LOCK(treeLock)
    #define MUTEX_TREE_UNLOCK(treeLock)
#endif

// Struct and file variables
tecnicofs* fs;
FILE *input,*output; 

//Command variables
char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
char inputFile[MAX_INPUT_SIZE];
char outputFile[MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc == 4) {
        strcpy(inputFile, argv[1]);
        strcpy(outputFile, argv[2]);
    } else if (argc == 3) { // Case where number of threads is not specified (nosync)
        strcpy(inputFile, argv[1]);
        strcpy(outputFile, argv[2]);
    } else {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
}

int insertCommand(char* data) {
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {
    MUTEX_COMMAND_LOCK(commandLock); // Limiting the command vector usage to one thread at a time
    if(numberCommands > 0){
        numberCommands--;
        return inputCommands[headQueue++];  
    }
    MUTEX_COMMAND_UNLOCK(commandLock); // Unlocking immediately when there are no more commands left
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    //exit(EXIT_FAILURE);
}

void processInput(){
    char line[MAX_INPUT_SIZE];

    while (fgets(line, sizeof(line)/sizeof(char), input)) { 
        char token;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(line, "%c %s", &token, name);

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
                return;
            case '#':
                break;
            default: { /* error */
                errorParse();
            }
        }
    }
}


void* applyCommands(){
    while(numberCommands > 0){
        const char* command = removeCommand(); 
        if (command == NULL) {
            continue;
        } 
        char token;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s", &token, name);
        if (numTokens != 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }
        int searchResult;
        int iNumber;
        switch (token) {
            // These locks/unlocks will only work if the correct compilation option is defined
            case 'c':
                iNumber = obtainNewInumber(fs);
                MUTEX_TREE_LOCK(treeLock); 
                RWLOCK_WRLOCK(treeRWLock);
                ASSERT_CHECK;
                create(fs, name, iNumber);
                MUTEX_TREE_UNLOCK(treeLock);
                RWLOCK_UNLOCK(treeRWLock);
                ASSERT_CHECK;
                break;
            case 'l':
                MUTEX_COMMAND_UNLOCK(commandLock);
                ASSERT_CHECK;
                MUTEX_TREE_LOCK(treeLock);
                RWLOCK_RDLOCK(treeRWLock);
                ASSERT_CHECK;
                searchResult = lookup(fs, name);
                MUTEX_TREE_UNLOCK(treeLock);
                RWLOCK_UNLOCK(treeRWLock);
                ASSERT_CHECK;
                if(!searchResult)
                    fprintf(stderr,"%s not found\n", name);
                else
                    fprintf(stderr,"%s found with inumber %d\n", name, searchResult);
                break;
            case 'd':
                MUTEX_COMMAND_UNLOCK(commandLock);
                ASSERT_CHECK;
                MUTEX_TREE_LOCK(treeLock);
                RWLOCK_WRLOCK(treeRWLock);
                ASSERT_CHECK;
                delete(fs, name);
                MUTEX_TREE_UNLOCK(treeLock);
                RWLOCK_UNLOCK(treeRWLock);
                ASSERT_CHECK;
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    if (compileOption != -1) {
        pthread_exit(NULL);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    int i = 0, err;
    struct timeval start, end; // gettimeofday struct variables
    
    parseArgs(argc, argv); // Reads the 3 arguments (Input,Output,Threads) from file
    fs = new_tecnicofs();
    
    // File opening 
    input = fopen(argv[1],"r");
    output = fopen(argv[2],"w");
    if (input == NULL) {
        perror(argv[1]);    
        exit(1);
    }
    processInput(); 
    fclose(input);

    // Thread handling
    if (compileOption == -1) {
        gettimeofday(&start, NULL); 
        applyCommands();
    } else {
        if (atoi(argv[3]) == 0) {
            fprintf(stderr, "Error: Please use a thread number higher than 0 for this specific compiling option");
            exit(EXIT_FAILURE);
        }
        pthread_t tid[atoi(argv[3])]; // Thread declaration according to compilation arguments received

        if (pthread_mutex_init(&commandLock, NULL) != 0) {
            fprintf(stderr, "Error: Couldn't initialize mutex");
            exit(EXIT_FAILURE);
        }
        if (compileOption == 0) {
            if (pthread_mutex_init(&treeLock, NULL) != 0) {
                fprintf(stderr, "Error: Couldn't initialize mutex");
                exit(EXIT_FAILURE);
            } 
        } else {
            if (pthread_rwlock_init(&treeRWLock, NULL) != 0) {
                fprintf(stderr, "Error: Couldn't initialize rwlock");
                exit(EXIT_FAILURE);
            }
        }
        
        gettimeofday(&start, NULL); // Starting clock as soon as the thread creation begins
        while (i < atoi(argv[3])) {
            err = pthread_create(&(tid[i]), NULL, applyCommands, NULL);
            if (err != 0) {
                fprintf(stderr, "Error: Couldn't create thread");
                exit(EXIT_FAILURE);
            }
            i++;
        }   
        for (i = 0; i < atoi(argv[3]); i++) {
            err = pthread_join(tid[i], NULL);
            if (err != 0) {
                fprintf(stderr, "Error: Couldn't join thread");
                exit(EXIT_FAILURE);
            }
        }
        if (pthread_mutex_destroy(&commandLock) != 0) {
            fprintf(stderr, "Error: Couldn't destroy mutex");
            exit(EXIT_FAILURE);
        } // Destroying the lock that takes care of the command vector and the iNumber obtainment
        (compileOption == 0) ? (err = pthread_mutex_destroy(&treeLock)) : (err = pthread_rwlock_destroy(&treeRWLock)); // Destroying the correct lock according to the compiling option
        if (err != 0) {
            fprintf(stderr, "Error: Couldn't destroy chosen compiling option");
            exit(EXIT_FAILURE);
        }
    }

    // Output handling
    print_tecnicofs_tree(output, fs);
    fclose(output);
    free_tecnicofs(fs);

    // Time handling
    gettimeofday(&end, NULL); // Finish clock and print time with four decimal places.
    printf("TecnicoFS completed in %.4f seconds.\n", ((double)(end.tv_usec - start.tv_usec) / 100000) + (double)(end.tv_sec - start.tv_sec));
    
    exit(EXIT_SUCCESS);
}
