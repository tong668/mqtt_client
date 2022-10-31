//
// Created by didi on 2022/10/27.
//

#ifndef ECLIPSE_PAHO_C_THREAD__H
#define ECLIPSE_PAHO_C_THREAD__H

#include <pthread.h>
#include <semaphore.h>

typedef void *(*thread_fn)(void *);

typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} cond_type_struct;


void Thread_start_(thread_fn, void *);

pthread_t Thread_getid_();

pthread_mutex_t *Thread_create_mutex_(int *);

int Thread_lock_mutex_(pthread_mutex_t *);

int Thread_unlock_mutex_(pthread_mutex_t *);

int Thread_destroy_mutex_(pthread_mutex_t *);


cond_type_struct *Thread_create_cond_(int *);

int Thread_signal_cond_(cond_type_struct *);

int Thread_wait_cond_(cond_type_struct *condvar, int timeout);

int Thread_destroy_cond_(cond_type_struct *);

sem_t *Thread_create_sem_(int *);

int Thread_wait_sem_(sem_t *sem, int timeout);

int Thread_check_sem_(sem_t *sem);

int Thread_post_sem_(sem_t *sem);

int Thread_destroy_sem_(sem_t *sem);

#endif //ECLIPSE_PAHO_C_THREAD__H
