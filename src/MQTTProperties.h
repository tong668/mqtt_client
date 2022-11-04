//
// Created by Administrator on 2022/11/3.
//

#ifndef MQTT_CLIENT_MQTTPROPERTIES_H
#define MQTT_CLIENT_MQTTPROPERTIES_H

#include "utils/TypeDefine.h"

extern int MQTTProperty_getType(enum MQTTPropertyCodes value);

int MQTTProperties_len(MQTTProperties *props);

extern int MQTTProperties_add(MQTTProperties *props, const MQTTProperty *prop);

int MQTTProperties_write(char **pptr, const MQTTProperties *properties);

extern void MQTTProperties_free(MQTTProperties *properties);

extern MQTTProperties MQTTProperties_copy(const MQTTProperties *props);

int MQTTProperties_socketCompare(void *a, void *b);


#endif //MQTT_CLIENT_MQTTPROPERTIES_H
