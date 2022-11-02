

#if !defined(MQTTPROTOCOL_H)
#define MQTTPROTOCOL_H

#include "LinkedList.h"
#include "MQTTPacket.h"
#include "Clients.h"

#define MAX_MSG_ID 65535

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


#include "MQTTProtocolOut.h"

#endif
