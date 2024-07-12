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


#define tree_lock_t pthread_rwlock_t


typedef struct tecnicofs {
    node** bstRoot;
    int nextINumber;
    tree_lock_t *treeLock;
} tecnicofs;

int obtainNewInumber(tecnicofs* fs);
tecnicofs* new_tecnicofs();
void free_tecnicofs(tecnicofs* fs);
void create(tecnicofs* fs, char *name, int inumber, int bucketIndex);
void delete(tecnicofs* fs, char *name, int bucketIndex);
int renameNode(tecnicofs* fs, char* name, char* rename, int bucketIndex);
int lookup(tecnicofs* fs, char *name, int bucketIndex);
void print_tecnicofs_tree(FILE * fp, tecnicofs *fs);

#endif /* FS_H */
