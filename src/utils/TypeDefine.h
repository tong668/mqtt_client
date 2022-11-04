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

#include "LinkedList.h"
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>

#define BUILD_TIMESTAMP "2022-11-01T01:05:37Z"
#define CLIENT_VERSION  "1.3.10"

#define SOCKET int
#define SOCKETBUFFER_COMPLETE 0
#define SOCKETBUFFER_INTERRUPTED -22 /* must be the same value as TCPSOCKET_INTERRUPTED */

#define PAHO_MEMORY_ERROR -99

#if defined(HIGH_PERFORMANCE)
#define NOSTACKTRACE 1
#endif

#if defined(NOSTACKTRACE)
#define FUNC_ENTRY
#define FUNC_EXIT
#define FUNC_EXIT_RC(x)
#endif

#define URI_TCP "tcp://"

#define MQTT_INVALID_PROPERTY_ID -2

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

enum msgTypes
{
    CONNECT = 1, CONNACK, PUBLISH, PUBACK, PUBREC, PUBREL,
    PUBCOMP, SUBSCRIBE, SUBACK, UNSUBSCRIBE, UNSUBACK,
    PINGREQ, PINGRESP, DISCONNECT, AUTH
};


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

/* connection states */
/** no connection in progress, see connected value */
#define NOT_IN_PROGRESS  0x0
/** TCP connection in progress */
#define TCP_IN_PROGRESS  0x1
/** SSL connection in progress */
#define SSL_IN_PROGRESS  0x2
/** Websocket connection in progress */
#define WEBSOCKET_IN_PROGRESS   0x3
/** TCP completed, waiting for MQTT ACK */
#define WAIT_FOR_CONNACK 0x4
/** Proxy connection in progress */
#define PROXY_CONNECT_IN_PROGRESS 0x5
/** Disconnecting */
#define DISCONNECTING    -2

#if !defined(min)
#define min(A,B) ( (A) < (B) ? (A):(B))
#endif
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))


#define MAX_MSG_ID 65535

#define MQTT_DEFAULT_PORT 1883
#define SECURE_MQTT_DEFAULT_PORT 8883
#define WS_DEFAULT_PORT 80
#define WSS_DEFAULT_PORT 443
#define PROXY_DEFAULT_PORT 8080


/**
 * WebSocket op codes
 * @{
 */
#define WebSocket_OP_CONTINUE 0x0 /* 0000 - continue frame */
#define WebSocket_OP_TEXT     0x1 /* 0001 - text frame */
#define WebSocket_OP_BINARY   0x2 /* 0010 - binary frame */
#define WebSocket_OP_CLOSE    0x8 /* 1000 - close frame */
#define WebSocket_OP_PING     0x9 /* 1001 - ping frame */
#define WebSocket_OP_PONG     0xA /* 1010 - pong frame */
/** @} */

/**
 * Various close status codes
 * @{
 */
#define WebSocket_CLOSE_NORMAL          1000
#define WebSocket_CLOSE_GOING_AWAY      1001
#define WebSocket_CLOSE_TLS_FAIL        1015 /* reserved: not be used */
/** @} */

#define HTTP_PROTOCOL(x) x ? "https" : "http"


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
    u_int8_t mask[4];
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


typedef struct
{
    /** The eyecatcher for this structure.  must be MQTM. */
    char struct_id[4];
    /** The version number of this structure.  Must be 0 or 1
     *  0 indicates no message properties */
    int struct_version;
    /** The length of the MQTT message payload in bytes. */
    int payloadlen;
    /** A pointer to the payload of the MQTT message. */
    void* payload;

    int qos;

    int retained;

    int dup;

    int msgid;

    MQTTProperties properties;
} MQTTClient_message;

#define MQTTClient_message_initializer { {'M', 'Q', 'T', 'M'}, 1, 0, NULL, 0, 0, 0, 0, MQTTProperties_initializer }

typedef struct
{
    /** The eyecatcher for this structure.  must be MQTC. */
    char struct_id[4];
    int struct_version;
    int keepAliveInterval;
    int cleansession;

    int reliable;
    MQTTClient_willOptions* will;
    const char* username;
    const char* password;
    int connectTimeout;
    int retryInterval;
    int serverURIcount;
    char* const* serverURIs;
    int MQTTVersion;
    struct
    {
        const char* serverURI;     /**< the serverURI connected to */
        int MQTTVersion;     /**< the MQTT version used to connect with */
        int sessionPresent;  /**< if the MQTT version is 3.1.1, the value of sessionPresent returned in the connack */
    } returned;
    struct
    {
        int len;           /**< binary password length */
        const void* data;  /**< binary password data */
    } binarypwd;
    int maxInflightMessages;
    /*
     * MQTT V5 clean start flag.  Only clears state at the beginning of the session.
     */
    int cleanstart;
    /**
     * HTTP headers for websockets
     */
    const MQTTClient_nameValue* httpHeaders;
    /**
     * HTTP proxy
     */
    const char* httpProxy;
    /**
     * HTTPS proxy
     */
    const char* httpsProxy;
} MQTTClient_connectOptions;

