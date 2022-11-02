/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp.
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
 *******************************************************************************/
#if !defined(_MUTEX_TYPE_H_)
#define _MUTEX_TYPE_H_


#include <pthread.h>
#include <stdlib.h>
#include <bits/stdint-uintn.h>

#define BUILD_TIMESTAMP "2022-11-01T01:05:37Z"
#define CLIENT_VERSION  "1.3.10"

#define LIBMQTT_API extern

#define mutex_type pthread_mutex_t*

#if defined(HIGH_PERFORMANCE)
#define NO_HEAP_TRACKING 1
#endif
#define PAHO_MEMORY_ERROR -99

#if defined(HIGH_PERFORMANCE)
#define NOSTACKTRACE 1
#endif

#if defined(NOSTACKTRACE)
#define FUNC_ENTRY
#define FUNC_EXIT
#define FUNC_EXIT_RC(x)

#endif

//todo remove
/**
  * This <i>persistence_type</i> value specifies the default file system-based
  * persistence mechanism (see MQTTClient_create()).
  */
#define MQTTCLIENT_PERSISTENCE_DEFAULT 0
/**
  * This <i>persistence_type</i> value specifies a memory-based
  * persistence mechanism (see MQTTClient_create()).
  */
#define MQTTCLIENT_PERSISTENCE_NONE 1
/**
  * This <i>persistence_type</i> value specifies an application-specific
  * persistence mechanism (see MQTTClient_create()).
  */
#define MQTTCLIENT_PERSISTENCE_USER 2

/**
  * Application-specific persistence functions must return this error code if
  * there is a problem executing the function.
  */
#define MQTTCLIENT_PERSISTENCE_ERROR -2

/** The MQTT V5 one byte reason code */
enum MQTTReasonCodes {
    MQTTREASONCODE_SUCCESS = 0,

};

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


/** The MQTT V5 subscribe options, apart from QoS which existed before V5. */
typedef struct MQTTSubscribe_options
{
//    /** The eyecatcher for this structure. Must be MQSO. */
//    char struct_id[4];
//    /** The version number of this structure.  Must be 0.
//     */
//    int struct_version;
//    /** To not receive our own publications, set to 1.
//     *  0 is the original MQTT behaviour - all messages matching the subscription are received.
//     */
//    unsigned char noLocal;
//    /** To keep the retain flag as on the original publish message, set to 1.
//     *  If 0, defaults to the original MQTT behaviour where the retain flag is only set on
//     *  publications sent by a broker if in response to a subscribe request.
//     */
//    unsigned char retainAsPublished;
//    /** 0 - send retained messages at the time of the subscribe (original MQTT behaviour)
//     *  1 - send retained messages on subscribe only if the subscription is new
//     *  2 - do not send retained messages at all
//     */
//    unsigned char retainHandling;
} MQTTSubscribe_options;

//#define MQTTSubscribe_options_initializer { {'M', 'Q', 'S', 'O'}, 0, 0, 0, 0 }

#define MQTTCLIENT_SUCCESS 0

#define MQTTCLIENT_FAILURE -1

#define MQTTCLIENT_DISCONNECTED -3

#define MQTTCLIENT_MAX_MESSAGES_INFLIGHT -4

#define MQTTCLIENT_BAD_UTF8_STRING -5

#define MQTTCLIENT_NULL_PARAMETER -6

#define MQTTCLIENT_TOPICNAME_TRUNCATED -7

#define MQTTCLIENT_BAD_STRUCTURE -8

#define MQTTCLIENT_BAD_QOS -9

#define MQTTCLIENT_SSL_NOT_SUPPORTED -10

#define MQTTCLIENT_BAD_MQTT_VERSION -11

#define MQTTCLIENT_BAD_PROTOCOL -14

#define MQTTCLIENT_BAD_MQTT_OPTION -15

#define MQTTCLIENT_WRONG_MQTT_VERSION -16

#define MQTTCLIENT_0_LEN_WILL_TOPIC -17

#define MQTTVERSION_3_1_1 4

#define MQTT_BAD_SUBSCRIBE 0x80

typedef void* MQTTClient;

typedef int MQTTClient_deliveryToken;

/**
 * Stored publication data to minimize copying
 */
typedef struct
{
    char *topic;
    int topiclen;
    char* payload;
    int payloadlen;
    int refcount;
    uint8_t mask[4];
} Publications;

/**
 * Client will message data
 */
typedef struct
{
    char *topic;
    int payloadlen;
    void *payload;
    int retained;
    int qos;
} willMessages;

typedef struct
{
    /** The eyecatcher for this structure.  must be MQCO. */
    char struct_id[4];
    /** The version number of this structure.  Must be 0 */
    int struct_version;

    int MQTTVersion;
} MQTTClient_createOptions;

typedef struct
{
    char struct_id[4];
    int struct_version;
    const char* topicName;
    const char* message;
    int retained;
    int qos;
    struct
    {
        int len;            /**< binary payload length */
        const void* data;  /**< binary payload data */
    } payload;
} MQTTClient_willOptions;


typedef struct
{
    const char* name;
    const char* value;
} MQTTClient_nameValue;


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



#endif /* _MUTEX_TYPE_H_ */
