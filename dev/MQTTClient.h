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

LIBMQTT_API int MQTTClient_setCallbacks(MQTTClient handle, void *context, MQTTClient_connectionLost *cl,
                                        MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc);

LIBMQTT_API int MQTTClient_create(MQTTClient *handle, const char *serverURI, const char *clientId,
                                  int persistence_type, void *persistence_context);

LIBMQTT_API int MQTTClient_createWithOptions(MQTTClient *handle, const char *serverURI, const char *clientId,
                                             int persistence_type, void *persistence_context,
                                             MQTTClient_createOptions *options);


LIBMQTT_API MQTTClient_nameValue *MQTTClient_getVersionInfo(void);


LIBMQTT_API int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions *options);

LIBMQTT_API int MQTTClient_disconnect(MQTTClient handle, int timeout);

LIBMQTT_API int MQTTClient_subscribe(MQTTClient handle, const char *topic, int qos);

LIBMQTT_API MQTTResponse MQTTClient_subscribe5(MQTTClient handle, const char *topic, int qos,
                                               MQTTSubscribe_options *opts, MQTTProperties *props);

LIBMQTT_API MQTTResponse MQTTClient_subscribeMany5(MQTTClient handle, int count, char *const *topic,
                                                   int *qos, MQTTSubscribe_options *opts, MQTTProperties *props);

LIBMQTT_API int MQTTClient_unsubscribe(MQTTClient handle, const char *topic);

LIBMQTT_API MQTTResponse MQTTClient_unsubscribe5(MQTTClient handle, const char *topic, MQTTProperties *props);

LIBMQTT_API MQTTResponse
MQTTClient_unsubscribeMany5(MQTTClient handle, int count, char *const *topic, MQTTProperties *props);

LIBMQTT_API MQTTResponse
MQTTClient_publish5(MQTTClient handle, const char *topicName, int payloadlen, const void *payload,
                    int qos, int retained, MQTTProperties *properties, MQTTClient_deliveryToken *dt);

LIBMQTT_API int MQTTClient_publishMessage(MQTTClient handle, const char *topicName, MQTTClient_message *msg,
                                          MQTTClient_deliveryToken *dt);


LIBMQTT_API MQTTResponse MQTTClient_publishMessage5(MQTTClient handle, const char *topicName, MQTTClient_message *msg,
                                                    MQTTClient_deliveryToken *dt);

LIBMQTT_API int MQTTClient_waitForCompletion(MQTTClient handle, MQTTClient_deliveryToken dt, unsigned long timeout);


LIBMQTT_API void MQTTClient_yield(void);


LIBMQTT_API void MQTTClient_freeMessage(MQTTClient_message **msg);


LIBMQTT_API void MQTTClient_free(void *ptr);

LIBMQTT_API void MQTTClient_destroy(MQTTClient *handle);

#if defined(__cplusplus)
}
#endif

#endif //MQTT_CLIENT_MQTTCLIENT_H
