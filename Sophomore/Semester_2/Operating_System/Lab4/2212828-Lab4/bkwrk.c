#include "bktpool.h"
#include <signal.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#define _GNU_SOURCE
#include <linux/sched.h>
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t worker_mutex = PTHREAD_MUTEX_INITIALIZER;
// #define DEBUG
#define INFO

#ifndef WORK_THREAD
int *wrkid_tid;
int *wrkid_busy;
struct bkworker_t *worker;
int *id;
int *tid;
#endif

// struct bkworker_t* worker;
void *bkwrk_worker(void *arg)
{
  sigset_t set;
  int sig;
  int s;
  int i = *((int *)arg); // Default arg is integer of workid
  struct bkworker_t *wrk = &worker[i];

  /* Taking the mask for waking up */
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGQUIT);

#ifdef DEBUG
  fprintf(stderr, "worker %i start living tid %d \n", i, getpid());
  fflush(stderr);
#endif

  while (1)
  {
    /* wait for signal */
    s = sigwait(&set, &sig);
    if (s != 0)
      continue;

#ifdef INFO
    fprintf(stderr, "worker wake %d up\n", i);
#endif

    /* Busy running */
    // if (wrk -> func != NULL) {
    //   wrk -> func(wrk -> arg);

    // }
    if (worker[i].func != NULL)
    {
      worker[i].func(worker[i].arg);
    }
    /* Advertise I DONE WORKING */
    wrkid_busy[i] = 0;
    worker[i].func = NULL;
    worker[i].arg = NULL;
    worker[i].bktaskid = -1;
  }
}

int bktask_assign_worker(unsigned int bktaskid, unsigned int wrkid)
{
  if (wrkid < 0 || wrkid > MAX_WORKER)
    return -1;

  struct bktask_t *tsk = bktask_get_byid(bktaskid);

  if (tsk == NULL)
    return -1;

  /* Advertise I AM WORKING */
  wrkid_busy[wrkid] = 1;

  worker[wrkid].func = tsk->func;
  worker[wrkid].arg = tsk->arg;
  worker[wrkid].bktaskid = bktaskid;
  printf("Assign tsk %d wrk %d \n", tsk->bktaskid, wrkid);
  return 0;
}

int bkwrk_create_worker(int shmid_tid, int shmid_busy, int shmid_worker, int shmid_id, int shmid_tid_tid)
{
  unsigned int i;

  for (i = 0; i < MAX_WORKER; i++)
  {
#ifdef WORK_THREAD
    void **child_stack = (void **)malloc(STACK_SIZE);
    unsigned int wrkid = i;
    pthread_t threadid;

    sigset_t set;
    int s;

    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    /* Stack grow down - start at top*/
    void *stack_top = child_stack + STACK_SIZE;

    wrkid_tid[i] = clone(&bkwrk_worker, stack_top,
                         CLONE_VM | CLONE_FILES,
                         (void *)&i);
#ifdef INFO
    fprintf(stderr, "bkwrk_create_worker got worker %u\n", wrkid_tid[i]);
#endif
    usleep(100);

#else
    /* TODO: Implement fork version of create worker */
    unsigned int wrkid = i;
    sigset_t set;
    int s;

    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);
    pid_t pid = fork();
    if (pid == -1)
    {
      perror("fork erorrrrrrrrrrrrrrrrrrrrrrrr");
      return 1;
    }
    else if (pid == 0)
    {
      wrkid_tid = (int *)shmat(shmid_tid, (void *)0, 0);
      wrkid_busy = (int *)shmat(shmid_busy, (void *)0, 0);
      worker = (struct bkworker_t *)shmat(shmid_worker, (void *)0, 0);
      id = (int *)shmat(shmid_id, (void *)0, 0);
      tid = (int *)shmat(shmid_tid_tid, (void *)0, 0);
      bkwrk_worker((void *)&i);
    }
    else
    {

      wrkid_tid[i] = pid;
      // Parent process
      // Your code for the parent process goes here
    }
#ifdef INFO
    fprintf(stderr, "bkwrk_create_worker got worker %u\n", wrkid_tid[i]);
#endif
    usleep(100);

#endif
  }

  return 0;
}

int bkwrk_get_worker()
{
  /* TODO Implement the scheduler to select the resource entity
   * The return value is the ID of the worker which is not currently
   * busy or wrkid_busy[1] == 0
   */
  pthread_mutex_lock(&worker_mutex);

  int freeWorker = -1;

  for (int i = 0; i < MAX_WORKER; i++)
  {
    if (wrkid_busy[i] == 0)
    {
      freeWorker = i;
      break;
    }
  }
  pthread_mutex_unlock(&worker_mutex);

  return freeWorker;
}

int bkwrk_dispatch_worker(unsigned int wrkid)
{

#ifdef WORK_THREAD
  unsigned int tid = wrkid_tid[wrkid];
  /* Invalid task */
  if (worker[wrkid].func == NULL)
    return -1;

#ifdef DEBUG
  fprintf(stderr, "brkwrk dispatch wrkid %d - send signal %u \n", wrkid, tid);
#endif

  syscall(SYS_tkill, tid, SIG_DISPATCH);
#else
  /* TODO: Implement fork version to signal worker process here */
  unsigned int pid = wrkid_tid[wrkid];
  if (worker[wrkid].func == NULL)
    return -1;
#ifdef DEBUG
  fprintf(stderr, "brkwrk dispatch wrkid %d - send signal %u \n", wrkid, tid);
#endif
  kill(pid, SIG_DISPATCH);
#endif
}