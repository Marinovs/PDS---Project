#ifndef THREADPOOL_H
#define THREADPOOL_H

#define MAX_THREADS 10
#define MAX_QUEUE 65536

typedef struct threadpool_t threadpool_t;

typedef enum
{
   threadpool_invalid = -1,
   threadpool_lock_failure = -2,
   threadpool_queue_full = -3,
   threadpool_shutdown = -4,
   threadpool_thread_failure = -5
} threadpool_error_t;

typedef enum
{
   threadpool_graceful = 1
} threadpool_destroy_flags_t;

typedef struct
{
    void (*function)(void *);
    void *argument;
} threadpool_task_t;

struct threadpool_t
{
    pthread_mutex_t lock;     /* mutex for the thread tasks*/
    pthread_mutex_t logLock;  /* mutex for the log file*/
    pthread_cond_t notify;    /* Conditional variable */
    pthread_t *threads;       /* Starting Pointer of Thread Array */
    threadpool_task_t *queue; /* Starting Pointer of Task Queue Array */
    int thread_count;         /* Number of threads */
    int queue_size;           /* Task queue length */
    int head;                 /* Current task queue head */
    int tail;                 /* End of current task queue */
    int count;                /* Number of tasks currently to be run */
    int shutdown;             /* Is the current state of the thread pool closed? */
    int started;              /* Number of threads running */
};

typedef enum
{
    immediate_shutdown = 1,
    graceful_shutdown = 2
} threadpool_shutdown_t;

threadpool_t *threadpool_create(int thread_count, int queue_size, int flags);
int threadpool_add(threadpool_t *pool, void (*routine)(void *), void *arg, int flags);
int threadpool_destroy(threadpool_t *pool, int flags);

#endif