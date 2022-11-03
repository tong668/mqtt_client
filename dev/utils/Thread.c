/*******************************************************************************
 * Copyright (c) 2009, 2021 IBM Corp. and Ian Craggs
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    https://www.eclipse.org/legal/epl-2.0/
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial implementation
 *    Ian Craggs, Allan Stockdill-Mander - async client updates
 *    Ian Craggs - bug #415042 - start Linux thread as disconnected
 *    Ian Craggs - fix for bug #420851
 *    Ian Craggs - change MacOS semaphore implementation
 *    Ian Craggs - fix for clock #284
 *******************************************************************************/

/**
 * @file
 * \brief Threading related functions
 *
 * Used to create platform independent threading functions
 */


#include "Thread.h"
#include "Log.h"

#undef malloc
#undef realloc
#undef free

#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>

/**
 * Start a new thread
 * @param fn the function to run, must be of the correct signature
 * @param parameter pointer to the function parameter, can be NULL
 */
void Thread_start(thread_fn fn, void *parameter) {
    thread_type thread = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attr, fn, parameter) != 0)
        thread = 0;
    pthread_attr_destroy(&attr);

}
/**
 * Create a new mutex
 * @param rc return code: 0 for success, negative otherwise
 * @return the new mutex
 */
mutex_type Thread_create_mutex(int *rc) {
    mutex_type mutex = NULL;
    *rc = -1;

    mutex = malloc(sizeof(pthread_mutex_t));
    if (mutex)
        *rc = pthread_mutex_init(mutex, NULL);

    return mutex;
}

/**
 * Lock a mutex which has alrea
 * @return completion code, 0 is success
 */
int Thread_lock_mutex(mutex_type mutex) {
    int rc = -1;
    rc = pthread_mutex_lock(mutex);
    return rc;
}

/**
 * Unlock a mutex which has already been locked
 * @param mutex the mutex
 * @return completion code, 0 is success
 */
int Thread_unlock_mutex(mutex_type mutex) {
    int rc = -1;
    rc = pthread_mutex_unlock(mutex);
    return rc;
}

/**
 * Destroy a mutex which has already been created
 * @param mutex the mutex
 */
int Thread_destroy_mutex(mutex_type mutex) {
    int rc = 0;
    rc = pthread_mutex_destroy(mutex);
    free(mutex);
    return rc;
}


/**
 * Get the thread id of the thread from which this function is called
 * @return thread id, type varying according to OS
 */
pthread_t Thread_getid(void) {
    return pthread_self();
}

/**
 * Create a new semaphore
 * @param rc return code: 0 for success, negative otherwise
 * @return the new condition variable
 */
sem_type Thread_create_sem(int *rc) {
    sem_type sem = NULL;

    *rc = -1;
    sem = malloc(sizeof(sem_t));
    if (sem)
        *rc = sem_init(sem, 0, 0);

    return sem;
}


/**
 * Wait for a semaphore to be posted, or timeout.
 * @param sem the semaphore
 * @param timeout the maximum time to wait, in milliseconds
 * @return completion code
 */
int Thread_wait_sem(sem_type sem, int timeout) {
/* sem_timedwait is the obvious call to use, but seemed not to work on the Viper,
 * so I've used trywait in a loop instead. Ian Craggs 23/7/2010
 */
    int rc = -1;
    int i = 0;
    useconds_t interval = 10000; /* 10000 microseconds: 10 milliseconds */
    int count = (1000 * timeout) / interval; /* how many intervals in timeout period */
    while (++i < count && (rc = sem_trywait(sem)) != 0) {
        if (rc == -1 && ((rc = errno) != EAGAIN)) {
            rc = 0;
            break;
        }
        usleep(interval); /* microseconds - .1 of a second */
    }
    return rc;
}

/**
 * Check to see if a semaphore has been posted, without waiting
 * The semaphore will be unchanged, if the return value is false.
 * The semaphore will have been decremented, if the return value is true.
 * @param sem the semaphore
 * @return 0 (false) or 1 (true)
 */
int Thread_check_sem(sem_type sem) {

    /* If the call was unsuccessful, the state of the semaphore shall be unchanged,
     * and the function shall return a value of -1 */
    return sem_trywait(sem) == 0;

}

/**
 * Post a semaphore
 * @param sem the semaphore
 * @return 0 on success
 */
int Thread_post_sem(sem_type sem) {
    int rc = 0;
    int val;
    int rc1 = sem_getvalue(sem, &val);
    if (rc1 != 0)
        rc = errno;
    else if (val == 0 && sem_post(sem) == -1)
        rc = errno;
    return rc;
}

/**
 * Destroy a semaphore which has already been created
 * @param sem the semaphore
 */
int Thread_destroy_sem(sem_type sem) {
    int rc = 0;
    rc = sem_destroy(sem);
    free(sem);
    return rc;
}

/**
 * Create a new condition variable
 * @return the condition variable struct
 */
cond_type Thread_create_cond(int *rc) {
    cond_type condvar = NULL;
    pthread_condattr_t attr;
    *rc = -1;
    pthread_condattr_init(&attr);
    condvar = malloc(sizeof(cond_type_struct));
    if (condvar) {
        *rc = pthread_cond_init(&condvar->cond, &attr);
        *rc = pthread_mutex_init(&condvar->mutex, NULL);
    }
    return condvar;
}

/**
 * Signal a condition variable
 * @return completion code
 */
int Thread_signal_cond(cond_type condvar) {
    int rc = 0;
    pthread_mutex_lock(&condvar->mutex);
    rc = pthread_cond_signal(&condvar->cond);
    pthread_mutex_unlock(&condvar->mutex);
    return rc;
}

/**
 * Wait with a timeout (ms) for condition variable
 * @return 0 for success, ETIMEDOUT otherwise
 */
int Thread_wait_cond(cond_type condvar, int timeout_ms) {
    int rc = 0;
    struct timespec cond_timeout;
    struct timespec interval;

    interval.tv_sec = timeout_ms / 1000;
    interval.tv_nsec = (timeout_ms % 1000) * 1000000L;
    clock_gettime(CLOCK_REALTIME, &cond_timeout);
    cond_timeout.tv_sec += interval.tv_sec;
    cond_timeout.tv_nsec += (timeout_ms % 1000) * 1000000L;

    if (cond_timeout.tv_nsec >= 1000000000L) {
        cond_timeout.tv_sec++;
        cond_timeout.tv_nsec += (cond_timeout.tv_nsec - 1000000000L);
    }

    pthread_mutex_lock(&condvar->mutex);
    rc = pthread_cond_timedwait(&condvar->cond, &condvar->mutex, &cond_timeout);
    pthread_mutex_unlock(&condvar->mutex);
    return rc;
}

/**
 * Destroy a condition variable
 * @return completion code
 */
int Thread_destroy_cond(cond_type condvar) {
    int rc = 0;
    rc = pthread_mutex_destroy(&condvar->mutex);
    rc = pthread_cond_destroy(&condvar->cond);
    free(condvar);

    return rc;
}

