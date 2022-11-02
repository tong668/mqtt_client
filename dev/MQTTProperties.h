
#if !defined(MQTTPROPERTIES_H)
#define MQTTPROPERTIES_H

#include "utils/TypeDefine.h"

#define MQTT_INVALID_PROPERTY_ID -2



LIBMQTT_API int MQTTProperty_getType(enum MQTTPropertyCodes value);





#define MQTTProperties_initializer {0, 0, 0, NULL}


int MQTTProperties_len(MQTTProperties* props);


LIBMQTT_API int MQTTProperties_add(MQTTProperties* props, const MQTTProperty* prop);


int MQTTProperties_write(char** pptr, const MQTTProperties* properties);



LIBMQTT_API void MQTTProperties_free(MQTTProperties* properties);


LIBMQTT_API MQTTProperties MQTTProperties_copy(const MQTTProperties* props);


LIBMQTT_API int MQTTProperties_hasProperty(MQTTProperties *props, enum MQTTPropertyCodes propid);

LIBMQTT_API int MQTTProperties_getNumericValue(MQTTProperties *props, enum MQTTPropertyCodes propid);


LIBMQTT_API int MQTTProperties_getNumericValueAt(MQTTProperties *props, enum MQTTPropertyCodes propid, int index);


#endif /* MQTTPROPERTIES_H */
