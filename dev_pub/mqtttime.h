//
// Created by Administrator on 2022/11/2.
//

#ifndef MQTT_CLIENT_MQTTTIME_H
#define MQTT_CLIENT_MQTTTIME_H

#include <stdint.h>
#include <sys/time.h>
#define START_TIME_TYPE struct timeval
#define START_TIME_ZERO {0, 0}

#define ELAPSED_TIME_TYPE uint64_t
#define DIFF_TIME_TYPE int64_t

void MQTTTime_sleep(ELAPSED_TIME_TYPE milliseconds);
START_TIME_TYPE MQTTTime_start_clock(void);
START_TIME_TYPE MQTTTime_now(void);
ELAPSED_TIME_TYPE MQTTTime_elapsed(START_TIME_TYPE milliseconds);
DIFF_TIME_TYPE MQTTTime_difftime(START_TIME_TYPE t_new, START_TIME_TYPE t_old);


#endif //MQTT_CLIENT_MQTTTIME_H
