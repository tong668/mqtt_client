//
// Created by Administrator on 2022/11/2.
//

#ifndef MQTT_CLIENT_MQTTCLIENT_H
#define MQTT_CLIENT_MQTTCLIENT_H

#include "TypeDefine.h"
#include "Socket.h"
#include "Thread.h"
#include "Log.h"
#include "mqttpacket.h"
//#include "clients.h"

/**
 * Return code: No error. Indicates successful completion of an MQTT client
 * operation.
 */
#define MQTTCLIENT_SUCCESS 0
/**
 * Return code: A generic error code indicating the failure of an MQTT client
 * operation.
 */
#define MQTTCLIENT_FAILURE -1

/* error code -2 is MQTTCLIENT_PERSISTENCE_ERROR */

/**
 * Return code: The client is disconnected.
 */
#define MQTTCLIENT_DISCONNECTED -3
/**
 * Return code: The maximum number of messages allowed to be simultaneously
 * in-flight has been reached.
 */
#define MQTTCLIENT_MAX_MESSAGES_INFLIGHT -4
/**
 * Return code: An invalid UTF-8 string has been detected.
 */
#define MQTTCLIENT_BAD_UTF8_STRING -5
/**
 * Return code: A NULL parameter has been supplied when this is invalid.
 */
#define MQTTCLIENT_NULL_PARAMETER -6
/**
 * Return code: The topic has been truncated (the topic string includes
 * embedded NULL characters). String functions will not access the full topic.
 * Use the topic length value to access the full topic.
 */
#define MQTTCLIENT_TOPICNAME_TRUNCATED -7
/**
 * Return code: A structure parameter does not have the correct eyecatcher
 * and version number.
 */
#define MQTTCLIENT_BAD_STRUCTURE -8
/**
 * Return code: A QoS value that falls outside of the acceptable range (0,1,2)
 */
#define MQTTCLIENT_BAD_QOS -9
/**
 * Return code: Attempting SSL connection using non-SSL version of library
 */
#define MQTTCLIENT_SSL_NOT_SUPPORTED -10
/**
 * Return code: unrecognized MQTT version
 */
#define MQTTCLIENT_BAD_MQTT_VERSION -11
/**
 * Return code: protocol prefix in serverURI should be tcp://, ssl://, ws:// or wss://
 * The TLS enabled prefixes (ssl, wss) are only valid if a TLS version of the library
 * is linked with.
 */
#define MQTTCLIENT_BAD_PROTOCOL -14
/**
 * Return code: option not applicable to the requested version of MQTT
 */
#define MQTTCLIENT_BAD_MQTT_OPTION -15
/**
 * Return code: call not applicable to the requested version of MQTT
 */
#define MQTTCLIENT_WRONG_MQTT_VERSION -16
/**
 * Return code: 0 length will topic on connect
 */
#define MQTTCLIENT_0_LEN_WILL_TOPIC -17


/**
 * Default MQTT version to connect with.  Use 3.1.1 then fall back to 3.1
 */
#define MQTTVERSION_DEFAULT 0
/**
 * MQTT version to connect with: 3.1
 */
#define MQTTVERSION_3_1 3
/**
 * MQTT version to connect with: 3.1.1
 */
#define MQTTVERSION_3_1_1 4
/**
 * MQTT version to connect with: 5
 */
#define MQTTVERSION_5 5
/**
 * Bad return code from subscribe, as defined in the 3.1.1 specification
 */
#define MQTT_BAD_SUBSCRIBE 0x80

typedef void* MQTTClient;

typedef struct
{
    const char* name;
    const char* value;
} MQTTClient_nameValue;

