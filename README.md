# DNS-Server-Multithreaded
Similar to the operation performed each time you access a new website in your web browser, this multi-threaded application, written in C, resolves domain names to IP addresses.

The program processes files containing one hostname per line using a single requester thread per file. The hostnames from the number of threads (one per file) are then placed into a shared array that uses a solution to the Bounded Buffer Synchronization Problem (where producers and consumers compete for the array resource using mutexes, semaphores, and conditional variables). 
The application synchronizes access to shared resources (the array, logfiles, stdout/stderr and argc/argv[] which are not thread-safe by default) to avoid deadlock, busy wait, delays, and starvation. 
Some number of resolver threads, determined by a command line argument, will then resolve hostnames from the shared array, lookup the IP address for that hostname, and write the results to a logfile results.txt. Each requester thread will note how many files they serviced in the command line. Once all the input files have been processed the requester threads will terminate. Each resolver thread notes how many hostnames it resolved. Once all the hostnames have been looked up, the resolver threads will terminate and the program will end. Finally, the total runtime is displayed before the program quits. 

To run, simple make the Makefile. The input/names&.txt contains a set of sample files with websites to exhibit the code's functionality. make clean to cleanup any extraneous .o files or executables. For example,
./multi-lookup 5 5 serviced.txt resolved.txt input/names1*.txt



The Man-Page for multi-lookup would appear as follows:

NAME
```
multi-lookup - resolve a set of hostnames to IP addresses
```

SYNOPSIS
```
multi-lookup <# requester> <# resolver> <requester log> <resolver log> [ <data file> ... ]
```
  
DESCRIPTION
The file names specified by <data file> are passed to the pool of requester threads which place information into a shared data area. Resolver threads read the shared data area and find the corresponding IP address.
  
```
 <# requesters> number of requestor threads to place into the thread pool
 <# resolvers> number of resolver threads to place into the thread pool
 <requester log> name of the file into which requested hostnames are written
 <resolver log> name of the file into which hostnames and resolved IP addresses are written
 <data file> filename to be processed. Each file contains a list of host names, one per line, that are to be resolved
```
   
SAMPLE INVOCATION
```
./multi-lookup 5 5 serviced.txt resolved.txt input/names1*.txt
```
   
SAMPLE CONSOLE OUTPUT
```
thread 0f9c0700 serviced 1 files
thread 0f1bf700 serviced 1 files
thread 109c2700 serviced 1 files
thread 101c1700 serviced 1 files
thread 0e9be700 serviced 2 files
thread 121c5700 resolved 26 hostnames
thread 131c7700 resolved 34 hostnames
thread 111c3700 resolved 23 hostnames
thread 129c6700 resolved 35 hostnames
thread 119c4700 resolved 5 hostnames 
./multi-lookup: total time is 25.323361 seconds
```
