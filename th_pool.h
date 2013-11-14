#ifndef _TH_POOL_

#define _TH_POOL_
#define BIG_INT_SIZE 1000

#include <pthread.h>
#include <semaphore.h>

char fS[BIG_INT_SIZE];                              /*file Size of the directory as a big Int*/

/* Individual job */
typedef struct thpool_job_t{
  void*  (*function)(void* arg);                     /* function pointer         */
  void*                     arg;                     /* function's argument      */
  struct thpool_job_t*     next;                     /* pointer to next job      */
  struct thpool_job_t*     prev;                     /* pointer to previous job  */
}thpool_job_t;


/* Job queue as doubly linked list */
typedef struct thpool_jobqueue{
  thpool_job_t *head;                                /* pointer to head of queue */
  thpool_job_t *tail;                                /* pointer to tail of queue */
  int           jobsN;                               /* amount of jobs in queue  */
  sem_t        *queueSem;                            /* semaphore(this is probably just holding the same as jobsN) */
}thpool_jobqueue;


/* The threadpool */
typedef struct thpool_t{
  pthread_t*       threads;                          /* pointer to threads' ID   */
  int              threadsN;                         /* amount of threads        */
  thpool_jobqueue* jobqueue;                         /* pointer to the job queue */
  int*             active;                           /* boolean array saying whether a thread is active*/
}thpool_t;


/* Container for all things that each thread is going to need */
typedef struct thread_data{                            
  pthread_mutex_t *mutex_p;
  thpool_t        *tp_p;
}thread_data;

thpool_t* th_pool_init(int nthreads);

int thpool_add_work(thpool_t* tp, void *(*function_p)(void*), void* arg_p);

int jobqueue_add_work(thpool_jobqueue* jq, thpool_job_t* nJ);

int thpool_do_work(thpool_t* tp);

//void thpool_jobqueue_removelast(thpool_t* tp);

void thpool_destroy(thpool_t* tp);

int thpool_jobqueue_init(thpool_t* tp_p);

#endif
