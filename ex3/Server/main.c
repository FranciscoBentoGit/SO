#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/un.h>
#include <signal.h>
#include "fs.h"  
#include "lib/hash.h" 
#include "lib/inodes.h"

#define MAX_INPUT_SIZE 100
#define FILE_TABLE_SIZE 5

struct ucred ucred;

extern int numberBuckets;

// Struct and file variables
tecnicofs* fs;
FILE *output;

// Thread and semaphore variables
pthread_mutex_t condLock; // For the command vector access and iNumber obtainment
pthread_cond_t cond;

struct timeval start, end; // gettimeofday struct variables

//Command variables
char socketname[MAX_INPUT_SIZE];
char outputFile[MAX_INPUT_SIZE];
int acceptedClients = 0;
int activeClients = 0;
int tecnicofs_fd;

pthread_t tid[5];
sigset_t sig_set;

static void displayUsage (const char* appname){
    printf("Usage: %s\n", appname);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc == 4) {
        strncpy(socketname, argv[1], MAX_INPUT_SIZE);
        strncpy(outputFile, argv[2], MAX_INPUT_SIZE);
        numberBuckets = atoi(argv[3]);
    } else {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

int responseClient(int sock, char* responseMessage, char* responseValue) {
    int connectionCheck;
    strncpy(responseMessage, responseValue, MAX_INPUT_SIZE);
    if ((connectionCheck = send(sock, responseMessage, strlen(responseMessage), 0)) < 0) {
        fprintf(stderr, "Error: Client connection failure.\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

void* applyCommands(void* sockfd){  

    if (pthread_sigmask(SIG_UNBLOCK, &sig_set, NULL) != 0) {
        fprintf(stderr, "Error: Sigmask failed.\n");
    }

    typedef struct open_table_t {
        permission file_perm;
        int file_inumber;
    }open_table;

    open_table file_table[5];

    int sock = *(int*)sockfd;

    uid_t owner;
    permission ownerPerms;
    permission otherPerms;
    char fileContents[MAX_INPUT_SIZE];
    
    char token, client_message[MAX_INPUT_SIZE], return_message[MAX_INPUT_SIZE];
    char arg1[MAX_INPUT_SIZE], arg2[MAX_INPUT_SIZE];
 
    int iNumber, fileInTable, readSize;

    for (int i = 0; i < 5; i++) {
        file_table[i].file_perm = NONE;
        file_table[i].file_inumber = -1;
    }
    while(1){
        memset(client_message, 0, sizeof(client_message));
        if ((readSize = recv(sock, client_message, sizeof(client_message)-1, 0)) < 0) { // -1 for \0 in case buffer fills
            fprintf(stderr, "Error: Client connection failure.\n");
            exit(EXIT_FAILURE);
        } else if (strncmp(client_message, "f", 2) == 0) {
            break;
        }
        client_message[readSize] = '\0';
        sscanf(client_message, "%c %s %s", &token, arg1, arg2); 
        int bucketIndex = hash(arg1, numberBuckets);
        int freeIndex = -1;
        switch (token) {
            case 'c':

                iNumber = lookup(fs, arg1, bucketIndex);
                
                if (iNumber != -1) {
                    responseClient(sock, return_message, "-4");
                    break;
                }

                if ((iNumber =  inode_create(ucred.uid, arg2[0], arg2[1])) == -1) {
                    responseClient(sock, return_message, "-11");
                    break;
                }
                
                create(fs, arg1, iNumber, bucketIndex);
                responseClient(sock, return_message, "0");
                
                break;

            case 'd': {
                iNumber = lookup(fs, arg1, bucketIndex);

                if (iNumber == -1) {
                    responseClient(sock, return_message, "-5");
                    break;
                }

                if (inode_get(iNumber, &owner, &ownerPerms, &otherPerms, fileContents, strlen(fileContents)) == -1) {
                    responseClient(sock, return_message, "-11");
                    break;
                }

                if ((owner != ucred.uid)) {
                    responseClient(sock, return_message, "-6");
                    break;
                }

                if (inode_delete(iNumber) == -1) {
                    responseClient(sock, return_message, "-11");
                    break;
                }
                delete(fs, arg1, bucketIndex);

                responseClient(sock, return_message, "0");

                break;
            }
            case 'r': {

                int errorCheck;
                char errorToString[3];

                iNumber = lookup(fs, arg1, bucketIndex);
                errorCheck = renameNode(fs, arg1, arg2, bucketIndex);

                if (errorCheck != 0) {
                    snprintf(errorToString, 3, "%d", errorCheck);
                    responseClient(sock, return_message, errorToString);
                    break;
                }   

                inode_get(iNumber, &owner, &ownerPerms, &otherPerms, fileContents, strlen(fileContents));
                
                if  (owner != ucred.uid) {
                    responseClient(sock, return_message, "-6");
                    break;
                }

                if (inode_delete(iNumber) == -1) {
                    responseClient(sock, return_message, "-11");
                    break;
                }
                
                if (inode_create(ucred.uid, ownerPerms, otherPerms) == -1) {
                    responseClient(sock, return_message, "-11");
                    break;
                }

                responseClient(sock, return_message, "0");
                
                break;
            }
            case 'o': {

                iNumber = lookup(fs, arg1, bucketIndex);

                if (iNumber == -1) {
                    responseClient(sock, return_message, "-5");
                    break;
                }

                if (inode_get(iNumber, &owner, &ownerPerms, &otherPerms, fileContents, strlen(fileContents)) == -1) {
                    responseClient(sock, return_message, "-11");
                    break;
                }

                fileInTable = 0;

                for (int i = 0; i < 5; i++) {
                    if (file_table[i].file_inumber == iNumber) { 
                        responseClient(sock, return_message, "-9");
                        fileInTable = 1;
                        break; 
                    }
                }
                if (fileInTable == 1) {
                    break;
                }
                for (int j = 0; j < 5; j++) {
                    if (file_table[j].file_inumber == -1) { 
                        freeIndex = j;
                        break;
                    }
                }
                if (freeIndex == -1) {
                    responseClient(sock, return_message, "-7");
                    break;
                }
                if (owner == ucred.uid) {
                    if (atoi(arg2) > ownerPerms || (atoi(arg2) == WRITE && ownerPerms == READ)) {
                            responseClient(sock, return_message, "-6");
                            break;
                        } 
                } else if (atoi(arg2) > otherPerms || (atoi(arg2) == WRITE && otherPerms == READ)) {
                    responseClient(sock, return_message, "-6");
                    break;
                }
                

                file_table[freeIndex].file_perm = atoi(arg2);
                file_table[freeIndex].file_inumber = iNumber;
                
                char fdReturn[2];
                snprintf(fdReturn, 2, "%d", freeIndex);

                responseClient(sock, return_message, fdReturn);

                break;
            }
            case 'x': {

                if (file_table[atoi(arg1)].file_inumber == -1) {
                    responseClient(sock, return_message, "-8");
                    break;
                }
                if (atoi(arg1) > 4) {
                    responseClient(sock, return_message, "-11");
                    break;
                }

                file_table[atoi(arg1)].file_inumber = -1;
                file_table[atoi(arg1)].file_perm = NONE;

                responseClient(sock, return_message, "0");

                break;
            }
            case 'l': {
                if (file_table[atoi(arg1)].file_inumber == -1) {
                    responseClient(sock, return_message, "-8");
                    break;
                }
                if (file_table[atoi(arg1)].file_perm < 2) {
                    responseClient(sock, return_message, "-10");
                    break;
                }
                if (inode_get(file_table[atoi(arg1)].file_inumber, &owner, &ownerPerms, &otherPerms, fileContents, atoi(arg2)) == -1) {
                    responseClient(sock, return_message, "-11");
                    break;
                }
                
                responseClient(sock, return_message, fileContents);

                break;
            }
            case 'w': {

                char buff[MAX_INPUT_SIZE];
                int nullTerminatorIndex;

                for (int i = 0, j = 4; i < strlen(client_message) - 4; i++, j++) { // Getting the client message without the "w %d " part
                    buff[i] = client_message[j];
                    nullTerminatorIndex = i + 1;
                }

                buff[nullTerminatorIndex] = '\0';
                fileInTable = 0;

                if (file_table[atoi(arg1)].file_inumber != -1) {
                    fileInTable = 1;
                }
                
                if (fileInTable == 0) {
                    responseClient(sock, return_message, "-8");
                    break;
                }
                if ((file_table[atoi(arg1)].file_perm != 1) && (file_table[atoi(arg1)].file_perm != 3)) {
                    responseClient(sock, return_message, "-6");
                    break;
                }

                if (inode_set(file_table[atoi(arg1)].file_inumber, buff, strlen(buff)) == -1) {
                    responseClient(sock, return_message, "-11");
                    break;
                }

                responseClient(sock, return_message, "0");

                break;
            }
            default: { 
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    } 
    for (int i = 0; i < 5; i++) { // Releasing the client's file table.
        file_table[i].file_perm = NONE;
        file_table[i].file_inumber = -1;
    }

    if (close(*(int*)sockfd) != 0) {
        fprintf(stderr, "Error: Close failed.\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_lock(&condLock) != 0) {
        fprintf(stderr, "Error: mutex\n");
        exit(EXIT_FAILURE);
    }

    activeClients--;
    if (pthread_cond_signal(&cond) != 0) {
        fprintf(stderr, "Error: Cond signal failed.\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_unlock(&condLock) != 0) {
        fprintf(stderr, "Error: mutex\n");
        exit(EXIT_FAILURE);
    }
    return NULL;
}

void termination(int term) {
    if (signal(SIGINT, termination) == SIG_ERR) {
        fprintf(stderr, "Error: Signal failed.\n");
        exit(EXIT_FAILURE);
    }

    int err;

    unlink(socketname);

    if (pthread_mutex_lock(&condLock) != 0) {
        fprintf(stderr, "Error: Mutex lock failed\n");
        exit(EXIT_FAILURE);
    }

    while (activeClients != 0) {
        if (pthread_cond_wait(&cond, &condLock) != 0) {
            fprintf(stderr, "Error: Cond wait faileed.\n");
        }
    }

    if (pthread_mutex_unlock(&condLock) != 0) {
        fprintf(stderr, "Error: Mutex unlock failed\n");
        exit(EXIT_FAILURE);
    }

    if (close(tecnicofs_fd) != 0) {
        fprintf(stderr, "Error: Close failed.\n");
        exit(EXIT_FAILURE);
    }

    print_tecnicofs_tree(output, fs);

    if (pthread_mutex_destroy(&condLock) != 0) {
        fprintf(stderr, "Error: Mutex destroy failed.\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_destroy(&cond) != 0) {
        fprintf(stderr, "Error: Cond destroy failed.\n");
        exit(EXIT_FAILURE);
    }

    // Time handling
    if ((err = gettimeofday(&end, NULL) != 0)) { // Finish clock and print time with four decimal places.
        fprintf(stderr, "Error: Couldn't gettimeofday\n");
        exit(EXIT_FAILURE);
    } 
    printf("\nTecnicoFS completed in %.4f seconds.\n", (((double)(end.tv_sec) + (double)(end.tv_usec / 1000000.0)) - ((double)(start.tv_sec) + (double)(start.tv_usec / 1000000.0))));

    if (fclose(output) != 0) {
        fprintf(stderr, "Error: Close failed.\n");
        exit(EXIT_FAILURE);
    }

    inode_table_destroy();

    free_tecnicofs(fs);

    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {

    int err;
    srand(time(NULL)); // Seeding the random number generator used by the function rand
    
    parseArgs(argc, argv); 
    if (numberBuckets <= 0) {
        fprintf(stderr, "Error: Please use atleast one bucket.\n");
        exit(EXIT_FAILURE);
    }
    fs = new_tecnicofs();
    inode_table_init();

    // File opening 
    output = fopen(outputFile,"w");
    if (output == NULL) {
        perror(outputFile);
        exit(EXIT_FAILURE);
    }

    // Thread handling

    if (pthread_mutex_init(&condLock, NULL) != 0) {
        fprintf(stderr, "Error: Mutex initialization failed.\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&cond, NULL) != 0) {
        fprintf(stderr, "Error: Cond initialization failed.\n");
        exit(EXIT_FAILURE);
    }

    // Signal handling

    if (sigaddset(&sig_set, SIGINT) != 0) {
        fprintf(stderr, "Error: Sigaddset failed\n");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGINT, termination) == SIG_ERR) {
        fprintf(stderr, "Error: Signal failed.\n");
        exit(EXIT_FAILURE);
    }

    // Socket handling

    struct sockaddr_un server_sockaddr;
    memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));

    int connectSocket_fd; 
    if ((tecnicofs_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == 0) {
        fprintf(stderr, "Error: Socket failure.\n");
        exit(EXIT_FAILURE);
    }

    unlink(socketname);

    server_sockaddr.sun_family = AF_UNIX;
    strncpy(server_sockaddr.sun_path, socketname, MAX_INPUT_SIZE);
    if (bind(tecnicofs_fd, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)) < 0) {
        fprintf(stderr, "Error: Bind failed.\n");
        exit(EXIT_FAILURE);
    }

    if (listen(tecnicofs_fd, 5) < 0) {
            fprintf(stderr, "Error: Listen failure.\n");
            exit(EXIT_FAILURE);
        }
    
    if ((err = gettimeofday(&start, NULL) != 0)) { 
        fprintf(stderr, "Error: Couldn't gettimeofday\n"); 
        exit(EXIT_FAILURE);
    } 

    while (1) {
        socklen_t addr_size = sizeof(server_sockaddr);
        if ((connectSocket_fd = accept(tecnicofs_fd, (struct sockaddr*)&server_sockaddr, &addr_size)) < 0) {
            fprintf(stderr, "Error: Connection failure.\n");
            exit(EXIT_FAILURE);
        } else {
            acceptedClients++;
            if (pthread_mutex_lock(&condLock) != 0) {
                fprintf(stderr, "Error: Mutex lock failed.\n");
                exit(EXIT_FAILURE);
            }
            activeClients++;
            if (pthread_mutex_unlock(&condLock) != 0) {
                fprintf(stderr, "Error: Mutex unlock failed\n");
                exit(EXIT_FAILURE);
            }
            socklen_t len = sizeof(struct ucred);
            if (getsockopt(connectSocket_fd,  SOL_SOCKET, SO_PEERCRED, &ucred, &len) == -1) {
                fprintf(stderr, "Error: Sockopt failed.");
                exit(EXIT_FAILURE);
            }
            if (pthread_create(&tid[acceptedClients], NULL, applyCommands, (void*)&connectSocket_fd) != 0) {
                fprintf(stderr, "Error: Couldn't create thread for client.");
                exit(EXIT_FAILURE);
            } 
        }
    }
}
