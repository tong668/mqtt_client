

#if !defined(MQTTPACKET_H)
#define MQTTPACKET_H

#include "Socket.h"
#include "LinkedList.h"
#include "Clients.h"

#include "MQTTProperties.h"

#include <endian.h>


int MQTTPacket_encode(char* buf, size_t length);
int MQTTPacket_decode(networkHandles* net, size_t* value);
int readInt(char** pptr);
unsigned char readChar(char** pptr);
void writeChar(char** pptr, char c);
void writeInt(char** pptr, int anInt);
void writeUTF(char** pptr, const char* string);
void writeData(char** pptr, const void* data, int datalen);

void* MQTTPacket_Factory(int MQTTVersion, networkHandles* net, int* error);
int MQTTPacket_send(networkHandles* net, Header header, char* buffer, size_t buflen, int free, int MQTTVersion);
int MQTTPacket_sends(networkHandles* net, Header header, PacketBuffers* buffers, int MQTTVersion);

void* MQTTPacket_header_only(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen);
int MQTTPacket_send_disconnect(Clients* client, enum MQTTReasonCodes reason, MQTTProperties* props);

void* MQTTPacket_publish(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen);
void MQTTPacket_freePublish(Publish* pack);
int MQTTPacket_send_publish(Publish* pack, int dup, int qos, int retained, networkHandles* net, const char* clientID);
int MQTTPacket_send_puback(int MQTTVersion, int msgid, networkHandles* net, const char* clientID);
void* MQTTPacket_ack(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen);

void MQTTPacket_freeSuback(Suback* pack);
void MQTTPacket_freeUnsuback(Unsuback* pack);
int MQTTPacket_send_pubrec(int MQTTVersion, int msgid, networkHandles* net, const char* clientID);
int MQTTPacket_send_pubrel(int MQTTVersion, int msgid, int dup, networkHandles* net, const char* clientID);
int MQTTPacket_send_pubcomp(int MQTTVersion, int msgid, networkHandles* net, const char* clientID);

void writeInt4(char** pptr, int anInt);
void writeMQTTLenString(char** pptr, MQTTLenString lenstring);
int MQTTPacket_VBIlen(int rem_len);

#include "MQTTPacketOut.h"

#endif /* MQTTPACKET_H */
