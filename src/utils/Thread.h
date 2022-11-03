/*******************************************************************************
 * Copyright (c) 2009, 2020 IBM Corp.
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
 *    Ian Craggs - fix for bug #420851
 *    Ian Craggs - change MacOS semaphore implementation
 *******************************************************************************/

#if !defined(THREAD_H)
#define THREAD_H

#include "TypeDefine.h"
#include <pthread.h>
#include <semaphore.h>

typedef void*(*thread_fn)(void *);

typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} cond_type_struct;
typedef cond_type_struct *cond_type;

cond_type Thread_create_cond(int *);

int Thread_signal_cond(cond_type);

int Thread_wait_cond(cond_type condvar, int timeout);

int Thread_destroy_cond(cond_type);

extern void Thread_start(thread_fn, void *);

extern pthread_mutex_t* Thread_create_mutex(int *);

extern int Thread_lock_mutex(pthread_mutex_t*);

extern int Thread_unlock_mutex(pthread_mutex_t*);

int Thread_destroy_mutex(pthread_mutex_t*);

extern pthread_t Thread_getid();

sem_t* Thread_create_sem(int *);

int Thread_wait_sem(sem_t* sem, int timeout);

int Thread_check_sem(sem_t* sem);

int Thread_post_sem(sem_t* sem);

int Thread_destroy_sem(sem_t* sem);

#endif
