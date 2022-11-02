
#if !defined(CLIENTS_H)
#define CLIENTS_H

#include <stdint.h>
#include "MQTTTime.h"
#include "MQTTClient.h"
#include "LinkedList.h"
#include "Socket.h"


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
	START_TIME_TYPE ping_due_time;  /**< the time at which the ping should have been sent (ping_due) */
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

int clientSocketCompare(void* a, void* b);

/**
 * Configuration data related to all clients
 */
typedef struct
{
	const char* version;
	List* clients;
} ClientStates;

#endif
