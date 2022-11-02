
#if !defined(MQTTPROPERTIES_H)
#define MQTTPROPERTIES_H

#include "utils/TypeDefine.h"

#define MQTT_INVALID_PROPERTY_ID -2

/** The one byte MQTT V5 property indicator */
enum MQTTPropertyCodes {
  MQTTPROPERTY_CODE_PAYLOAD_FORMAT_INDICATOR = 1,  /**< The value is 1 */
  MQTTPROPERTY_CODE_MESSAGE_EXPIRY_INTERVAL = 2,   /**< The value is 2 */
  MQTTPROPERTY_CODE_CONTENT_TYPE = 3,              /**< The value is 3 */
  MQTTPROPERTY_CODE_RESPONSE_TOPIC = 8,            /**< The value is 8 */
  MQTTPROPERTY_CODE_CORRELATION_DATA = 9,          /**< The value is 9 */
  MQTTPROPERTY_CODE_SUBSCRIPTION_IDENTIFIER = 11,  /**< The value is 11 */
  MQTTPROPERTY_CODE_SESSION_EXPIRY_INTERVAL = 17,  /**< The value is 17 */
  MQTTPROPERTY_CODE_ASSIGNED_CLIENT_IDENTIFER = 18,/**< The value is 18 */
  MQTTPROPERTY_CODE_SERVER_KEEP_ALIVE = 19,        /**< The value is 19 */
  MQTTPROPERTY_CODE_AUTHENTICATION_METHOD = 21,    /**< The value is 21 */
  MQTTPROPERTY_CODE_AUTHENTICATION_DATA = 22,      /**< The value is 22 */
  MQTTPROPERTY_CODE_REQUEST_PROBLEM_INFORMATION = 23,/**< The value is 23 */
  MQTTPROPERTY_CODE_WILL_DELAY_INTERVAL = 24,      /**< The value is 24 */
  MQTTPROPERTY_CODE_REQUEST_RESPONSE_INFORMATION = 25,/**< The value is 25 */
  MQTTPROPERTY_CODE_RESPONSE_INFORMATION = 26,     /**< The value is 26 */
  MQTTPROPERTY_CODE_SERVER_REFERENCE = 28,         /**< The value is 28 */
  MQTTPROPERTY_CODE_REASON_STRING = 31,            /**< The value is 31 */
  MQTTPROPERTY_CODE_RECEIVE_MAXIMUM = 33,          /**< The value is 33*/
  MQTTPROPERTY_CODE_TOPIC_ALIAS_MAXIMUM = 34,      /**< The value is 34 */
  MQTTPROPERTY_CODE_TOPIC_ALIAS = 35,              /**< The value is 35 */
  MQTTPROPERTY_CODE_MAXIMUM_QOS = 36,              /**< The value is 36 */
  MQTTPROPERTY_CODE_RETAIN_AVAILABLE = 37,         /**< The value is 37 */
  MQTTPROPERTY_CODE_USER_PROPERTY = 38,            /**< The value is 38 */
  MQTTPROPERTY_CODE_MAXIMUM_PACKET_SIZE = 39,      /**< The value is 39 */
  MQTTPROPERTY_CODE_WILDCARD_SUBSCRIPTION_AVAILABLE = 40,/**< The value is 40 */
  MQTTPROPERTY_CODE_SUBSCRIPTION_IDENTIFIERS_AVAILABLE = 41,/**< The value is 41 */
  MQTTPROPERTY_CODE_SHARED_SUBSCRIPTION_AVAILABLE = 42/**< The value is 241 */
};

/** The one byte MQTT V5 property type */
enum MQTTPropertyTypes {
  MQTTPROPERTY_TYPE_BYTE,
  MQTTPROPERTY_TYPE_TWO_BYTE_INTEGER,
  MQTTPROPERTY_TYPE_FOUR_BYTE_INTEGER,
  MQTTPROPERTY_TYPE_VARIABLE_BYTE_INTEGER,
  MQTTPROPERTY_TYPE_BINARY_DATA,
  MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING,
  MQTTPROPERTY_TYPE_UTF_8_STRING_PAIR
};


LIBMQTT_API int MQTTProperty_getType(enum MQTTPropertyCodes value);


typedef struct
{
	int len; /**< the length of the string */
	char* data; /**< pointer to the string data */
} MQTTLenString;


typedef struct
{
  enum MQTTPropertyCodes identifier; /**<  The MQTT V5 property id. A multi-byte integer. */
  /** The value of the property, as a union of the different possible types. */
  union {
    unsigned char byte;       /**< holds the value of a byte property type */
    unsigned short integer2;  /**< holds the value of a 2 byte integer property type */
    unsigned int integer4;    /**< holds the value of a 4 byte integer property type */
    struct {
      MQTTLenString data;  /**< The value of a string property, or the name of a user property. */
      MQTTLenString value; /**< The value of a user property. */
    };
  } value;
} MQTTProperty;

/**
 * MQTT version 5 property list
 */
typedef struct MQTTProperties
{
  int count;     /**< number of property entries in the array */
  int max_count; /**< max number of properties that the currently allocated array can store */
  int length;    /**< mbi: byte length of all properties */
  MQTTProperty *array;  /**< array of properties */
} MQTTProperties;

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
