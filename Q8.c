/*******************************************************************
 *
 *	filesize.c 
 *      Objective: To calculate the total size of all the files that fall under a directory with threads.
 *                 and with an added constraint that only system calls shoould be used.
 * 
 *      Method: Threads are implemented with pthreads library.
 *              System calls such as readdir_r are not thread safe on all file systems.
 *              Please make sure that your file system is thread safe for running this code.
 *              (Though care was taken that no two threads try to manipulate or analyze the same file directory.)
 *
 *      MPI_Bcast uses Tree broadcast algorithm, for a good network utilzation.
 *	MPICH uses different algorithm based on the size of buffer.
 *	In essence it performs scatter followed by all_gather, it has a complexity of log(number of processes).
 *
 *      For compiling: use the make file provided.
 *      For running: See the README file in the same directory.
 *       
----------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>          /*Pthreads library*/
#include <sys/types.h>        /*For system calls*/
#include <dirent.h>           /*Structure for file structures*/
#include <errno.h>            /*Error codes*/
#include <fcntl.h>            /*For some macros like O_RDonly*/
#include "th_pool.c"          /*For thread pool API*/
#include "th_pool.h"

#define MAX_THREADS 1000

thpool_t* tp;
int fileSize=0;               /*Global file size variable*/
//pthread_mutex_t fileMutex;  /*Mutex for updates on file*/
pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;
int numThreads=0;             /*present number of threads*/
int maxThreads=0;             /*Maximum number of threads that can be triggered*/
pthread_t threads[MAX_THREADS];/*For threads*/
int iret;

/*This defines add on bigIntegers so that the size can vary on larger limits.*/
int bigIntAdd(char* tmp,int add){
  int carry=0;
  int i=BIG_INT_SIZE-1;
  while((add>0)||(carry>0)){
    int a = (tmp[i]-'0')+(add%10)+carry;
    tmp[i]=(a%10+'0');
    carry = a/10;
    add /= 10;
    i--;
  }
}

/*Calculates filesize of all the files that fall under the directory 
  given as arguement and adds it to the gloab variable*/
void *FileSize(void* ptr)
{
  DIR *dir;
  struct dirent *entry;
  char *folder = (char*)ptr;
  
  //printf("Trying to open %s\n",folder);
  if ((dir = opendir(folder)) == NULL)
    perror("opendir() error");
  else {
    //printf("Successsfully opened %s\n",folder);
    while ((entry = readdir(dir)) != NULL){
      /*If it is a regular file*/
      //printf(" %s %d\n", entry->d_name,entry->d_type);
      if(entry->d_type==8)
	{
	  int file=0;
	  char tmpFolder[1000];
	  strcpy(tmpFolder,folder);
	  strcat(tmpFolder,"/");
	  strcat(tmpFolder,entry->d_name);
	  //printf("Called %s\n",tmpFolder);
	  
	  if((file=open(tmpFolder,O_RDONLY)) == -1){
	    //printf("%s \n",tmpFolder);
	    //perror("File opening error ");
            continue;
	  }
	  
	  struct stat fileStat;
	  //printf("%s %d\n",tmpFolder,(int)fileStat.st_size);
	  if(fstat(file,&fileStat) == -1)    
	    continue ;
	  
	  //fileSize+=fileStat.st_size;
	  bigIntAdd(fS,fileStat.st_size);
	  //printf("%s %d\n",tmpFolder,(int)fileStat.st_size);
	  close(file);
	}
      else if(entry->d_type==4)
	{
	  /*Skip parent and itself; this avoids infinite loops*/
	  if ((strcmp(entry->d_name, ".") == 0)
	      || (strcmp(entry->d_name, "..")==0))
	    continue;    
	  char tmpFolder[1000];
	  char folderName[1000];
	  strcpy(tmpFolder,folder);
	  strcat(tmpFolder,"/");
	  strcat(tmpFolder,entry->d_name);
	  strcpy(folderName,entry->d_name);
	  //printf("Called %s\n",tmpFolder);
	  
	  /*This way of recursive calls are stacking up the 
	    files that are opened with open and after a while 
	    open may return with error because of many opened files
	    To avoid such errors we close the descriptor, open 
	    it and read the directory until we get to previous state and 
	    then let things go*/
	  closedir(dir);
	  (*FileSize)(tmpFolder);
	  if ((dir = opendir(folder)) == NULL)
	    perror("opendir() error");
	  /*Seek the previous set pointer*/
	  while ((entry = readdir(dir)) !=NULL){
	    if(strcmp(entry->d_name,folderName)==0)
	      break;
	  }
	}
    }
    closedir(dir);
  }
}

/*This routine differs from fileSize in that instead of recursively calling the function,
  threads were created and assigned whenever the number of threads is less than MAX_THREADS
  Special care is taken when accessing the shared variable and open system call is replaced 
  with opendir_r which is thread safe*/
