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
#define FUNC_ENTRY_NOLOG
#define FUNC_ENTRY_MED
#define FUNC_ENTRY_MAX
#define FUNC_EXIT
#define FUNC_EXIT_NOLOG
#define FUNC_EXIT_MED
#define FUNC_EXIT_MAX
#define FUNC_EXIT_RC(x)
#define FUNC_EXIT_MED_RC(x)
#define FUNC_EXIT_MAX_RC(x)
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
    MQTTREASONCODE_NORMAL_DISCONNECTION = 0,
    MQTTREASONCODE_GRANTED_QOS_0 = 0,
    MQTTREASONCODE_GRANTED_QOS_1 = 1,
    MQTTREASONCODE_GRANTED_QOS_2 = 2,
    MQTTREASONCODE_DISCONNECT_WITH_WILL_MESSAGE = 4,
    MQTTREASONCODE_NO_MATCHING_SUBSCRIBERS = 16,
    MQTTREASONCODE_NO_SUBSCRIPTION_FOUND = 17,
    MQTTREASONCODE_CONTINUE_AUTHENTICATION = 24,
    MQTTREASONCODE_RE_AUTHENTICATE = 25,
    MQTTREASONCODE_UNSPECIFIED_ERROR = 128,
    MQTTREASONCODE_MALFORMED_PACKET = 129,
    MQTTREASONCODE_PROTOCOL_ERROR = 130,
    MQTTREASONCODE_IMPLEMENTATION_SPECIFIC_ERROR = 131,
    MQTTREASONCODE_UNSUPPORTED_PROTOCOL_VERSION = 132,
    MQTTREASONCODE_CLIENT_IDENTIFIER_NOT_VALID = 133,
    MQTTREASONCODE_BAD_USER_NAME_OR_PASSWORD = 134,
    MQTTREASONCODE_NOT_AUTHORIZED = 135,
    MQTTREASONCODE_SERVER_UNAVAILABLE = 136,
    MQTTREASONCODE_SERVER_BUSY = 137,
    MQTTREASONCODE_BANNED = 138,
    MQTTREASONCODE_SERVER_SHUTTING_DOWN = 139,
    MQTTREASONCODE_BAD_AUTHENTICATION_METHOD = 140,
    MQTTREASONCODE_KEEP_ALIVE_TIMEOUT = 141,
    MQTTREASONCODE_SESSION_TAKEN_OVER = 142,
    MQTTREASONCODE_TOPIC_FILTER_INVALID = 143,
    MQTTREASONCODE_TOPIC_NAME_INVALID = 144,
    MQTTREASONCODE_PACKET_IDENTIFIER_IN_USE = 145,
    MQTTREASONCODE_PACKET_IDENTIFIER_NOT_FOUND = 146,
    MQTTREASONCODE_RECEIVE_MAXIMUM_EXCEEDED = 147,
    MQTTREASONCODE_TOPIC_ALIAS_INVALID = 148,
    MQTTREASONCODE_PACKET_TOO_LARGE = 149,
    MQTTREASONCODE_MESSAGE_RATE_TOO_HIGH = 150,
    MQTTREASONCODE_QUOTA_EXCEEDED = 151,
    MQTTREASONCODE_ADMINISTRATIVE_ACTION = 152,
    MQTTREASONCODE_PAYLOAD_FORMAT_INVALID = 153,
    MQTTREASONCODE_RETAIN_NOT_SUPPORTED = 154,
    MQTTREASONCODE_QOS_NOT_SUPPORTED = 155,
    MQTTREASONCODE_USE_ANOTHER_SERVER = 156,
    MQTTREASONCODE_SERVER_MOVED = 157,
    MQTTREASONCODE_SHARED_SUBSCRIPTIONS_NOT_SUPPORTED = 158,
    MQTTREASONCODE_CONNECTION_RATE_EXCEEDED = 159,
    MQTTREASONCODE_MAXIMUM_CONNECT_TIME = 160,
    MQTTREASONCODE_SUBSCRIPTION_IDENTIFIERS_NOT_SUPPORTED = 161,
    MQTTREASONCODE_WILDCARD_SUBSCRIPTIONS_NOT_SUPPORTED = 162
};

/** The MQTT V5 subscribe options, apart from QoS which existed before V5. */
typedef struct MQTTSubscribe_options
{
    /** The eyecatcher for this structure. Must be MQSO. */
    char struct_id[4];
    /** The version number of this structure.  Must be 0.
     */
    int struct_version;
    /** To not receive our own publications, set to 1.
     *  0 is the original MQTT behaviour - all messages matching the subscription are received.
     */
    unsigned char noLocal;
    /** To keep the retain flag as on the original publish message, set to 1.
     *  If 0, defaults to the original MQTT behaviour where the retain flag is only set on
     *  publications sent by a broker if in response to a subscribe request.
     */
    unsigned char retainAsPublished;
    /** 0 - send retained messages at the time of the subscribe (original MQTT behaviour)
     *  1 - send retained messages on subscribe only if the subscription is new
     *  2 - do not send retained messages at all
     */
    unsigned char retainHandling;
} MQTTSubscribe_options;

#define MQTTSubscribe_options_initializer { {'M', 'Q', 'S', 'O'}, 0, 0, 0, 0 }


#endif /* _MUTEX_TYPE_H_ */