#define MQTTClient_connectOptions_initializer { {'M', 'Q', 'T', 'C'}, 8, 60, 1, 1, NULL, NULL, NULL, 30, 0, NULL,\
0, NULL, MQTTVERSION_3_1_1, {NULL, 0, 0}, {0, NULL}, -1, 0, NULL, NULL, NULL}

/** MQTT version 5.0 response information */
typedef struct MQTTResponse
{
    int version;                        /* the version number of this structure */
    enum MQTTReasonCodes reasonCode;    /* the MQTT 5.0 reason code returned */
    int reasonCodeCount;	            /* the number of reason codes.  Used for subscribeMany5 and unsubscribeMany5 */
    enum MQTTReasonCodes* reasonCodes;  /* a list of reason codes.  Used for subscribeMany5 and unsubscribeMany5 */
    MQTTProperties* properties;         /* optionally, the MQTT 5.0 properties returned */
} MQTTResponse;

typedef int MQTTClient_messageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message);


typedef void MQTTClient_deliveryComplete(void* context, MQTTClient_deliveryToken dt);


typedef void MQTTClient_connectionLost(void* context, char* cause);

/**
 * Client publication message data
 */
typedef struct
{
    int qos;
    int retain;
    int msgid;
    int MQTTVersion;
    MQTTProperties properties;
    Publications *publish;
    struct timeval lastTouch;		    /**> used for retry and expiry */
    char nextMessageType;	/**> PUBREC, PUBREL, PUBCOMP */
    int len;				/**> length of the whole structure+data */
} Messages;


typedef struct
{
    SOCKET socket;
    struct timeval lastSent;
    struct timeval lastReceived;
    struct timeval lastPing;
    char *http_proxy;
    char *http_proxy_auth;
    int websocket; /**< socket has been upgraded to use web sockets */
    char *websocket_key;
    const MQTTClient_nameValue* httpHeaders;
} networkHandles;


typedef unsigned int bool;
typedef void* (*pf)(int, unsigned char, char*, size_t);
/**
 * Bitfields for the MQTT header byte.
 */
typedef union
{
    /*unsigned*/ char byte;	/**< the whole byte */

    struct
    {
        bool retain : 1;		/**< retained flag bit */
        unsigned int qos : 2;	/**< QoS value, 0, 1 or 2 */
        bool dup : 1;			/**< DUP flag bit */
        unsigned int type : 4;	/**< message type nibble */
    } bits;

} Header;


/**
 * Data for a connect packet.
 */
typedef struct
{
    Header header;	/**< MQTT header byte */
    union
    {
        unsigned char all;	/**< all connect flags */
        struct
        {
            int : 1;	/**< unused */
            bool cleanstart : 1;	/**< cleansession flag */
            bool will : 1;			/**< will flag */
            unsigned int willQoS : 2;	/**< will QoS value */
            bool willRetain : 1;		/**< will retain setting */
            bool password : 1; 			/**< 3.1 password */
            bool username : 1;			/**< 3.1 user name */
        } bits;
    } flags;	/**< connect flags byte */

    char *Protocol, /**< MQTT protocol name */
    *clientID,	/**< string client id */
    *willTopic,	/**< will topic */
    *willMsg;	/**< will payload */

    int keepAliveTimer;		/**< keepalive timeout value in seconds */
    unsigned char version;	/**< MQTT version number */
} Connect;


/**
 * Data for a connack packet.
 */
typedef struct
{
    Header header; /**< MQTT header byte */
    union
    {
        unsigned char all;	/**< all connack flags */
        struct
        {
            bool sessionPresent : 1;    /**< was a session found on the server? */
            unsigned int reserved : 7;	/**< message type nibble */
        } bits;
    } flags;	 /**< connack flags byte */
    unsigned char rc; /**< connack reason code */
    unsigned int MQTTVersion;  /**< the version of MQTT */
    MQTTProperties properties; /**< MQTT 5.0 properties.  Not used for MQTT < 5.0 */
} Connack;


/**
 * Data for a packet with header only.
 */
typedef struct
{
    Header header;	/**< MQTT header byte */
} MQTTPacket;


/**
 * Data for a publish packet.
 */
typedef struct
{
    Header header;	/**< MQTT header byte */
    char* topic;	/**< topic string */
    int topiclen;
    int msgId;		/**< MQTT message id */
    char* payload;	/**< binary payload, length delimited */
    int payloadlen;	/**< payload length */
    int MQTTVersion;  /**< the version of MQTT */
    MQTTProperties properties; /**< MQTT 5.0 properties.  Not used for MQTT < 5.0 */
    u_int8_t mask[4]; /**< the websockets mask the payload is masked with, if any */
} Publish;


/**
 * Data for one of the ack packets.
 */
