// Connor Humiston
// Workers Implementation
#include "multi-lookup.h"


/* Handles input arguments, organizes producers & consumers, and cleans up */
int main(int argc, char* argv[])
{
    struct timespec t0,t1;                          // Timekeeping: time at start and end of program
    double elapsed;                                 // time elapsed from program start to finish
    clock_gettime(CLOCK_MONOTONIC, &t0);            // start elapsed time
    int requesters = 0;                             // number of requesters
    int resolvers = 0;                              // number of resolvers
    Buffer* buffer;                                 // declare shared bounded buffer
    FileList* files;                                // list of input files & parameters
    char** fileslist;                               // list of file names
    File reqfile;                                   // requester serviced output file
    File resfile;                                   // resolved output file
    pthread_t* reqID;                               // requester thread IDs array
    pthread_t* resID;                               // resolver thread IDs array
    struct Req_Packet reqpacket;                    // requester function arguments
    struct Res_Packet respacket;                    // resolver function arguments
    int numfiles;                                   // keeps track of total input files
    int r, i;                                       // used to keep track of loops

    // Error Checks
    if(argc < 6)                                    // Missing arguments: usage synopsis & terminate
    {
        fprintf(stderr, "Not enough arguments: %d of minimum 5 arguments given.\n", (argc-1));
        printf("usage: ./multi-lookup num_requestors num_resolvers requestor_log resolver_log [data_file ...]\n");
        exit(EXIT_FAILURE);
    }
    else if(argc > 5 + MAX_INPUT_FILES)             // Arguments out of range (too many): error
    {
        fprintf(stderr, "Too many arguments or input files. Limit input files to 100.\n");
        exit(EXIT_FAILURE);
    }

    // Argument Handling    
    requesters = atoi(argv[1]);                     // Number of Requesters
    if(requesters > MAX_REQUESTER_THREADS || requesters < 0)
    {
        if(resolvers > MAX_RESOLVER_THREADS || resolvers < 0)
            fprintf(stderr, "The number of requester & resolver threads is out of bounds. Choose between 0 and 10 inclusively.\n");
        else
            fprintf(stderr, "The number of requester threads is out of bounds. Choose between 0 and 10 inclusively.\n");
        exit(EXIT_FAILURE);
    }
    resolvers = atoi(argv[2]);                      // Number of Resolvers
    if(resolvers > MAX_RESOLVER_THREADS || resolvers < 0)
    {
        if(requesters > MAX_REQUESTER_THREADS || requesters < 0)
            fprintf(stderr, "The number of requester & resolver threads is out of bounds. Choose between 0 and 10 inclusively.\n");
        else
            fprintf(stderr, "The number of resolver threads is out of bounds. Choose between 0 and 10 inclusively.\n");
        exit(EXIT_FAILURE);
    }
    reqfile.name = argv[3];                         // Requester Log
    reqfile.flock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    reqfile.fp = fopen(argv[3], "w");               // open/create the file & save the file pointer
    if(!reqfile.fp)                                 // if NULL, unable to open or create file
    {
        fprintf(stderr, "Unable to open \"%s\" requestor log.\n", argv[3]);
        exit(EXIT_FAILURE);
    }
    resfile.name = argv[4];                         // Resolver Log
    resfile.flock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    resfile.fp = fopen(argv[4], "w");               // open/create the file & save the file pointer
    if(!resfile.fp)
    {
        fprintf(stderr, "Unable to open \"%s\" resolver log.\n", argv[4]);
        exit(EXIT_FAILURE);
    }

    // Prepare Input Files
    numfiles = 0;
    for(i = 0; i < argc - 5; i++)                   // count number of valid input files
    {                                               // # cl arguments given minus first 5 parameters
        if(access(argv[i+5], F_OK | R_OK) != 0)     // check if the file exists & is readable (fopen willl create the file otherwise)
            fprintf(stderr, "Invalid file: %s.\n", argv[i+5]);
        else
            numfiles++;                             // increment the number of valid files
    }
    fileslist = malloc(sizeof(char*) * numfiles);
    for(i = 0; i < numfiles; i++)
    {
        fileslist[i] = argv[i+5];                   // add the valid file name to the list
    }

    // Build Files List
    files = malloc(sizeof(*files) + sizeof(File) * numfiles); //allocate memory for the file list structure
    files->listlock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER; //initialize default mutex
    files->tot = numfiles;                          // the total number of files in the list
    files->curr = 0;                                // list starts at 0
    files->num_serviced = 0;                        // none serviced yet
    for(i = 0; i < numfiles; i++)
    {
        File file;                                  // declare a file
        file.name = fileslist[i];                   // give file structure a name
        file.fp = fopen(fileslist[i], "r");         // open the file & attach the file pointer
        file.flock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER; //intialize default mutex
        file.serviced = 0;                          // initialize to not serviced yet
        files->files[i] = file;                     // add the newly created file structure to the list of files
    }

    // Initialize Buffer
    buffer = malloc(sizeof(*buffer) + sizeof(char*) * ARRAY_SIZE); //allocate dynamic memory block for buffer
    buffer->size = ARRAY_SIZE;                      // array size of 10
    buffer->curr = 0;                               // start at 0
    buffer->bufflock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    buffer->bfull = (pthread_cond_t) PTHREAD_COND_INITIALIZER; //initialize default condition variable
    buffer->bempty = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    buffer->full = 0;                               // buffer is not full at initialization
    buffer->empty = 1;                              // buffer is empty at initialization
    buffer->reqsdone = 0;                           // requesters not done at init

    // Create & Run Requester Threads
    reqID = malloc(sizeof(pthread_t) * requesters); // allocate space for the requester IDs array
    reqpacket.files = files;                        // pass the requesters the input file list
    reqpacket.buff = buffer;                        // pass the shared buffer
    reqpacket.reqfile = reqfile;                    // pass the output requester serviced file (initialized above)
    for(r = 0; r < requesters; r++)                 // create the requester threads
    {                                               // pass a pointer to the argument structure
        if(pthread_create(&reqID[r], NULL, requester, (void*) &reqpacket) != 0)
        {                                           // check the return value for successful creation 
            fprintf(stderr, "Error creating a requester thread.\n");
            exit(EXIT_FAILURE);
        }
    }

    // Create & Run Resolver Threads
    resID = malloc(sizeof(pthread_t) * resolvers);  // create resolver thread ID storage
    respacket.buff = buffer;                        // attach the bounded buffer
    respacket.resfile = resfile;                    // pass the output resolved file struct (initialized above)
    for(r = 0; r < resolvers; r++)
    {
        if(pthread_create(&resID[r], NULL, resolver, (void*) &respacket) != 0) //creates thread using resolver function with resolve parameter
        {
            fprintf(stderr, "Error creating a resolver thread.\n");
            return -1;
        }
    }

    // Wait for Threads
    for(r = 0; r < requesters; r++)                 // wait for requesters & print results
    {
        if(pthread_join(reqID[r], NULL) != 0)       // join the thread & check for error
            fprintf(stderr, "Error joining thread %d.\n", reqID[r]);
    }
    pthread_mutex_lock(&buffer->bufflock);          // once requesters are done, lock the buffer to update 
    buffer->reqsdone = 1;                           // indicate that the requesters are done if not indicated already
    pthread_cond_broadcast(&buffer->bempty);        // signal that the buffer is empty
    pthread_mutex_unlock(&buffer->bufflock);        // unlock the buffer mutex
    for(r = 0; r < resolvers; r++)                  // wait for resolvers & print results
    {
        if(pthread_join(resID[r], NULL) != 0)       // join the thread & check for error
            fprintf(stderr, "Error joining thread %d.\n", resID[r]);
    }

    // Cleanup & Close
    for(i = 0; i < numfiles; i++)                   // close the input files
    {
        fclose(files->files[i].fp);
    }
    fclose(reqfile.fp);                             // close the output files
    fclose(resfile.fp);
    free(fileslist);                                // free the preliminary list of file names
    free(files);                                    // free the structure of files
    free(buffer);                                   // free the bounded buffer
    free(reqID);                                    // free the requester ID array
    free(resID);                                    // free the resolver ID array
    //pthread_mutex_destroy(&);                     // destroy the mutexes
    clock_gettime(CLOCK_MONOTONIC, &t1);            // stop elapsted time stopwatch
    elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1000000000.0;
    printf("./multi-lookup: total time is %f seconds\n", elapsed);
    return 0;                                       // return success
}

