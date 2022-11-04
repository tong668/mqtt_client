//
// Created by Administrator on 2022/11/3.
//

#ifndef MQTT_CLIENT_MQTTPROTOCOL_H
#define MQTT_CLIENT_MQTTPROTOCOL_H

#include "Socket.h"

int MQTTProtocol_startPublish(Clients *pubclient, Publish *publish, int qos, int retained, Messages **m);

Messages *MQTTProtocol_createMessage(Publish *publish, Messages **mm, int qos, int retained, int allocatePayload);

Publications *MQTTProtocol_storePublication(Publish *publish, int *len);

int messageIDCompare(void *a, void *b);

int MQTTProtocol_assignMsgId(Clients *client);

void MQTTProtocol_removePublication(Publications *p);

void Protocol_processPublication(Publish *publish, Clients *client, int allocatePayload);

int MQTTProtocol_handlePublishes(void *pack, SOCKET sock);

int MQTTProtocol_handlePubacks(void *pack, SOCKET sock);

void MQTTProtocol_freeClient(Clients *client);

void MQTTProtocol_emptyMessageList(List *msgList);

void MQTTProtocol_freeMessageList(List *msgList);

char *MQTTStrncpy(char *dest, const char *src, size_t num);

char *MQTTStrdup(const char *src);

size_t MQTTProtocol_addressPort(const char *uri, int *port, const char **topic, int default_port);

int MQTTProtocol_connect(const char *ip_address, Clients *acClients, int websocket, int MQTTVersion,
                         MQTTProperties *connectProperties, MQTTProperties *willProperties, long timeout);

int MQTTProtocol_subscribe(Clients *client, List *topics, List *qoss, int msgID,MQTTProperties *props);




#endif //MQTT_CLIENT_MQTTPROTOCOL_H