typedef struct
{
    Header header;	/**< MQTT header byte */
    int msgId;		/**< MQTT message id */
    unsigned char rc; /**< MQTT 5 reason code */
    int MQTTVersion;  /**< the version of MQTT */
    MQTTProperties properties; /**< MQTT 5.0 properties.  Not used for MQTT < 5.0 */
} Ack;

typedef Ack Puback;
typedef Ack Pubrec;
typedef Ack Pubrel;
typedef Ack Pubcomp;

/**
 * Data for a suback packet.
 */
typedef struct
{
    Header header;	/**< MQTT header byte */
    int msgId;		/**< MQTT message id */
    int MQTTVersion;  /**< the version of MQTT */
    MQTTProperties properties; /**< MQTT 5.0 properties.  Not used for MQTT < 5.0 */
    List* qoss;		/**< list of granted QoSs (MQTT 3/4) / reason codes (MQTT 5) */
} Suback;

typedef struct
{
    SOCKET socket;
    Publications* p;
} pending_write;


typedef struct
{
    List publications;
    unsigned int msgs_received;
    unsigned int msgs_sent;
    List pending_writes; /* for qos 0 writes not complete */
} MQTTProtocol;

#define MQTTProperties_initializer {0, 0, 0, NULL} //todo

/**
 * Configuration data related to all clients
 */
typedef struct
{
    const char* version;
    List* clients;
} ClientStates;

/**
 * Data related to one client
 */
typedef struct
{
    char* clientID;					      /**< the string id of the client */
    const char* username;					/**< MQTT v3.1 user name */
    int passwordlen;              /**< MQTT password length */
    const void* password;					/**< MQTT v3.1 binary password */
    unsigned int cleansession : 1;	/**< MQTT V3 clean session flag */
    unsigned int cleanstart : 1;		/**< MQTT V5 clean start flag */
    unsigned int connected : 1;		/**< whether it is currently connected */
    unsigned int good : 1; 			  /**< if we have an error on the socket we turn this off */
    unsigned int ping_outstanding : 1;
    unsigned int ping_due : 1;      /**< we couldn't send a ping so we should send one when we can */
    signed int connect_state : 4;
    struct timeval ping_due_time;  /**< the time at which the ping should have been sent (ping_due) */
    networkHandles net;             /**< network info for this client */
    int msgID;                      /**< the MQTT message id */
    int keepAliveInterval;          /**< the MQTT keep alive interval */
    int retryInterval;              /**< the MQTT retry interval for QoS > 0 */
    int maxInflightMessages;        /**< the max number of inflight outbound messages we allow */
    willMessages* will;             /**< the MQTT will message, if any */
    List* inboundMsgs;              /**< inbound in flight messages */
    List* outboundMsgs;				/**< outbound in flight messages */
    int connect_count;              /**< the number of outbound messages on reconnect - to ensure we send them all */
    int connect_sent;               /**< the current number of outbound messages on reconnect that we've sent */
    List* messageQueue;             /**< inbound complete but undelivered messages */
    List* outboundQueue;            /**< outbound queued messages */
    unsigned int qentry_seqno;
    void* phandle;                  /**< the persistence handle */
    void* beforeWrite_context;      /**< context to be used with the persistence beforeWrite callbacks */
    void* afterRead_context;        /**< context to be used with the persistence afterRead callback */
    void* context;                  /**< calling context - used when calling disconnect_internal */
    int MQTTVersion;                /**< the version of MQTT being used, 3, 4 or 5 */
    int sessionExpiry;              /**< MQTT 5 session expiry */
    char* httpProxy;                /**< HTTP proxy */
    char* httpsProxy;               /**< HTTPS proxy */
} Clients;

#define MQTTResponse_initializer {1, MQTTREASONCODE_SUCCESS, 0, NULL, NULL}

typedef void MQTTClient_disconnected(void* context, MQTTProperties* properties,
                                     enum MQTTReasonCodes reasonCode);

typedef void MQTTClient_published(void* context, int dt, int packet_type, MQTTProperties* properties,
                                  enum MQTTReasonCodes reasonCode);


typedef struct {
    MQTTClient_message *msg;
    char *topicName;
    int topicLen;
    unsigned int seqno; /* only used on restore */
} qEntry;


/** @brief raw uuid type */
typedef unsigned char uuid_t[16];

typedef struct {
    char *serverURI;
    const char *currentServerURI; /* when using HA options, set the currently used serverURI */
    int websocket;
    Clients *c;
//    MQTTClient_connectionLost *cl;
    MQTTClient_messageArrived *ma;
    MQTTClient_deliveryComplete *dc;
    void *context;

    MQTTClient_disconnected *disconnected;
    void *disconnected_context; /* the context to be associated with the disconnected callback*/

    MQTTClient_published *published;
    void *published_context; /* the context to be associated with the disconnected callback*/
    sem_t *connect_sem;
    int rc; /* getsockopt return code in connect */
    sem_t *connack_sem;
    sem_t *suback_sem;
//    sem_t *unsuback_sem;
    MQTTPacket *pack;

    unsigned long commandTimeout;
} MQTTClients;





#endif /* _MUTEX_TYPE_H_ */
