
#if !defined(MQTTCLIENT_H)
#define MQTTCLIENT_H

#if defined(__cplusplus)
 extern "C" {
#endif

#include <stdio.h>
/*
/// @endcond
*/

#include "utils/TypeDefine.h"
#include "MQTTProperties.h"


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

typedef int MQTTClient_messageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message);


typedef void MQTTClient_deliveryComplete(void* context, MQTTClient_deliveryToken dt);


typedef void MQTTClient_connectionLost(void* context, char* cause);


LIBMQTT_API int MQTTClient_setCallbacks(MQTTClient handle, void* context, MQTTClient_connectionLost* cl,
									MQTTClient_messageArrived* ma, MQTTClient_deliveryComplete* dc);


typedef void MQTTClient_disconnected(void* context, MQTTProperties* properties,
		enum MQTTReasonCodes reasonCode);

typedef void MQTTClient_published(void* context, int dt, int packet_type, MQTTProperties* properties,
		enum MQTTReasonCodes reasonCode);

LIBMQTT_API int MQTTClient_create(MQTTClient* handle, const char* serverURI, const char* clientId,
		int persistence_type, void* persistence_context);

typedef struct
{
	/** The eyecatcher for this structure.  must be MQCO. */
	char struct_id[4];
	/** The version number of this structure.  Must be 0 */
	int struct_version;

	int MQTTVersion;
} MQTTClient_createOptions;

LIBMQTT_API int MQTTClient_createWithOptions(MQTTClient* handle, const char* serverURI, const char* clientId,
		int persistence_type, void* persistence_context, MQTTClient_createOptions* options);

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


LIBMQTT_API MQTTClient_nameValue* MQTTClient_getVersionInfo(void);

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
//todo
LIBMQTT_API int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions* options);

/** MQTT version 5.0 response information */
typedef struct MQTTResponse
{
	int version;                        /* the version number of this structure */
	enum MQTTReasonCodes reasonCode;    /* the MQTT 5.0 reason code returned */
	int reasonCodeCount;	            /* the number of reason codes.  Used for subscribeMany5 and unsubscribeMany5 */
	enum MQTTReasonCodes* reasonCodes;  /* a list of reason codes.  Used for subscribeMany5 and unsubscribeMany5 */
	MQTTProperties* properties;         /* optionally, the MQTT 5.0 properties returned */
} MQTTResponse;

#define MQTTResponse_initializer {1, MQTTREASONCODE_SUCCESS, 0, NULL, NULL}

LIBMQTT_API int MQTTClient_disconnect(MQTTClient handle, int timeout);

LIBMQTT_API int MQTTClient_subscribe(MQTTClient handle, const char* topic, int qos);

LIBMQTT_API MQTTResponse MQTTClient_subscribe5(MQTTClient handle, const char* topic, int qos,
		MQTTSubscribe_options* opts, MQTTProperties* props);

LIBMQTT_API MQTTResponse MQTTClient_subscribeMany5(MQTTClient handle, int count, char* const* topic,
		int* qos, MQTTSubscribe_options* opts, MQTTProperties* props);

LIBMQTT_API int MQTTClient_unsubscribe(MQTTClient handle, const char* topic);

LIBMQTT_API MQTTResponse MQTTClient_unsubscribe5(MQTTClient handle, const char* topic, MQTTProperties* props);

LIBMQTT_API MQTTResponse MQTTClient_unsubscribeMany5(MQTTClient handle, int count, char* const* topic, MQTTProperties* props);

LIBMQTT_API MQTTResponse MQTTClient_publish5(MQTTClient handle, const char* topicName, int payloadlen, const void* payload,
		int qos, int retained, MQTTProperties* properties, MQTTClient_deliveryToken* dt);
LIBMQTT_API int MQTTClient_publishMessage(MQTTClient handle, const char* topicName, MQTTClient_message* msg, MQTTClient_deliveryToken* dt);


LIBMQTT_API MQTTResponse MQTTClient_publishMessage5(MQTTClient handle, const char* topicName, MQTTClient_message* msg,
		MQTTClient_deliveryToken* dt);

LIBMQTT_API int MQTTClient_waitForCompletion(MQTTClient handle, MQTTClient_deliveryToken dt, unsigned long timeout);


LIBMQTT_API void MQTTClient_yield(void);


LIBMQTT_API void MQTTClient_freeMessage(MQTTClient_message** msg);


LIBMQTT_API void MQTTClient_free(void* ptr);

LIBMQTT_API void MQTTClient_destroy(MQTTClient* handle);

#if defined(__cplusplus)
     }
#endif

#endif
