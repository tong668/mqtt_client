//
// Created by Administrator on 2022/11/3.
//

#ifndef MQTT_CLIENT_MQTTCLIENT_H
#define MQTT_CLIENT_MQTTCLIENT_H

#include <stdio.h>
#include "utils/TypeDefine.h"
#include "MQTTProperties.h"

extern MQTTClient_nameValue *MQTTClient_getVersionInfo(void);

extern int MQTTClient_setCallbacks(MQTTClient handle, void *context, MQTTClient_connectionLost *cl,
                                   MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc);

extern int MQTTClient_create(MQTTClient *handle, const char *serverURI, const char *clientId);

extern int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions *options);

extern int MQTTClient_publishMessage(MQTTClient handle, const char *topicName, MQTTClient_message *msg,
                                     MQTTClient_deliveryToken *dt);

extern int MQTTClient_subscribe(MQTTClient handle, const char *topic, int qos);


extern void MQTTClient_destroy(MQTTClient *handle);
extern MQTTResponse MQTTClient_publishMessage5(MQTTClient handle, const char *topicName, MQTTClient_message *msg,
                                               MQTTClient_deliveryToken *dt);
extern MQTTResponse
MQTTClient_publish5(MQTTClient handle, const char *topicName, int payloadlen, const void *payload, int qos,
                    int retained, MQTTClient_deliveryToken *deliveryToken);



#endif //MQTT_CLIENT_MQTTCLIENT_H
