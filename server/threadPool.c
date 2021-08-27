#define THREAD 10
#define QUEUE 256

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include "threadPool.h"

static void *threadpool_thread(void *threadpool);
int threadpool_free(threadpool_t *pool);

threadpool_t *threadpool_create(int thread_count, int queue_size, int flags)
{

    if (thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE)
    {
        return NULL;
    }

    threadpool_t *pool;
    int i;

    /* Request memory to create memory pool objects */
    if ((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL)
    {
        goto err;
    }

    /* Init*/
    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;

    /* Allocate thread and task queue */
    /* Memory required to request thread arrays and task queues */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);

    /* Initialize mutex and conditional variable first */
    /* Initialize mutexes and conditional variables */
    if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
        (pthread_mutex_init(&(pool->logLock), NULL) != 0) ||
        (pthread_cond_init(&(pool->notify), NULL) != 0) ||
        (pool->threads == NULL) ||
        (pool->queue == NULL))
    {
        goto err;
    }

    /* Start worker threads */
    /* Create a specified number of threads to start running */
    for (i = 0; i < thread_count; i++)
    {
        if (pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void *)pool) != 0)
        {
            threadpool_destroy(pool, 0);
            return NULL;
        }
        pool->thread_count++;
        pool->started++;
    }
    return pool;

err:
    if (pool)
    {
        threadpool_free(pool);
    }
    return NULL;
}

int threadpool_add(threadpool_t *pool, void (*function)(void *), void *argument, int flags)
{
    int err = 0;
    int next;

    if (pool == NULL || function == NULL)
    {
        printf("pool or function null");

        return threadpool_invalid;
    }

    /* Mutual exclusion lock ownership must be acquired first */
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {

        return threadpool_lock_failure;
    }

    /* Calculate the next location where task can be stored */
    next = pool->tail + 1;
    next = (next == pool->queue_size) ? 0 : next;

    do
    {
        /* Are we full ? */
        /* Check if the task queue is full */
        if (pool->count == pool->queue_size)
        {
            printf("task queue full");
            err = threadpool_queue_full;
            break;
        }

        /* Are we shutting down ? */
        /* Check whether the current thread pool state is closed */
        if (pool->shutdown)
        {
            printf("shutdown");
            err = threadpool_shutdown;
            break;
        }

        /* Add task to queue */
        /* Place function pointers and parameters in tail to add to the task queue */
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;
        /* Update tail and count */
        pool->tail = next;
        pool->count += 1;

        /* pthread_cond_broadcast */
        // Send out signal to indicate that task has been added
        if (pthread_cond_signal(&(pool->notify)) != 0)
        {
            printf("task queue empty");
            err = threadpool_lock_failure;
            break;
        }
    } while (0);

    /* Release mutex resources */
    if (pthread_mutex_unlock(&pool->lock) != 0)
        err = threadpool_lock_failure;

    return err;
}

int threadpool_destroy(threadpool_t *pool, int flags)
{
    int i, err = 0;

    if (pool == NULL)
    {
        return threadpool_invalid;
    }

    /* Get mutex resources */
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return threadpool_lock_failure;
    }

    do
    {
        /* Already shutting down */
        /* Determine whether it has been closed elsewhere */
        if (pool->shutdown)
        {

            err = threadpool_shutdown;
            break;
        }

        /* Gets the specified closing mode */
        pool->shutdown = (flags & threadpool_graceful) ? graceful_shutdown : immediate_shutdown;

        /* Wake up all worker threads */
        /* Wake up all threads blocked by dependent variables and release mutexes */

        if ((pthread_cond_broadcast(&(pool->notify)) != 0) ||
            (pthread_mutex_unlock(&(pool->lock)) != 0))
        {

            err = threadpool_lock_failure;
            break;
        }

        /* Join all worker thread */
        /* Waiting for all threads to end */
        for (i = 0; i < pool->thread_count; i++)
        {
            if (pthread_join(pool->threads[i], NULL) != 0)
            {

                err = threadpool_thread_failure;
            }
        }
    } while (0);

    if (!err)
    {
        /* Release memory resources */

        threadpool_free(pool);
    }
    return err;
}

int threadpool_free(threadpool_t *pool)
{
    if (pool == NULL || pool->started > 0)
    {
        return -1;
    }

    if (pool->threads)
    {
        free(pool->threads);
        free(pool->queue);

        //Destroy the locks
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_mutex_lock(&(pool->logLock));
        pthread_mutex_destroy(&(pool->logLock));

        pthread_cond_destroy(&(pool->notify));
    }

    free(pool);
    return 0;
}

static void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;

    while (1)
    {
        /* Get mutex resources */
        pthread_mutex_lock(&(pool->lock));

        while ((pool->count == 0) && (!pool->shutdown))
        {
            //If the pool is active but there aren't any taks, wait for a task to be added OR for the pool to be shutted down
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        /* Closing Processing */
        if ((pool->shutdown == immediate_shutdown) ||
            ((pool->shutdown == graceful_shutdown) &&
             (pool->count == 0)))
        {
            break;
        }

        /* Grab our task */
        /* Get the first task in the task queue */
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;
        /* Update head and count */
        pool->head += 1;
        pool->head = (pool->head == pool->queue_size) ? 0 : pool->head;
        pool->count -= 1;

        /* Unlock */
        /* Release mutex */
        pthread_mutex_unlock(&(pool->lock));

        /* Get to work */
        /* Start running tasks */
        (*(task.function))(task.argument);
        /* Here is the end of a task run */
    }

    /* Threads will end, update the number of running threads */

    pool->started--;

    pthread_mutex_unlock(&(pool->lock));

    pthread_exit(NULL);
    return (NULL);
}