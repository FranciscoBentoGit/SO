#include "fs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "lib/hash.h"

#define ASSERT_CHECK assert(operationStatus == 0) // Verifies that a specific operation executes succesfully 
extern int operationStatus; // Global variable intended for assert operations

#ifdef MUTEX
    #define MUTEX_TREE_LOCK(treeLock) operationStatus = pthread_mutex_lock(treeLock)
    #define MUTEX_TREE_UNLOCK(treeLock) operationStatus = pthread_mutex_unlock(treeLock)
    #define MUTEX_TREE_INIT(treeLock) pthread_mutex_init(treeLock, NULL)
	#define MUTEX_TREE_DESTROY(treeLock) pthread_mutex_destroy(treeLock)
	#define MUTEX_TREE_TRYLOCK(treeLock) pthread_mutex_trylock(treeLock)
	
	#define RWLOCK_WRTRYLOCK(treeLock)
	#define RWLOCK_TREE_INIT(treeLock)
	#define RWLOCK_RDLOCK(treeLock)
    #define RWLOCK_WRLOCK(treeLock)
    #define RWLOCK_UNLOCK(treeLock)
	#define RWLOCK_TREE_DESTROY(treeLock)
#elif RWLOCK
	#define RWLOCK_RDLOCK(treeLock) operationStatus = pthread_rwlock_rdlock(treeLock)
    #define RWLOCK_WRLOCK(treeLock) operationStatus = pthread_rwlock_wrlock(treeLock)
    #define RWLOCK_UNLOCK(treeLock) operationStatus = pthread_rwlock_unlock(treeLock)
	#define RWLOCK_TREE_INIT(treeLock) pthread_rwlock_init(treeLock, NULL)
	#define RWLOCK_TREE_DESTROY(treeLock) pthread_rwlock_destroy(treeLock)
	#define RWLOCK_WRTRYLOCK(treeLock) pthread_rwlock_trywrlock(treeLock)
	
	#define MUTEX_TREE_TRYLOCK(treeLock)
	#define MUTEX_TREE_INIT(treeLock)
    #define MUTEX_TREE_LOCK(treeLock)
    #define MUTEX_TREE_UNLOCK(treeLock)
	#define MUTEX_TREE_DESTROY(treeLock)
#else 
    #define RWLOCK_WRTRYLOCK(treeLock)
	#define RWLOCK_TREE_INIT(treeLock)
	#define RWLOCK_RDLOCK(treeLock)
    #define RWLOCK_WRLOCK(treeLock)
    #define RWLOCK_UNLOCK(treeLock)
	#define RWLOCK_TREE_DESTROY(treeLock)
	#define MUTEX_TREE_TRYLOCK(treeLock)
	#define MUTEX_TREE_INIT(treeLock)
    #define MUTEX_TREE_LOCK(treeLock)
    #define MUTEX_TREE_UNLOCK(treeLock)
	#define MUTEX_TREE_DESTROY(treeLock)
#endif

int obtainNewInumber(tecnicofs* fs) {
	int newInumber = ++(fs->nextINumber); 
	return newInumber;
}

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
	#if defined MUTEX || defined RWLOCK
		fs->treeLock = malloc(numberBuckets * sizeof(tree_lock_t));
		if (!(fs->treeLock)) {
			perror("Failed to allocate treeLock");
		}
	#endif
	for (int i = 0; i < numberBuckets; i++) {
		*(fs->bstRoot + i) = NULL;
		#ifdef MUTEX
			if (MUTEX_TREE_INIT(fs->treeLock + i) != 0) {
                fprintf(stderr, "Error: Couldn't initialize mutex\n");
                exit(EXIT_FAILURE);
            	} 
		#elif RWLOCK
			if (RWLOCK_TREE_INIT(fs->treeLock + i) != 0) {
                fprintf(stderr, "Error: Couldn't initialize mutex\n");
                exit(EXIT_FAILURE);
            	} 
		#endif
	}	
	return fs;
}

