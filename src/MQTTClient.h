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

extern int MQTTClient_setCallbacks(MQTTClient handle, void *context, MQTTClient_connectionLost *cl,
                                        MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc);

extern int MQTTClient_create(MQTTClient *handle, const char *serverURI, const char *clientId);

extern int MQTTClient_createWithOptions(MQTTClient *handle, const char *serverURI, const char *clientId,
                                             MQTTClient_createOptions *options);


extern MQTTClient_nameValue *MQTTClient_getVersionInfo(void);


extern int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions *options);


extern int MQTTClient_subscribe(MQTTClient handle, const char *topic, int qos);

extern MQTTResponse MQTTClient_subscribe5(MQTTClient handle, const char *topic, int qos,
                                               MQTTSubscribe_options *opts, MQTTProperties *props);

extern MQTTResponse MQTTClient_subscribeMany5(MQTTClient handle, int count, char *const *topic,
                                                   int *qos, MQTTSubscribe_options *opts, MQTTProperties *props);

extern int MQTTClient_unsubscribe(MQTTClient handle, const char *topic);

extern MQTTResponse MQTTClient_unsubscribe5(MQTTClient handle, const char *topic, MQTTProperties *props);

extern MQTTResponse
MQTTClient_unsubscribeMany5(MQTTClient handle, int count, char *const *topic, MQTTProperties *props);

extern MQTTResponse
MQTTClient_publish5(MQTTClient handle, const char *topicName, int payloadlen, const void *payload,
                    int qos, int retained, MQTTProperties *properties, MQTTClient_deliveryToken *dt);

extern int MQTTClient_publishMessage(MQTTClient handle, const char *topicName, MQTTClient_message *msg,
                                          MQTTClient_deliveryToken *dt);


extern MQTTResponse MQTTClient_publishMessage5(MQTTClient handle, const char *topicName, MQTTClient_message *msg,
                                                    MQTTClient_deliveryToken *dt);


extern void MQTTClient_freeMessage(MQTTClient_message **msg);


extern void MQTTClient_free(void *ptr);

extern void MQTTClient_destroy(MQTTClient *handle);

#if defined(__cplusplus)
}
#endif

#endif //MQTT_CLIENT_MQTTCLIENT_H
