#include "fs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "lib/hash.h"
#include "lib/inodes.h"

#define ASSERT_CHECK assert(operationStatus == 0) // Verifies that a specific operation executes succesfully 
extern int operationStatus; // Global variable intended for assert operations

#define RWLOCK_RDLOCK(treeLock) operationStatus = pthread_rwlock_rdlock(treeLock)
#define RWLOCK_WRLOCK(treeLock) operationStatus = pthread_rwlock_wrlock(treeLock)
#define RWLOCK_UNLOCK(treeLock) operationStatus = pthread_rwlock_unlock(treeLock)
#define RWLOCK_TREE_INIT(treeLock) pthread_rwlock_init(treeLock, NULL)
#define RWLOCK_TREE_DESTROY(treeLock) pthread_rwlock_destroy(treeLock)
#define RWLOCK_WRTRYLOCK(treeLock) pthread_rwlock_trywrlock(treeLock)

tecnicofs* new_tecnicofs(){
	tecnicofs*fs = malloc(sizeof(tecnicofs));
	if (!fs) {
		perror("Failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}
	fs->nextINumber = 0;
	fs->bstRoot = malloc(numberBuckets * sizeof(node*));
	if (!(fs->bstRoot)) {
		perror("Failed to allocate btRoot");
	}
	fs->treeLock = malloc(numberBuckets * sizeof(tree_lock_t));
	if (!(fs->treeLock)) {
		perror("Failed to allocate treeLock");
	}
	for (int i = 0; i < numberBuckets; i++) {
		*(fs->bstRoot + i) = NULL;
		if (RWLOCK_TREE_INIT(fs->treeLock + i) != 0) {
			fprintf(stderr, "Error: Couldn't initialize mutex\n");
			exit(EXIT_FAILURE);
		} 
	}	
	return fs;
}

void free_tecnicofs(tecnicofs* fs){
	for (int i = 0; i < numberBuckets; i++) {
		free_tree(*(fs->bstRoot + i));
		if (RWLOCK_TREE_DESTROY(fs->treeLock + i) != 0) {
			fprintf(stderr, "Error: Couldn't destroy mutex\n");
			exit(EXIT_FAILURE);
		} 
	}
	free(fs->treeLock);
	free(fs->bstRoot);
	free(fs);
}

void create(tecnicofs* fs, char *name, int inumber, int bucketIndex){
	RWLOCK_WRLOCK(fs->treeLock + bucketIndex); 
	ASSERT_CHECK;
	*(fs->bstRoot + bucketIndex) = insert(*(fs->bstRoot + bucketIndex), name, inumber);
	RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
}

void delete(tecnicofs* fs, char *name, int bucketIndex){
    RWLOCK_WRLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
	*(fs->bstRoot + bucketIndex) = remove_item(*(fs->bstRoot + bucketIndex), name);
    RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
}

int lookup(tecnicofs* fs, char *name, int bucketIndex){
    RWLOCK_RDLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
	node* searchNode = search(*(fs->bstRoot + bucketIndex), name);
    RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
	if ( searchNode ) return searchNode->inumber;
	return -1;
}

int renameNode(tecnicofs* fs, char* name, char* rename, int bucketIndex) { 
	int searchResult = lookup(fs, name, bucketIndex);
	int iNumberSaver = searchResult;
	if (searchResult == -1)
		return -4;
	else {
		int newBucketIndex = hash(rename, numberBuckets); 
		searchResult = lookup(fs, rename, newBucketIndex);
		if (searchResult != -1) {
			return -5;
		}
		int sleepTime, retVal, tryNumber = 0, renameConditions = 0;
		srand(time(NULL));
		sleepTime = ((rand() % 1000)) + 1;
		while (renameConditions != 1) {
			tryNumber++;
			retVal = RWLOCK_WRTRYLOCK(fs->treeLock + bucketIndex);
			if (retVal == 0) {
				retVal = RWLOCK_WRTRYLOCK(fs->treeLock + newBucketIndex);
				if ((retVal == 0) || bucketIndex == newBucketIndex) {
					renameConditions = 1;
				} else if (retVal != EBUSY) {
					fprintf(stderr,"Error: Trylock error not related to busy lock");
					exit(EXIT_FAILURE);
				} else {
					RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
					ASSERT_CHECK;
				}
			} else if (retVal != EBUSY) { // Exit if the error is not associated with the thread being already busy
				fprintf(stderr,"Error: Trylock error not related to busy lock");
				exit(EXIT_FAILURE);
			} 
			if (renameConditions == 0) {
				if (usleep((sleepTime * tryNumber) * 1000) != 0) { // Sleep a random time between 0 and 1 sec times the number of tries
					fprintf(stderr,"Error: Failure on usleep function.");
					exit(EXIT_FAILURE);
				}	
			}
		}
		*(fs->bstRoot + bucketIndex) = remove_item(*(fs->bstRoot + bucketIndex), name);
		*(fs->bstRoot + newBucketIndex) = insert(*(fs->bstRoot + newBucketIndex), rename, iNumberSaver);
		RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
		ASSERT_CHECK;
		if (bucketIndex != newBucketIndex) {
			RWLOCK_UNLOCK(fs->treeLock + newBucketIndex);
			ASSERT_CHECK;
		}
		return 0;
	}
}

void print_tecnicofs_tree(FILE * fp, tecnicofs *fs){
	for (int i = 0; i < numberBuckets; i++) {
		print_tree(fp, fs->bstRoot[i]);
	}
}