void free_tecnicofs(tecnicofs* fs){
	for (int i = 0; i < numberBuckets; i++) {
		free_tree(*(fs->bstRoot + i));
		#ifdef MUTEX
			if (MUTEX_TREE_DESTROY(fs->treeLock + i) != 0) {
                fprintf(stderr, "Error: Couldn't destroy mutex\n");
                exit(EXIT_FAILURE);
            } 
		#elif RWLOCK
			if (RWLOCK_TREE_DESTROY(fs->treeLock + i) != 0) {
                fprintf(stderr, "Error: Couldn't destroy mutex\n");
                exit(EXIT_FAILURE);
            } 
		#endif
	}
	#if defined MUTEX || defined RWLOCK 
		free(fs->treeLock);
	#endif
	free(fs->bstRoot);
	free(fs);
}

void create(tecnicofs* fs, char *name, int inumber, int bucketIndex){
	MUTEX_TREE_LOCK(fs->treeLock + bucketIndex);
	RWLOCK_WRLOCK(fs->treeLock + bucketIndex); 
	ASSERT_CHECK;
	*(fs->bstRoot + bucketIndex) = insert(*(fs->bstRoot + bucketIndex), name, inumber);
	MUTEX_TREE_UNLOCK(fs->treeLock + bucketIndex);
	RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
}

void delete(tecnicofs* fs, char *name, int bucketIndex){
	MUTEX_TREE_LOCK(fs->treeLock + bucketIndex);
    RWLOCK_WRLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
	*(fs->bstRoot + bucketIndex) = remove_item(*(fs->bstRoot + bucketIndex), name);
	MUTEX_TREE_UNLOCK(fs->treeLock + bucketIndex);
    RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
}

int lookup(tecnicofs* fs, char *name, int bucketIndex){
	MUTEX_TREE_LOCK(fs->treeLock + bucketIndex);
    RWLOCK_RDLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
	node* searchNode = search(*(fs->bstRoot + bucketIndex), name);
	MUTEX_TREE_UNLOCK(fs->treeLock + bucketIndex);
    RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
	ASSERT_CHECK;
	if ( searchNode ) return searchNode->inumber;
	return 0;
}

void renameNode(tecnicofs* fs, char* name, char* rename, int bucketIndex) { 
	int searchResult = lookup(fs, name, bucketIndex);
	int iNumberSaver = searchResult;
	if (!searchResult)
		return;
	else {
		int newBucketIndex = hash(rename, numberBuckets); 
		searchResult = lookup(fs, rename, newBucketIndex);
		if (searchResult) {
			return;
		}
		#ifdef MUTEX
			int sleepTime, retVal, tryNumber = 0, renameConditions = 0;
			sleepTime = ((rand() % 1000)) + 1; 
			while (renameConditions != 1) {
				tryNumber++;
				retVal = MUTEX_TREE_TRYLOCK(fs->treeLock + bucketIndex);
				if (retVal == 0) {
					retVal = MUTEX_TREE_TRYLOCK(fs->treeLock + newBucketIndex);
					if ((retVal == 0) || bucketIndex == newBucketIndex) {
						renameConditions = 1;
					} else if (retVal != EBUSY) {
						fprintf(stderr,"Error: Trylock error not related to busy lock");
						exit(EXIT_FAILURE);
					} else {
						MUTEX_TREE_UNLOCK(fs->treeLock + bucketIndex);
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
		#elif RWLOCK 
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
		#endif
		*(fs->bstRoot + bucketIndex) = remove_item(*(fs->bstRoot + bucketIndex), name);
		*(fs->bstRoot + newBucketIndex) = insert(*(fs->bstRoot + newBucketIndex), rename, iNumberSaver);
		MUTEX_TREE_UNLOCK(fs->treeLock + bucketIndex);
		RWLOCK_UNLOCK(fs->treeLock + bucketIndex);
		ASSERT_CHECK;
		if (bucketIndex != newBucketIndex) {
			MUTEX_TREE_UNLOCK(fs->treeLock + newBucketIndex);
			RWLOCK_UNLOCK(fs->treeLock + newBucketIndex);
			ASSERT_CHECK;
		}
		return;
	}
}

void print_tecnicofs_tree(FILE * fp, tecnicofs *fs){
	for (int i = 0; i < numberBuckets; i++) {
		print_tree(fp, fs->bstRoot[i]);
	}
}