/* Producer: reads hostnames from file and pushes to the queue & requester log, returns # files serviced */
void* requester(void* packet)
{
    int serviced = 0;                               // tracker for number of files serviced
    requester_helper(packet, &serviced);            // begin recursive helper function for servicing files
    printf("thread %x serviced %d files\n", pthread_self(), serviced);
    return 0;
}

/* Recursive function that services input files until there are none left to service */
void* requester_helper(void* packet, int* serviced)
{
    struct Req_Packet* p = (struct Req_Packet*) packet; //cast void* to packet struct
    FileList* flist = p->files;                     // collect the argument variables
    Buffer* buff = p->buff;                         // pointer to the buffer
    File reqlog = p->reqfile;                       // requester log file structure

    // Base Case: Check Remaining Files
    pthread_mutex_lock(&flist->listlock);           // lock the list from other threads when grabbing a new file
    if(flist->num_serviced == flist->tot)           // if there are no more files to read, exit
    {
        pthread_mutex_unlock(&flist->listlock);     // unlock before exiting
        return 0;
    }
    // Find an Unserviced File
    while(flist->files[flist->curr].serviced)       // loop through file list while files serviced until an unserviced one is found
    {
        if(flist->curr == flist->tot-1)
            flist->curr = 0;                        // restart the loop if the end is reached
        else
            flist->curr++;                          // update the current position in the list
    }
    File* currfile = &flist->files[flist->curr];    // grab the current file from the list
    (*serviced)++;                                  // increment the number of files serviced
    if(flist->curr == flist->tot-1)                 // update the list after the new file has been chosen
        flist->curr = 0;
    else
        flist->curr++;
    pthread_mutex_unlock(&flist->listlock);         // release the file list now that file chosen & it has been updated

    // Gather Hostnames & Write to Files/Buffer
    while(1)
    {
        pthread_mutex_lock(&currfile->flock);       // lock the current file from other threads (obtained every cycle since fgets updates offset)
        char* hostname = (char*) malloc(sizeof(char) * (MAX_NAME_LENGTH)); //allocate hostname memory
        // Read the File
        if(fgets(hostname, MAX_NAME_LENGTH, currfile->fp)) //fgets but doesn't lock the stream w/ return check
        {
            pthread_mutex_unlock(&currfile->flock); // don't forget to unlock the current file
            hostname[strcspn(hostname, "\r\n")] = 0;  // remove newline by getting span until newline char
            //printf("Requester Thread - hostname: \"%s\"\n", hostname);
            // Add Hostname to the Buffer
            pthread_mutex_lock(&buff->bufflock);    // grab the buffer if available (sleep otherwise)        
            while(buff->full)                       // if the buffer is full, we (producer must wait)
            {                                       // block on this condition until buff not full
                pthread_cond_wait(&buff->bfull, &buff->bufflock); //wait until broadcast that buffer not full
            }
            buff->arr[buff->curr] = hostname;       // add the hostname to the buffer if not full and not busy
            // Update Buffer Parameters
            if(buff->curr == buff->size - 1)        // if the buffer is now full,
                buff->full = 1;                     // set the full variable
            buff->curr++;                           // update the current position
            if(buff->curr - 1 == 0)                 // check if the buffer was empty before the new hostname
            {
                buff->empty = 0;                    // the buffer is no longer empty
                pthread_cond_broadcast(&buff->bempty); //tell other threads buffer not empty anymore
            }
            // Write to the Requester Log
            pthread_mutex_lock(&reqlog.flock);      // protect the requester log during writing
            if(fputs(hostname, reqlog.fp) == EOF)   // write the hostname to the requester log
                fprintf(stderr, "Error writing to the requester log file.\n"); 
            fputc('\n', reqlog.fp);                 // add a newline after each host name
            pthread_mutex_unlock(&buff->bufflock);  // release the buffer
            pthread_mutex_unlock(&reqlog.flock);    // don't forget to unlock the file
        }
        else                                        // if fgets is done (the entire file/all hostnames have been read)
        {  
            free(hostname);                         // delete unneeded memory
            if(!currfile->serviced)
            {
                currfile->serviced = 1;             // update that this file has been fully serviced
                pthread_mutex_unlock(&currfile->flock); //release the current file
                pthread_mutex_lock(&flist->listlock);
                flist->num_serviced++;              // increment the number of files this thread serviced
                pthread_mutex_unlock(&flist->listlock); 
                // Call the Recursive Function Again
                requester_helper(packet, serviced++); //recursive call with one more file serviced
            }
            else                                    // unlikely case at eof and service flag not raised
                pthread_mutex_unlock(&currfile->flock); // unlock the mutex if done with the file
            break;                                  // exit the loop after file complete
        }
    }
    return 0;
}

