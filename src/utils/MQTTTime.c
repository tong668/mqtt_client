/*******************************************************************************
 * Copyright (c) 2020, 2021 IBM Corp. and Ian Craggs
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
 *******************************************************************************/

#include "MQTTTime.h"
#include "TypeDefine.h"

#include <unistd.h>
#include <sys/time.h>

void MQTTTime_sleep(uint64_t milliseconds)
{
	usleep((useconds_t)(milliseconds*1000));
}

struct timeval MQTTTime_start_clock(void)
{
	struct timeval start;
	struct timespec start_ts;

	clock_gettime(CLOCK_MONOTONIC, &start_ts);
	start.tv_sec = start_ts.tv_sec;
	start.tv_usec = start_ts.tv_nsec / 1000;
	return start;
}
struct timeval MQTTTime_now(void)
{
	return MQTTTime_start_clock();
}

int64_t MQTTTime_difftime(struct timeval t_new, struct timeval t_old)
{
	struct timeval result;

	timersub(&t_new, &t_old, &result);
	return (int64_t)(((int64_t)result.tv_sec)*1000 + ((int64_t)result.tv_usec)/1000); /* convert to milliseconds */
}

uint64_t MQTTTime_elapsed(struct timeval milliseconds)
{
	return (uint64_t)MQTTTime_difftime(MQTTTime_now(), milliseconds);
}
