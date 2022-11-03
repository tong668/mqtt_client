//
// Created by Administrator on 2022/11/3.
//

#ifndef MQTT_CLIENT_MQTTPACKET_H
#define MQTT_CLIENT_MQTTPACKET_H

#include "Socket.h"
#include "LinkedList.h"
#include "mqttProperties.h"
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

int MQTTPacket_send(networkHandles *net, Header header, char *buffer, size_t buflen, int free, int MQTTVersion);

int MQTTPacket_sends(networkHandles *net, Header header, PacketBuffers *buffers, int MQTTVersion);

void *MQTTPacket_header_only(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);

int MQTTPacket_send_disconnect(Clients *client, enum MQTTReasonCodes reason, MQTTProperties *props);

void *MQTTPacket_publish(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);

void MQTTPacket_freePublish(Publish *pack);

int MQTTPacket_send_publish(Publish *pack, int dup, int qos, int retained, networkHandles *net, const char *clientID);

int MQTTPacket_send_puback(int MQTTVersion, int msgid, networkHandles *net, const char *clientID);

void *MQTTPacket_ack(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);

void MQTTPacket_freeSuback(Suback *pack);

void MQTTPacket_freeUnsuback(Unsuback *pack);

int MQTTPacket_send_pubrec(int MQTTVersion, int msgid, networkHandles *net, const char *clientID);

int MQTTPacket_send_pubrel(int MQTTVersion, int msgid, int dup, networkHandles *net, const char *clientID);

int MQTTPacket_send_pubcomp(int MQTTVersion, int msgid, networkHandles *net, const char *clientID);

void writeInt4(char **pptr, int anInt);

void writeMQTTLenString(char **pptr, MQTTLenString lenstring);

int MQTTPacket_VBIlen(int rem_len);

int clientSocketCompare(void *a, void *b); //todo 暂时放这里，将来移走

//#include "mqttPacketOut.h"

int MQTTPacket_send_connect(Clients *client, int MQTTVersion,
                            MQTTProperties *connectProperties, MQTTProperties *willProperties);

void *MQTTPacket_connack(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);

void MQTTPacket_freeConnack(Connack *pack);

int MQTTPacket_send_pingreq(networkHandles *net, const char *clientID);

int MQTTPacket_send_subscribe(List *topics, List *qoss, MQTTSubscribe_options *opts, MQTTProperties *props,
                              int msgid, int dup, Clients *client);

void *MQTTPacket_suback(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);

int MQTTPacket_send_unsubscribe(List *topics, MQTTProperties *props, int msgid, int dup, Clients *client);

void *MQTTPacket_unsuback(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen);

#endif //MQTT_CLIENT_MQTTPACKET_H
