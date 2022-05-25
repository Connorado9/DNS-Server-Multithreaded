// Connor Humiston
// Workers Header
#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include <stdlib.h>                                 // standard vars, macros & functions
#include <stdio.h>                                  // standard i/o
#include <string.h>                                 // C string library
#include <pthread.h>                                // thread library
#include <semaphore.h>                              // semaphore library
#include <sys/time.h>                               // for gettimeofday()
#include <unistd.h>                                 // POSIX API
#include <assert.h>                                 // used for assert macro

#include "util.h"                                   // DNS lookup

#define ARRAY_SIZE              10                  // number of elements in shared array
#define MAX_INPUT_FILES         100                 // maximum hostname file arguments
#define MAX_REQUESTER_THREADS   10                  // max concurrent requestors
#define MAX_RESOLVER_THREADS    10                  // max concurrent resolvers
#define MAX_NAME_LENGTH         255                 // max size of hostname w/ null terminator
#define MAX_IP_LENGTH           INET6_ADDRSTRLEN    // max size IP address util.c will return


/* File Data Structure */
typedef struct File
{
    FILE* fp;                                       // input file pointer
    char* name;                                     // file name
    pthread_mutex_t flock;                          // input file lock
    int serviced;                                   // indicates if file has been fully serviced
} File;

/* Structure organizing list of input files */
typedef struct FileList 
{
    pthread_mutex_t listlock;                       // protects access to list of file
    int tot;                                        // total number of files in the list
    int curr;                                       // current file being serviced
    int num_serviced;                               // number of files serviced so far across all threads
    File files[];                                   // array of files
} FileList;

/* Shared Buffer Array */
typedef struct Buffer 
{
    int size;                                       // total buffer size
    int curr;                                       // current position in the array
    pthread_mutex_t bufflock;                       // buffer mutex
    pthread_cond_t bfull;                           // condition for full buffer array, tell other threads
    pthread_cond_t bempty;                          // condition for empty buffer, tell other threads
    int full;                                       // indicator for full buffer
    int empty;                                      // indicator for empty buffer
    int reqsdone;                                   // indicates when the requester threads are finished
    char* arr[];                                    // shared buffer array
} Buffer;

/* Requester Data Arguments */
struct Req_Packet
{
    FileList* files;                                // pointer to the files list struct
    Buffer* buff;                                   // pointer to the shared buffer
    File reqfile;                                   // requester file & parameters
};

/* Resolver Data Arguments */
struct Res_Packet
{
    Buffer* buff;                                   // pointer to the shared buffer
    File resfile;                                   // resolver file & parameters
};


/* Handles input arguments, organizes producers & consumers, and cleans up */
int main(int arg, char* argv[]);

/* Producer: reads hostnames from file and pushes to the queue & requester log, returns # files serviced */
void* requester(void* packet); 

/* Recursive function that services input files until there are none left to service */
void* requester_helper(void* packet, int* serviced);

/* Consumer: resolves hostnames from queue and writes to resolver log, returns # hostnames resolved */
void* resolver(void* packet);

#endif