void *threadSafeFileSize(void* ptr){
  printf("Analyzing: %s\n",(char*)ptr);
  int return_code;
  DIR *dir;
  struct dirent allocEntry;
  struct dirent *entry = &allocEntry;
  struct dirent *result;
  char folder[10000];
  //char *folder = (char*)ptr;
  strcpy(folder,(char*)ptr);
  
  if ((dir = opendir(folder)) == NULL){
    printf("Failed to open: %s\n",folder);
    perror("opendir() error");
    ;
  }
  else {
    for (return_code = readdir_r(dir, entry, &result);
         result != NULL && return_code == 0;
         return_code = readdir_r(dir, entry, &result)){
      //while (readdir_r(dir,entry,&result) != NULL){
      
      //printf("%s\n",entry->d_name);
      /*If it is a regular file*/
      if(entry->d_type==8)
	{
	  int file=0;
	  char tmpFolder[10000];

	  /*In readdir_r the entry_name may not be null terminated*/
	  //char entryName[1000];
	  //strcpy(entryName,entry->d_name);
	  //strcat(entryName,'\0');
	  
	  strcpy(tmpFolder,folder);
	  strcat(tmpFolder,"/");
	  strcat(tmpFolder,entry->d_name);

	  printf("%s\n",tmpFolder);
	  
	  if((file=open(tmpFolder,O_RDONLY)) == -1){
	    //printf("%s \n",tmpFolder);
	    //perror("File opening error ");
            continue;
	  }
	  
	  struct stat fileStat;
	  //printf("%s %d\n",tmpFolder,(int)fileStat.st_size);
	  if(fstat(file,&fileStat) == -1)    
	    continue ;
	  
	  //fileSize+=fileStat.st_size;
	  //pthread_mutex_lock(&mutex);
	  bigIntAdd(fS,fileStat.st_size);
	  //pthread_mutex_unlock(&mutex);
	  
	  //printf("%s %d\n",tmpFolder,(int)fileStat.st_size);
	  if(close(file))
	    perror("Closing the file");
	}
      else if(entry->d_type==4)
	{
	  /*Skip parent and itself; this avoids infinite loops*/
	  if ((strcmp(entry->d_name, ".") == 0)
	      || (strcmp(entry->d_name, "..")==0))
	    continue;    
	  

	  //strcat(entryName,'\0');
	  char tmpFolder[10000];
	  char folderName[10000];
	  char folderStore[1000];
	  int i=0;
	  int size = 0;
	  folder[strlen(folder)]='\0';
	  strcpy(folderStore,folder);

	  printf("\n\nOlder folder name %s\n",folder);
	  /*for(i=0;i<10000;i++){
	    tmpFolder[i] = folder[i];
	    if(folder[i]=='\0')
	      break;
	    size++;
	  }
	  
	  tmpFolder[size]='/';
	  tmpFolder[size+1]='\0';
	  size++;
	  //strcat(tmpFolder,entry->d_name);
	  int j=0;
	  for(i=size;i<10000;i++){
	    tmpFolder[i] = entry->d_name[j];
	    if(entry->d_name[j]=='\0')
	      break;
	    j++;
	    size++;
	    }*/
	  
	  //strcpy(tmpFolder,folder);
	  for(i=0;i<10000;i++){
	    tmpFolder[i] = folder[i];
	    if(folder[i]=='\0')
	      break;
	    size++;
	  }
	  
	  strcat(tmpFolder,"/");
	  strcpy(folderName,entry->d_name);
	  strcat(tmpFolder,entry->d_name);
	  tmpFolder[strlen(tmpFolder)]='\0';
	  strcpy(folder,folderStore);
	  
	  printf("Folder name %s\n",folderName);
	  printf("tmpfolder %s\n",tmpFolder);
	  //printf("folder %s\n\n\n",folder);
	  for(i=0;i<strlen(folder);i++)
	    printf("%c",folder[i]);
	  printf("\n");
	    
	  /*for every directory encountered in the folder add the job to the task pool*/
	  //printf("Q8.c: Add work\n");
	  //pthread_mutex_lock(&mutex);
	  printf("Job added: %s\n",(char*)tmpFolder);
	  thpool_add_work(tp,(void*)threadSafeFileSize,(void*)tmpFolder);
	  //pthread_mutex_unlock(&mutex);
	}
    }
    if(closedir(dir))
      perror("Closing directory");
  }
}

int main(int argc,char** argv)
{
  if(argc!=3)
    {
      printf("------------------------\n");
      printf("The usage is: \n");
      printf("exec [dirctory] [num_threads]\n");
      printf("------------------------\n");
      return(1);
    }
  
  char fldr[1000];
  strcpy(fldr,argv[1]);
  char *message2 = "Thread 2";
  int  iret1, iret2;
  int i=0;
  
  /*Make sure that fS is initialized to 0*/
  for(i=0;i<BIG_INT_SIZE;i++)
    fS[i] = '0';
  
  maxThreads = atoi(argv[2]);
  
  /*To avoid the overhead of defining the task pool in the case of #threads is 1*/
  if(maxThreads<1)
    {
      (*FileSize)(fldr);
    }
  else
    {
      /*Initialize*/
      tp = th_pool_init(maxThreads);
      
      //thpool_add_work(tp,(void*)threadSafeFileSize,(void*)fldr);
      threadSafeFileSize(fldr);
      
      int active=0;
      int t;

      while((tp->jobqueue->jobsN>0)){//||(active>0)){
	sleep(1);
	//printf("Active: %d\n",active);
      }
    }
  
  int start=0;
  for(i=0;i<BIG_INT_SIZE;i++){
    if((!start)&&(fS[i]!='0'))
      start=1;
    if(start)
      printf("%c",fS[i]);
  }
  printf(" Bytes \n");
  //printf("Thread 2 returns: %d\n",iret2);
  exit(0);
}
