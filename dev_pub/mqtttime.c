//
// Created by Administrator on 2022/11/2.
//

#include "mqtttime.h"
#include "TypeDefine.h"

#include <unistd.h>
#include <sys/time.h>

void MQTTTime_sleep(ELAPSED_TIME_TYPE milliseconds)
{
    usleep((useconds_t)(milliseconds*1000));
}

START_TIME_TYPE MQTTTime_start_clock(void)
{
    struct timeval start;
    struct timespec start_ts;

    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    start.tv_sec = start_ts.tv_sec;
    start.tv_usec = start_ts.tv_nsec / 1000;
    return start;
}
START_TIME_TYPE MQTTTime_now(void)
{
    return MQTTTime_start_clock();
}

DIFF_TIME_TYPE MQTTTime_difftime(START_TIME_TYPE t_new, START_TIME_TYPE t_old)
{
    struct timeval result;

    timersub(&t_new, &t_old, &result);
    return (DIFF_TIME_TYPE)(((DIFF_TIME_TYPE)result.tv_sec)*1000 + ((DIFF_TIME_TYPE)result.tv_usec)/1000); /* convert to milliseconds */
}

ELAPSED_TIME_TYPE MQTTTime_elapsed(START_TIME_TYPE milliseconds)
{
    return (ELAPSED_TIME_TYPE)MQTTTime_difftime(MQTTTime_now(), milliseconds);
}


