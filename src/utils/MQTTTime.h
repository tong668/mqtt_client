/*******************************************************************************
 * Copyright (c) 2020 IBM Corp.
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

#if !defined(MQTTTIME_H)
#define MQTTTIME_H

#include <stdint.h>
#include <sys/time.h>
#include "TypeDefine.h"

void MQTTTime_sleep(uint64_t milliseconds);
struct timeval MQTTTime_start_clock(void);
struct timeval MQTTTime_now(void);
uint64_t MQTTTime_elapsed(struct timeval milliseconds);
int64_t MQTTTime_difftime(struct timeval t_new, struct timeval t_old);

#endif
