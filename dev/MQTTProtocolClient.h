

#if !defined(MQTTPROTOCOLCLIENT_H)
#define MQTTPROTOCOLCLIENT_H

#include "LinkedList.h"
#include "MQTTPacket.h"
#include "Log.h"
#include "MQTTProtocol.h"
#include "Messages.h"
#include "MQTTProperties.h"

#define MAX_MSG_ID 65535
#define MAX_CLIENTID_LEN 65535

int MQTTProtocol_startPublish(Clients *pubclient, Publish *publish, int qos, int retained, Messages **m);

Messages *MQTTProtocol_createMessage(Publish *publish, Messages **mm, int qos, int retained, int allocatePayload);

Publications *MQTTProtocol_storePublication(Publish *publish, int *len);

int messageIDCompare(void *a, void *b);

int MQTTProtocol_assignMsgId(Clients *client);

void MQTTProtocol_removePublication(Publications *p);

void Protocol_processPublication(Publish *publish, Clients *client, int allocatePayload);

int MQTTProtocol_handlePublishes(void *pack, SOCKET sock);

int MQTTProtocol_handlePubacks(void *pack, SOCKET sock);

int MQTTProtocol_handlePubrecs(void *pack, SOCKET sock);

int MQTTProtocol_handlePubrels(void *pack, SOCKET sock);

int MQTTProtocol_handlePubcomps(void *pack, SOCKET sock);

void MQTTProtocol_closeSession(Clients *c, int sendwill);

void MQTTProtocol_keepalive(START_TIME_TYPE);

void MQTTProtocol_retry(START_TIME_TYPE, int, int);

void MQTTProtocol_freeClient(Clients *client);

void MQTTProtocol_emptyMessageList(List *msgList);

void MQTTProtocol_freeMessageList(List *msgList);

char *MQTTStrncpy(char *dest, const char *src, size_t num);

char *MQTTStrdup(const char *src);

void MQTTProtocol_writeAvailable(SOCKET socket);

#endif
