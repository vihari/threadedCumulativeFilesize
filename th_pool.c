/*******************************************************************
 *
 *	th_pool.c 
 *      Objective: An api to implement the thread pool using pthreads.
 *                 Fixed number of threads are initialized at the start 
 *                 and jobs are subsequently added to the job queue.
 * 
 *      Method: Threads are implemented with pthreads library.
 *      Purpose: see filesize.c in the same directory for use case 
 *               and sample use case
 *              
 *      For compiling: use the make file provided.
 *      For running: See the README file in the same directory.
 *       
 *      Reference: https://github.com/Pithikos/C-Thread-Pool/
 *      The above reference is used to develop some insight and hence 
 *      may have some un-deliberate similarities in the code.
 *      More ever camel notation is used in this code.
----------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>          /*Pthreads library*/
#include <sys/types.h>        /*For system calls*/
#include <dirent.h>           /*Structure for file structures*/
#include <errno.h>            /*Error codes*/
#include <fcntl.h>            /*For some macros like O_RDonly*/

#include "th_pool.h"

/*For synchronization purposes between various threads*/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int killSignal=0;

/*Initialize with #threads*/
thpool_t* th_pool_init(int nthreads){
  thpool_t* tp_p;

  if (!nthreads || nthreads<1) nthreads=1;

  /* Make new thread pool */
  tp_p=(thpool_t*)malloc(sizeof(thpool_t));                              /* MALLOC thread pool */
  if (tp_p==NULL){
    fprintf(stderr, "thpool_init(): Could not allocate memory for thread pool\n");
    return NULL;
  }

  tp_p->threads=(pthread_t*)malloc(nthreads*sizeof(pthread_t));          /* MALLOC thread IDs */
  if (tp_p->threads==NULL){
    fprintf(stderr, "thpool_init(): Could not allocate memory for thread IDs\n");
    return NULL;
  }

  tp_p->active=(int*)malloc(nthreads*sizeof(int));          /* MALLOC active threads */
  tp_p->threadsN=nthreads;

  /* Initialise the job queue */
  if (thpool_jobqueue_init(tp_p)==-1){
    fprintf(stderr, "thpool_init(): Could not allocate memory for job queue\n");
    return NULL;
  }

  /* Initialise semaphore*/
  tp_p->jobqueue->queueSem=(sem_t*)malloc(sizeof(sem_t));                 /* MALLOC job queue semaphore */
  sem_init(tp_p->jobqueue->queueSem, 0, 0);                               /* no shared, initial value */

  /* Make threads in pool */
  int t;
  for (t=0; t<nthreads; t++){
    pthread_create(&(tp_p->threads[t]), NULL, (void *)thpool_do_work, (void *)tp_p); /* MALLOCS INSIDE PTHREAD HERE */
  }

  return tp_p;  
}

int thpool_jobqueue_removelast(thpool_t* tp_p){
  thpool_job_t *oldLastJob;
  oldLastJob = tp_p->jobqueue->tail;

  /* fix jobs' pointers */
  switch(tp_p->jobqueue->jobsN){

  case 0:     /* if there are no jobs in queue */
    return -1;
    break;

  case 1:     /* if there is only one job in queue */
    tp_p->jobqueue->tail=NULL;
    tp_p->jobqueue->head=NULL;
    break;

  default: 	/* if there are more than one jobs in queue */
    oldLastJob->prev->next=NULL;               /* the almost last item */
    tp_p->jobqueue->tail=oldLastJob->prev;

  }

  (tp_p->jobqueue->jobsN)--;

  int sval;
  sem_getvalue(tp_p->jobqueue->queueSem, &sval);
  return 0;
}

int thpool_jobqueue_init(thpool_t* tp_p){
  tp_p->jobqueue=(thpool_jobqueue*)malloc(sizeof(thpool_jobqueue));      /* MALLOC job queue */
  if (tp_p->jobqueue==NULL) return -1;
  tp_p->jobqueue->tail=NULL;
  tp_p->jobqueue->head=NULL;
  tp_p->jobqueue->jobsN=0;
  return 0;
}

/*Pointer to the thread pool and task along with arguements*/
int thpool_add_work(thpool_t* tp, void *(*function_p)(void*), void* arg_p){
  thpool_job_t* newJob;
  newJob = (thpool_job_t*)malloc(1*sizeof(thpool_job_t));
  newJob->function = function_p;
  newJob->arg = arg_p;
  
  /*As many threads can contend to add jobs to the queue*/
  /*lock*/
  pthread_mutex_lock(&mutex);
  jobqueue_add_work(tp->jobqueue,newJob);
  /*unlock*/
  pthread_mutex_unlock(&mutex);
  
}

int jobqueue_add_work(thpool_jobqueue* jq, thpool_job_t* nJ){
  nJ->next=NULL;
  nJ->prev=NULL;

  thpool_job_t *oJ;
  oJ = jq->head;

  /* fix jobs' pointers */
  switch(jq->jobsN){

  case 0:     /* if there are no jobs in queue */
    jq->tail=nJ;
    jq->head=nJ;
    break;
    
  default: 	/* if there are already jobs in queue */
    oJ->prev=nJ;
    nJ->next=oJ;
    jq->head=nJ;
  }

  (jq->jobsN)++;     /* increment amount of jobs in queue */

  /*Threads may be waiting for the semaphore to be non empty*/
  sem_post(jq->queueSem);

  int sval;
  sem_getvalue(jq->queueSem, &sval);
}

int thpool_do_work(thpool_t* tp){
  while(!killSignal){
    //printf("DO work: %d\n",tp->jobqueue->jobsN);
    void* (*func)(void* arg);
    void* arg;
    int t;

    for(t=0;t<tp->threadsN;t++){
      pthread_t id= pthread_self();
      if(pthread_equal(tp->threads[t],id)){
	tp->active[t] = ((tp->jobqueue->jobsN)>0)?1:0;
	break;
      }
    }
    
    /*Non spinning or conditional wait*/
    if (sem_wait(tp->jobqueue->queueSem)) {/* WAITING until there is work in the queue */
      perror("thpool_do_work(): Waiting for semaphore");
      exit(1);
    }

    thpool_job_t* job;

    /*lock*/
    pthread_mutex_lock(&mutex);
    job = tp->jobqueue->head;
    func = job->function;
    arg = job->arg;
    thpool_jobqueue_removelast(tp);
    /*Unlock*/
    pthread_mutex_unlock(&mutex);
    
    func(arg);

    return 0;
  }
}
  
void thpool_destroy(thpool_t* tp){
  int start=0;
  int i;
  for(i=0;i<BIG_INT_SIZE;i++){
    if((!start)&&(fS[i]!='0'))
      start=1;
    if(start)
      printf("%c",fS[i]);
  }
  printf("\n");
  
  /*free the heap and awake all threads that are waiting on the job queue semaphore*/
  int t=0;
  killSignal = 1;

  for(t=0;t<tp->threadsN;t++){
    sem_post(tp->jobqueue->queueSem);
  }
  sem_destroy(tp->jobqueue->queueSem);
  
  /*Wait for all threads to finish*/
  for(t=0;t<tp->threadsN;t++){
    pthread_join(tp->threads[t],NULL);
  }

  /*Deallocation*/
  free(tp->threads);
  free(&tp->threadsN);
  free(tp->jobqueue);
  free(tp);
  printf("Destroy successful\n");
}
 
void exclusiveOp(void* tmp,void* add){
  pthread_mutex_lock(&mutex);
  //func(tmp,*add);
  bigIntAdd(tmp,*(int*)add);
  pthread_mutex_unlock(&mutex);
}
