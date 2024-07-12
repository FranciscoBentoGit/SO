#include "tecnicofs-client-api.h"
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

#define MAX_INPUT_SIZE 100

int client_fd;

char return_message[100];

int tfsMount(char* sun_path) {
    
    struct sockaddr_un server_sockaddr;
    char full_path[MAX_INPUT_SIZE];
    memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
    if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == 0) {
        return TECNICOFS_ERROR_OTHER;
    }
    strncpy(full_path, sun_path, MAX_INPUT_SIZE);
    server_sockaddr.sun_family = AF_UNIX;
    strncpy(server_sockaddr.sun_path, full_path, MAX_INPUT_SIZE);
    if (connect(client_fd, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)) < 0) {
        return TECNICOFS_ERROR_CONNECTION_ERROR;
    } 
    return 0;
}

int tfsCreate(char *filename, permission ownerPermissions, permission othersPermissions) {
    char command[MAX_INPUT_SIZE];

    if(!filename[0] || (ownerPermissions > 3 || ownerPermissions < 0) || (othersPermissions > 3 || othersPermissions < 0)) {
        return TECNICOFS_ERROR_OTHER;
    }

    snprintf(command, MAX_INPUT_SIZE, "c %s %d%d", filename, ownerPermissions, othersPermissions);
   
    if (send(client_fd, command, strlen(command), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    memset(return_message, 0, sizeof(return_message));
    if (recv(client_fd, return_message, sizeof(return_message), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    
    return atoi(return_message);
}

int tfsDelete(char *filename) {
    char command[MAX_INPUT_SIZE]; 

    if (!filename[0]) {
        return TECNICOFS_ERROR_OTHER;
    }
 
    snprintf(command, MAX_INPUT_SIZE, "d %s", filename);
    
    if (send(client_fd, command, strlen(command), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    memset(return_message, 0, sizeof(return_message));
    if (recv(client_fd, return_message, sizeof(return_message), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }

    return atoi(return_message);
}

int tfsRename(char *filenameOld, char *filenameNew) {
    char command[MAX_INPUT_SIZE];

    if (!filenameOld[0] || !filenameNew[0]) {
        return TECNICOFS_ERROR_OTHER;
    }

    snprintf(command, MAX_INPUT_SIZE, "r %s %s", filenameOld, filenameNew);

    if (send(client_fd, command, strlen(command), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    memset(return_message, 0, sizeof(return_message));
    if (recv(client_fd, return_message, sizeof(return_message), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }

    return atoi(return_message);
}

int tfsOpen(char *filename, permission mode) {
    char command[MAX_INPUT_SIZE];

    if (!filename[0] || (mode > 3 || mode < 0)) {
        return TECNICOFS_ERROR_OTHER;
    }

    snprintf(command, MAX_INPUT_SIZE, "o %s %d", filename, mode);
    
    if (send(client_fd, command, strlen(command), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    memset(return_message, 0, sizeof(return_message));
    if (recv(client_fd, return_message, sizeof(return_message), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }

    return atoi(return_message);
}

int tfsClose(int fd) {
    char command[MAX_INPUT_SIZE];

    if (fd < 0 || fd > 4) {
        return TECNICOFS_ERROR_OTHER;
    }

    snprintf(command, MAX_INPUT_SIZE, "x %d", fd);

    if (send(client_fd, command, strlen(command), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    memset(return_message, 0, sizeof(return_message));
    if (recv(client_fd, return_message, 4, 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    
    return atoi(return_message);
}

int tfsRead(int fd, char *buffer, int len) {
    char command[MAX_INPUT_SIZE]; 
    
    if ((fd < 0 || fd > 4) || len <= 0) {
        return TECNICOFS_ERROR_OTHER;
    }
    
    snprintf(command, MAX_INPUT_SIZE, "l %d %d", fd, len);
    
    if (send(client_fd, command, strlen(command), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    
    memset(return_message, 0, sizeof(return_message));
    
    if (recv(client_fd, return_message, sizeof(return_message), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }

    if (atoi(return_message) < 0) {
        return atoi(return_message);
    }
    
    strncpy(buffer, return_message, len-1);
    buffer[len-1] = '\0';
    return strlen(buffer);
}

int tfsWrite(int fd, char *buffer, int len) {
    char command[5 + len];

    if ((fd < 0 || fd > 4) || !buffer[0] || len <= 0) {
        return TECNICOFS_ERROR_OTHER;
    }

    snprintf(command, MAX_INPUT_SIZE, "w %d %s", fd, buffer);

    if (send(client_fd, command, strlen(command), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    memset(return_message, 0, sizeof(return_message));
    if (recv(client_fd, return_message, sizeof(return_message), 0) < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }

    return atoi(return_message);
}

int tfsUnmount() {
    char term_msg[2];
    strncpy(term_msg, "f", 2);
    if (send(client_fd, term_msg, strlen(term_msg), 0)  < 0) {
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    } else if (close(client_fd) == 0) {
        return 0;
    } else {
        return TECNICOFS_ERROR_OTHER;
    }
}