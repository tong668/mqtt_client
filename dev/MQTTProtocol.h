//
// Created by Administrator on 2022/11/3.
//

#ifndef MQTT_CLIENT_MQTTPROTOCOL_H
#define MQTT_CLIENT_MQTTPROTOCOL_H

#include "Socket.h"


Publications *MQTTProtocol_storePublication(Publish *publish, int *len);

int messageIDCompare(void *a, void *b);

void MQTTProtocol_removePublication(Publications *p);

void Protocol_processPublication(Publish *publish, Clients *client, int allocatePayload);

int MQTTProtocol_handlePublishes(void *pack, SOCKET sock);

int MQTTProtocol_handlePubacks(void *pack, SOCKET sock);

int MQTTProtocol_handlePubrecs(void *pack, SOCKET sock);

int MQTTProtocol_handlePubrels(void *pack, SOCKET sock);

int MQTTProtocol_handlePubcomps(void *pack, SOCKET sock);

void MQTTProtocol_closeSession(Clients *c, int sendwill);

void MQTTProtocol_keepalive(struct timeval);

void MQTTProtocol_retry(struct timeval, int, int);


void MQTTProtocol_emptyMessageList(List *msgList);

char *MQTTStrncpy(char *dest, const char *src, size_t num);

char *MQTTStrdup(const char *src);

void MQTTProtocol_writeAvailable(SOCKET socket);


int MQTTProtocol_handlePingresps(void *pack, SOCKET sock);



#endif //MQTT_CLIENT_MQTTPROTOCOL_H
