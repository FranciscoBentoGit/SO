#ifndef FS_H
#define FS_H
#include "lib/bst.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>

int numberBuckets;
int operationStatus; // Global variable intended for assert operations

#ifdef MUTEX    
    #define tree_lock_t pthread_mutex_t
#else 
    #ifdef RWLOCK
        #define tree_lock_t pthread_rwlock_t
    #endif
#endif

typedef struct tecnicofs {
    node** bstRoot;
    int nextINumber;
    #if defined MUTEX || defined RWLOCK
        tree_lock_t *treeLock;
    #endif
} tecnicofs;

int obtainNewInumber(tecnicofs* fs);
tecnicofs* new_tecnicofs();
void free_tecnicofs(tecnicofs* fs);
void create(tecnicofs* fs, char *name, int inumber, int bucketIndex);
void delete(tecnicofs* fs, char *name, int bucketIndex);
void renameNode(tecnicofs* fs, char* name, char* rename, int bucketIndex);
int lookup(tecnicofs* fs, char *name, int bucketIndex);
void print_tecnicofs_tree(FILE * fp, tecnicofs *fs);

#endif /* FS_H */