/* Consumer: resolves hostnames from queue and writes to resolver log, returns # hostnames resolved */
void* resolver(void* packet)
{
    struct Res_Packet* p = (struct Res_Packet*) packet; //cast resolver packet struct from void*
    Buffer* buff = p->buff;                         // collect the buffer pointer
    File reslog = p->resfile;                       // resolver log file structure
    int resolved = 0;                               // track the number of host names that were resolved

    // Loop Until the Queue is Empty & Requesters Done
    while(1)
    {
        // Bounds/Initial Conditions
        pthread_mutex_lock(&buff->bufflock);        // lock the buffer for use
        while(buff->empty && !buff->reqsdone)       // wait while the buffer is empty and requesters are not done yet
        {
            pthread_cond_wait(&buff->bempty, &buff->bufflock);
        }
        if(buff->empty && buff->reqsdone)           // if the buffer us empty & requesters done, we are done!
        {
            pthread_mutex_unlock(&buff->bufflock);  // release the mutex before exiting loop
            break;
        }
        // Get Hostname from Buffer
        char* hostname = buff->arr[buff->curr-1];   // set hostname to the current array position
        buff->arr[buff->curr-1] = NULL;             // clear the pointer to that position with NULL
        if(buff->curr == 1)                         // if the current position was 1, the buffer is now empty
        {
            buff->empty = 1;                        // buffer now empty
        }
        buff->curr--;
        if(buff->curr == buff->size - 1)            // buffer is no longer full now that hostname was popped off
        {
            buff->full = 0;                         // not full anymore
            pthread_cond_broadcast(&buff->bfull);   // alert the other (producer) threads
        }
        pthread_mutex_unlock(&buff->bufflock);      // unlock the buffer
        // Get IP Address
        char ip[INET6_ADDRSTRLEN];                  // character array to store IP address from dnslookup
        //printf("Resolver Thread - hostname: \"%s\"\n", hostname);
        if(dnslookup(hostname, ip, INET6_ADDRSTRLEN) == UTIL_SUCCESS)
        {
            // Construct the Mapping
            char* mapping = malloc(sizeof(char) * (strlen(hostname) + strlen(ip) + 4)); // string for hostname, IP/n mapping
            strcpy(mapping, hostname);              // copy the hostname as the first string into the mapping
            strcat(mapping, ", ");                  // concatenate a comma and a space
            strcat(mapping, ip);                    // add ip address
            strcat(mapping, "\n");                  // add newline
            // Write Mapping to File
            pthread_mutex_lock(&reslog.flock);      // lock the resolver log from other threads
            if(fputs(mapping, reslog.fp) == EOF)    // write the mapping to the resolver log
                fprintf(stderr, "Error writing to the resolver log file.\n"); 
            pthread_mutex_unlock(&reslog.flock);    // don't forget to unlock the file
            resolved++;                             // increment the number of successfully resolved host names
            free(mapping);                          // free the temporary concatenation of strings 
        }
        else                                        // if the lookup was a failure
        {
            // Construct the Mapping
            char* mapping = malloc(sizeof(char) * (strlen(hostname) + 16)); // string for hostname, NOT_RESOLVED\n
            strcpy(mapping, hostname);              // copy the hostname as the first string into the mapping
            strcat(mapping, ", NOT_RESOLVED\n");    // concatenate a comma, the NOT_RESOLVED keyword & newline
            // Write Mapping to File
            pthread_mutex_lock(&reslog.flock);      // lock the resolver log from other threads
            if(fputs(mapping, reslog.fp) == EOF)    // write the mapping to the resolver log
                fprintf(stderr, "Error writing to the resolver log file.\n"); 
            pthread_mutex_unlock(&reslog.flock);    // don't forget to unlock the file
            free(mapping);                          // free the temporary concatenation of strings
        }
        free(hostname);                             // free hostname memory
    }
    printf("thread %x resolved %d hostnames\n", pthread_self(), resolved);
    return NULL;
}

