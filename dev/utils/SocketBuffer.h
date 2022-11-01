/*******************************************************************************
 * Copyright (c) 2009, 2022 IBM Corp., Ian Craggs and others
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
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Ian Craggs, Allan Stockdill-Mander - SSL updates
 *******************************************************************************/

#if !defined(SOCKETBUFFER_H)
#define SOCKETBUFFER_H

#include "Socket.h"

typedef struct iovec iobuf;
typedef struct {
    SOCKET socket;
    unsigned int index;
    size_t headerlen;
    char fixed_header[5];    /**< header plus up to 4 length bytes */
    size_t buflen,            /**< total length of the buffer */
    datalen;            /**< current length of data in buf */
    char *buf;
} socket_queue;

typedef struct {
    SOCKET socket;
    int count;
    size_t total;
    size_t bytes;
    iobuf iovecs[5];
    int frees[5];
} pending_writes;

#define SOCKETBUFFER_COMPLETE 0
#if !defined(SOCKET_ERROR)
#define SOCKET_ERROR -1
#endif
#define SOCKETBUFFER_INTERRUPTED -22 /* must be the same value as TCPSOCKET_INTERRUPTED */

int SocketBuffer_initialize(void);

void SocketBuffer_terminate(void);

void SocketBuffer_cleanup(SOCKET socket);

char *SocketBuffer_getQueuedData(SOCKET socket, size_t bytes, size_t *actual_len);

int SocketBuffer_getQueuedChar(SOCKET socket, char *c);

void SocketBuffer_interrupted(SOCKET socket, size_t actual_len);

char *SocketBuffer_complete(SOCKET socket);

void SocketBuffer_queueChar(SOCKET socket, char c);

int SocketBuffer_pendingWrite(SOCKET socket, int count, iobuf *iovecs, int *frees, size_t total, size_t bytes);

pending_writes *SocketBuffer_getWrite(SOCKET socket);

int SocketBuffer_writeComplete(SOCKET socket);

pending_writes *SocketBuffer_updateWrite(SOCKET socket, char *topic, char *payload);

#endif
