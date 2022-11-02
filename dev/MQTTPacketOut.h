
#if !defined(MQTTPACKETOUT_H)
#define MQTTPACKETOUT_H

#include "MQTTPacket.h"

int MQTTPacket_send_connect(Clients* client, int MQTTVersion,
		MQTTProperties* connectProperties, MQTTProperties* willProperties);
void* MQTTPacket_connack(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen);
void MQTTPacket_freeConnack(Connack* pack);

int MQTTPacket_send_pingreq(networkHandles* net, const char* clientID);

int MQTTPacket_send_subscribe(List* topics, List* qoss, MQTTSubscribe_options* opts, MQTTProperties* props,
		int msgid, int dup, Clients* client);
void* MQTTPacket_suback(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen);

int MQTTPacket_send_unsubscribe(List* topics, MQTTProperties* props, int msgid, int dup, Clients* client);
void* MQTTPacket_unsuback(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen);

#endif
