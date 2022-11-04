//
// Created by Administrator on 2022/11/3.
//

#ifndef MQTT_CLIENT_MQTTPACKET_H
#define MQTT_CLIENT_MQTTPACKET_H

#include "Socket.h"
#include "LinkedList.h"
#include "MQTTProperties.h"
#include <endian.h>

int MQTTPacket_encode(char *buf, size_t length);

int MQTTPacket_decode(networkHandles *net, size_t *value);

int readInt(char **pptr);

unsigned char readChar(char **pptr);

void writeChar(char **pptr, char c);

void writeInt(char **pptr, int anInt);

void writeUTF(char **pptr, const char *string);

void writeData(char **pptr, const void *data, int datalen);

void *MQTTPacket_Factory(int MQTTVersion, networkHandles *net, int *error);

int MQTTPacket_send(networkHandles *net, Header header, char *buffer, size_t buflen, int freeData);

int MQTTPacket_sends(networkHandles *net, Header header, PacketBuffers *bufs);

void *MQTTPacket_publish(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);

void MQTTPacket_freePublish(Publish *pack);

int MQTTPacket_send_publish(Publish *pack, int dup, int qos, int retained, networkHandles *net, const char *clientID);

int MQTTPacket_send_puback(int msgid, networkHandles *net, const char *clientID);

void *MQTTPacket_ack(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);

void writeInt4(char **pptr, int anInt);

void writeMQTTLenString(char **pptr, MQTTLenString lenstring);

int MQTTPacket_VBIlen(int rem_len);

int MQTTPacket_send_connect(Clients *client, int MQTTVersion);

void *MQTTPacket_connack(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);


int MQTTPacket_send_subscribe(List *topics, List *qoss, int msgid, int dup, Clients *client);

void *MQTTPacket_suback(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);


#endif //MQTT_CLIENT_MQTTPACKET_H