typedef struct
{
    /** The eyecatcher for this structure.  must be MQTW. */
    char struct_id[4];
    /** The version number of this structure.  Must be 0 or 1
           0 means there is no binary payload option
     */
    int struct_version;
    /** The LWT topic to which the LWT message will be published. */
    const char* topicName;
    /** The LWT payload in string form. */
    const char* message;
    /**
     * The retained flag for the LWT message (see MQTTClient_message.retained).
     */
    int retained;
    /**
     * The quality of service setting for the LWT message (see
     * MQTTClient_message.qos and @ref qos).
     */
    int qos;
    /** The LWT payload in binary form. This is only checked and used if the message option is NULL */
    struct
    {
        int len;            /**< binary payload length */
        const void* data;  /**< binary payload data */
    } payload;
} MQTTClient_willOptions;


typedef struct
{
    char struct_id[4];


    int struct_version;

    const char* trustStore;

    const char* keyStore;

    const char* privateKey;

    const char* privateKeyPassword;


    const char* enabledCipherSuites;


    int enableServerCertAuth;

    int sslVersion;

    int verify;

    const char* CApath;

    int (*ssl_error_cb) (const char *str, size_t len, void *u);

    void* ssl_error_context;

    unsigned int (*ssl_psk_cb) (const char *hint, char *identity, unsigned int max_identity_len, unsigned char *psk, unsigned int max_psk_len, void *u);

    void* ssl_psk_context;

    int disableDefaultTrustStore;

    const unsigned char *protos;

    unsigned int protos_len;
} MQTTClient_SSLOptions;


typedef struct
{

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

    MQTTClient_SSLOptions* ssl;

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

    const MQTTClient_nameValue* httpHeaders;

    const char* httpProxy;
    /**
     * HTTPS proxy
     */
    const char* httpsProxy;
} MQTTClient_connectOptions;

typedef struct
{
    /** The eyecatcher for this structure.  must be MQCO. */
    char struct_id[4];
    /** The version number of this structure.  Must be 0 */
    int struct_version;
    int MQTTVersion;
} MQTTClient_createOptions;

typedef void MQTTClient_connectionLost(void* context, char* cause);
typedef void MQTTClient_deliveryComplete(void* context, MQTTClient_deliveryToken dt);
typedef int MQTTClient_messageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message);
typedef void MQTTClient_disconnected(void* context, MQTTProperties* properties,
                                     enum MQTTReasonCodes reasonCode);
typedef void MQTTClient_published(void* context, int dt, int packet_type, MQTTProperties* properties,
                                  enum MQTTReasonCodes reasonCode);

typedef struct {
    char *serverURI;
    const char *currentServerURI; /* when using HA options, set the currently used serverURI */
    int websocket;
    Clients *c;
    MQTTClient_connectionLost *cl;
    MQTTClient_messageArrived *ma;
    MQTTClient_deliveryComplete *dc;
    void *context;

    MQTTClient_disconnected *disconnected;
    void *disconnected_context; /* the context to be associated with the disconnected callback*/

    MQTTClient_published *published;
    void *published_context; /* the context to be associated with the disconnected callback*/
    sem_type connect_sem;
    int rc; /* getsockopt return code in connect */
    sem_type connack_sem;
    sem_type suback_sem;
    sem_type unsuback_sem;
    MQTTPacket *pack;

    unsigned long commandTimeout;
} MQTTClients;


#define MQTTClient_connectOptions_initializer { {'M', 'Q', 'T', 'C'}, 8, 60, 1, 1, NULL, NULL, NULL, 30, 0, NULL,\
0, NULL, MQTTVERSION_DEFAULT, {NULL, 0, 0}, {0, NULL}, -1, 0, NULL, NULL, NULL}

LIBMQTT_API int MQTTClient_create(MQTTClient* handle, const char* serverURI, const char* clientId,
                                  int persistence_type, void* persistence_context);

LIBMQTT_API int MQTTClient_createWithOptions(MQTTClient* handle, const char* serverURI, const char* clientId,
                                             int persistence_type, void* persistence_context, MQTTClient_createOptions* options);

#endif //MQTT_CLIENT_MQTTCLIENT_H
