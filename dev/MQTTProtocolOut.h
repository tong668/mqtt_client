
#if !defined(MQTTPROTOCOLOUT_H)
#define MQTTPROTOCOLOUT_H

#include "LinkedList.h"
#include "MQTTPacket.h"
#include "Clients.h"
#include "Log.h"
#include "Messages.h"
#include "MQTTProtocol.h"
#include "MQTTProtocolClient.h"

#define MQTT_DEFAULT_PORT 1883
#define SECURE_MQTT_DEFAULT_PORT 8883
#define WS_DEFAULT_PORT 80
#define WSS_DEFAULT_PORT 443
#define PROXY_DEFAULT_PORT 8080

size_t MQTTProtocol_addressPort(const char *uri, int *port, const char **topic, int default_port);


int MQTTProtocol_connect(const char *ip_address, Clients *acClients, int websocket, int MQTTVersion,
                         MQTTProperties *connectProperties, MQTTProperties *willProperties, long timeout);

int MQTTProtocol_handlePingresps(void *pack, SOCKET sock);

int MQTTProtocol_subscribe(Clients *client, List *topics, List *qoss, int msgID, MQTTSubscribe_options *opts,
                           MQTTProperties *props);

int MQTTProtocol_handleSubacks(void *pack, SOCKET sock);

int MQTTProtocol_unsubscribe(Clients *client, List *topics, int msgID, MQTTProperties *props);

int MQTTProtocol_handleUnsubacks(void *pack, SOCKET sock);


#endif
