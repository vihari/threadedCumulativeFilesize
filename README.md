filesize.c
Objective: To calculate the total size of all the files that fall under a directory with threads.
 and with an added constraint that only system calls shoould be used.
 
Method: Threads are implemented with pthreads library
        System calls such as readdir_r are not thread safe on all fil\
e systems.
        Please make sure that your file system is thread safe for running this code.(Though care was taken that no two threads try to manipulate or analyze the same file directory.)
 
     MPI_Bcast uses Tree broadcast algorithm, for a good network utilzation.
     MPICH uses different algorithm based on the size of buffer.
     In essence it performs scatter followed by all_gather, it has a complexity of log(number of processes).
 
     For compiling: use the make file provided.
     For running: See the README file in the same directory.