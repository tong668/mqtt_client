//
// Created by didi on 2022/10/27.
//

#include "Thread_.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>

/**
 * Start a new thread
 * @param fn the function to run, must be of the correct signature
 * @param parameter pointer to the function parameter, can be NULL
 */
void Thread_start_(thread_fn fn, void *parameter) {
    pthread_t thread = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attr, fn, parameter) != 0)
        thread = 0;
    pthread_attr_destroy(&attr);
}

/**
 * Get the thread id of the thread from which this function is called
 * @return thread id, type varying according to OS
 */
pthread_t Thread_getid_(void) {
    return pthread_self();
}

/**
 * Create a new mutex
 * @param rc return code: 0 for success, negative otherwise
 * @return the new mutex
 */
pthread_mutex_t *Thread_create_mutex_(int *rc) {
    pthread_mutex_t *mutex = NULL;
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
int Thread_lock_mutex_(pthread_mutex_t *mutex) {
    int rc = -1;

    /* don't add entry/exit trace points as the stack log uses mutexes - recursion beckons */

    rc = pthread_mutex_lock(mutex);

    return rc;
}


/**
 * Unlock a mutex which has already been locked
 * @param mutex the mutex
 * @return completion code, 0 is success
 */
int Thread_unlock_mutex_(pthread_mutex_t *mutex) {
    int rc = -1;
    /* don't add entry/exit trace points as the stack log uses mutexes - recursion beckons */
    rc = pthread_mutex_unlock(mutex);
    return rc;
}

/**
 * Destroy a mutex which has already been created
 * @param mutex the mutex
 */
int Thread_destroy_mutex_(pthread_mutex_t *mutex) {
    int rc = 0;
    rc = pthread_mutex_destroy(mutex);
    free(mutex);
    return rc;
}

/**
 * Create a new condition variable
 * @return the condition variable struct
 */
cond_type_struct *Thread_create_cond_(int *rc) {
    cond_type_struct *condvar = NULL;
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
int Thread_signal_cond_(cond_type_struct *condvar) {
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
int Thread_wait_cond_(cond_type_struct *condvar, int timeout_ms) {
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
int Thread_destroy_cond_(cond_type_struct *condvar) {
    int rc = 0;
    rc = pthread_mutex_destroy(&condvar->mutex);
    rc = pthread_cond_destroy(&condvar->cond);
    free(condvar);
    return rc;
}


/**
 * Create a new semaphore
 * @param rc return code: 0 for success, negative otherwise
 * @return the new condition variable
 */
sem_t *Thread_create_sem_(int *rc) {
    sem_t *sem = NULL;

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
int Thread_wait_sem_(sem_t *sem, int timeout) {
/* sem_timedwait is the obvious call to use, but seemed not to work on the Viper,
 * so I've used trywait in a loop instead. Ian Craggs 23/7/2010
 */
    int rc = -1;
#define USE_TRYWAIT
#if defined(USE_TRYWAIT)
    int i = 0;
    useconds_t interval = 10000; /* 10000 microseconds: 10 milliseconds */
    int count = (1000 * timeout) / interval; /* how many intervals in timeout period */
#endif

#if defined(USE_TRYWAIT)
    while (++i < count && (rc = sem_trywait(sem)) != 0) {
        if (rc == -1 && ((rc = errno) != EAGAIN)) {
            rc = 0;
            break;
        }
        usleep(interval); /* microseconds - .1 of a second */
    }
#endif
    return rc;
}

/**
 * Check to see if a semaphore has been posted, without waiting
 * The semaphore will be unchanged, if the return value is false.
 * The semaphore will have been decremented, if the return value is true.
 * @param sem the semaphore
 * @return 0 (false) or 1 (true)
 */
int Thread_check_sem_(sem_t *sem) {
    /* If the call was unsuccessful, the state of the semaphore shall be unchanged,
     * and the function shall return a value of -1 */
    return sem_trywait(sem) == 0;
}


/**
 * Post a semaphore
 * @param sem the semaphore
 * @return 0 on success
 */
int Thread_post_sem_(sem_t *sem) {
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
int Thread_destroy_sem_(sem_t *sem) {
    int rc = 0;

    rc = sem_destroy(sem);
    free(sem);
    return rc;
}

#define THREAD_UNIT_TESTS_
#if defined(THREAD_UNIT_TESTS_)
// unit test
struct timeval start_clock_(void)
{
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    return start_time;
}

long elapsed_(struct timeval start_time)
{
    struct timeval now, res;

    gettimeofday(&now, NULL);
    timersub(&now, &start_time, &res);
    return (res.tv_sec)*1000 + (res.tv_usec)/1000;
}


int tests_ = 0, failures_ = 0;

void myassert_(char* filename, int lineno, char* description, int value, char* format, ...)
{
    ++tests_;
    if (!value)
    {
        va_list args;

        ++failures_;
        printf("Assertion failed, file %s, line %d, description: %s\n", filename, lineno, description);

        va_start(args, format);
        vprintf(format, args);
        va_end(args);

        //cur_output += sprintf(cur_output, "<failure type=\"%s\">file %s, line %d </failure>\n",
        //                description, filename, lineno);
    }
    else
        printf("Assertion succeeded, file %s, line %d, description: %s\n", filename, lineno, description);
}

#define assert(a, b, c, d) myassert_(__FILE__, __LINE__, a, b, c, d)
#define assert1(a, b, c, d, e) myassert_(__FILE__, __LINE__, a, b, c, d, e)


void * cond_secondary_(void* n)
{
    int rc = 0;
    cond_type_struct * cond = n;

    printf("This should return immediately as it was posted already\n");
    rc = Thread_wait_cond_(cond, 99999);
    assert("rc 1 from wait_cond", rc == 1, "rc was %d", rc);

    printf("This should hang around a few seconds\n");
    rc = Thread_wait_cond_(cond, 99999);
    assert("rc 1 from wait_cond", rc == 1, "rc was %d", rc);

    printf("Secondary cond thread ending\n");
    return 0;
}

int cond_test_()
{
    int rc = 0;
    cond_type_struct * cond = Thread_create_cond_(&rc);

    printf("Post secondary so it should return immediately\n");
    rc = Thread_signal_cond_(cond);
    assert("rc 0 from signal cond", rc == 0, "rc was %d", rc);

    printf("Starting secondary thread\n");
    Thread_start_(cond_secondary_, (void*)cond);

    sleep(3);

    printf("post secondary\n");
    rc = Thread_signal_cond_(cond);
    assert("rc 1 from signal cond", rc == 1, "rc was %d", rc);

    sleep(3);

    printf("Main thread ending\n");

    return failures_;
}

void* sem_secondary_(void* n)
{
    int rc = 0;
    sem_t* sem = n;

    printf("Secondary semaphore pointer %p\n", sem);

    rc = Thread_check_sem_(sem);
    assert("rc 1 from check_sem", rc == 1, "rc was %d", rc);

    printf("Secondary thread about to wait\n");
    rc = Thread_wait_sem_(sem, 99999);
    printf("Secondary thread returned from wait %d\n", rc);

    printf("Secondary thread about to wait\n");
    rc = Thread_wait_sem_(sem, 99999);
    printf("Secondary thread returned from wait %d\n", rc);
    printf("Secondary check sem %d\n", Thread_check_sem_(sem));

    printf("Secondary thread ending\n");
    return 0;
}

int sem_test_()
{
    int rc = 0;
    sem_t* sem = Thread_create_sem_(&rc);

    printf("Primary semaphore pointer %p\n", sem);

    rc = Thread_check_sem_(sem);
    assert("rc 0 from check_sem", rc == 0, "rc was %d\n", rc);

    printf("post secondary so then check should be 1\n");
    rc = Thread_post_sem_(sem);
    assert("rc 0 from post_sem", rc == 0, "rc was %d\n", rc);

    rc = Thread_check_sem_(sem);
    assert("rc 1 from check_sem", rc == 1, "rc was %d", rc);

    printf("Starting secondary thread\n");
    Thread_start_(sem_secondary_, (void*)sem);

    sleep(3);
    rc = Thread_check_sem_(sem);
    assert("rc 1 from check_sem", rc == 1, "rc was %d", rc);

    printf("post secondary\n");
    rc = Thread_post_sem_(sem);
    assert("rc 1 from post_sem", rc == 1, "rc was %d", rc);

    sleep(3);

    printf("Main thread ending\n");

    return failures_;

}

int main (int argc, char *argv[]){
    return 0;
}
#endif