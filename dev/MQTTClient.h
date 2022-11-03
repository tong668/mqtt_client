//
// Created by Administrator on 2022/11/3.
//

#ifndef MQTT_CLIENT_MQTTCLIENT_H
#define MQTT_CLIENT_MQTTCLIENT_H


#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include "utils/TypeDefine.h"
#include "MQTTProperties.h"


extern int MQTTClient_create(MQTTClient *handle, const char *serverURI, const char *clientId,
                                  int persistence_type, void *persistence_context);

extern int MQTTClient_createWithOptions(MQTTClient *handle, const char *serverURI, const char *clientId,
                                             int persistence_type, void *persistence_context,
                                             MQTTClient_createOptions *options);


extern MQTTClient_nameValue *MQTTClient_getVersionInfo(void);



extern void MQTTClient_yield(void);


#if defined(__cplusplus)
}
#endif

#endif //MQTT_CLIENT_MQTTCLIENT_H
