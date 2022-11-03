
/**
 * @file
 * \brief functions to deal with reading and writing of MQTT packets from and to sockets
 *
 * Some other related functions are in the MQTTPacketOut module
 */

#include "MQTTPacket.h"
#include "Log.h"

#include "Messages.h"
#include "WebSocket.h"
#include "MQTTTime.h"
#include <stdlib.h>
#include <string.h>


/**
 * Array of functions to build packets, indexed according to packet code
 */
pf new_packets[] =
{
	NULL,	/**< reserved */
	NULL,	/**< MQTTPacket_connect*/
	MQTTPacket_connack, /**< CONNACK */
	MQTTPacket_publish,	/**< PUBLISH */
	MQTTPacket_ack, /**< PUBACK */
	MQTTPacket_ack, /**< PUBREC */
	MQTTPacket_ack, /**< PUBREL */
	MQTTPacket_ack, /**< PUBCOMP */
	NULL, /**< MQTTPacket_subscribe*/
	MQTTPacket_suback, /**< SUBACK */
	NULL, /**< MQTTPacket_unsubscribe*/
	MQTTPacket_unsuback, /**< UNSUBACK */
	MQTTPacket_header_only, /**< PINGREQ */
	MQTTPacket_header_only, /**< PINGRESP */
	MQTTPacket_ack,  /**< DISCONNECT */
	MQTTPacket_ack   /**< AUTH */
};


static char* readUTFlen(char** pptr, char* enddata, int* len);
static int MQTTPacket_send_ack(int MQTTVersion, int type, int msgid, int dup, networkHandles *net);


void* MQTTPacket_Factory(int MQTTVersion, networkHandles* net, int* error)
{
	char* data = NULL;
	static Header header;
	size_t remaining_length;
	int ptype;
	void* pack = NULL;
	size_t actual_len = 0;

	*error = SOCKET_ERROR;  /* indicate whether an error occurred, or not */

	const size_t headerWsFramePos = WebSocket_framePos();

	/* read the packet data from the socket */
	*error = WebSocket_getch(net, &header.byte);
	if (*error != TCPSOCKET_COMPLETE)   /* first byte is the header byte */
		goto exit; /* packet not read, *error indicates whether SOCKET_ERROR occurred */

	/* now read the remaining length, so we know how much more to read */
	if ((*error = MQTTPacket_decode(net, &remaining_length)) != TCPSOCKET_COMPLETE)
		goto exit; /* packet not read, *error indicates whether SOCKET_ERROR occurred */

	/* now read the rest, the variable header and payload */
	data = WebSocket_getdata(net, remaining_length, &actual_len);
	if (remaining_length && data == NULL)
	{
		*error = SOCKET_ERROR;
		goto exit; /* socket error */
	}

	if (actual_len < remaining_length)
		*error = TCPSOCKET_INTERRUPTED;
	else
	{
		ptype = header.bits.type;
		//todo 5
		if (ptype < CONNECT || (MQTTVersion < 5/*MQTTVERSION_5*/ && ptype >= DISCONNECT) ||
				(MQTTVersion >= 5/*MQTTVERSION_5*/ && ptype > AUTH) ||
				new_packets[ptype] == NULL)
			Log(TRACE_MIN, 2, NULL, ptype);
		else
		{
			if ((pack = (*new_packets[ptype])(MQTTVersion, header.byte, data, remaining_length)) == NULL)
			{
				*error = SOCKET_ERROR; // was BAD_MQTT_PACKET;
				Log(LOG_ERROR, -1, "Bad MQTT packet, type %d", ptype);
			}
		}
	}
	if (pack)
		net->lastReceived = MQTTTime_now();
exit:
	if (*error == TCPSOCKET_INTERRUPTED)
		WebSocket_framePosSeekTo(headerWsFramePos);

	return pack;
}


int MQTTPacket_send(networkHandles* net, Header header, char* buffer, size_t buflen, int freeData,
		int MQTTVersion)
{
	int rc = SOCKET_ERROR;
	size_t buf0len;
	char *buf;
	PacketBuffers packetbufs;

	buf0len = 1 + MQTTPacket_encode(NULL, buflen);
	buf = malloc(buf0len);
	if (buf == NULL)
	{
		rc = SOCKET_ERROR;
		goto exit;
	}
	buf[0] = header.byte;
	MQTTPacket_encode(&buf[1], buflen);
	packetbufs.count = 1;
	packetbufs.buffers = &buffer;
	packetbufs.buflens = &buflen;
	packetbufs.frees = &freeData;
	memset(packetbufs.mask, '\0', sizeof(packetbufs.mask));
	rc = WebSocket_putdatas(net, &buf, &buf0len, &packetbufs);

	if (rc == TCPSOCKET_COMPLETE)
		net->lastSent = MQTTTime_now();
	
	if (rc != TCPSOCKET_INTERRUPTED)
	  free(buf);

exit:
	return rc;
}

int MQTTPacket_sends(networkHandles* net, Header header, PacketBuffers* bufs, int MQTTVersion)
{
	int i, rc = SOCKET_ERROR;
	size_t buf0len, total = 0;
	char *buf;
	for (i = 0; i < bufs->count; i++)
		total += bufs->buflens[i];
	buf0len = 1 + MQTTPacket_encode(NULL, total);
	buf = malloc(buf0len);
	if (buf == NULL)
	{
		rc = SOCKET_ERROR;
		goto exit;
	}
	buf[0] = header.byte;
	MQTTPacket_encode(&buf[1], total);
	rc = WebSocket_putdatas(net, &buf, &buf0len, bufs);

	if (rc == TCPSOCKET_COMPLETE)
		net->lastSent = MQTTTime_now();
	
	if (rc != TCPSOCKET_INTERRUPTED)
	  free(buf);
exit:
	return rc;
}

int MQTTPacket_encode(char* buf, size_t length)
{
	int rc = 0;
	do
	{
		char d = length % 128;
		length /= 128;
		/* if there are more digits to encode, set the top bit of this digit */
		if (length > 0)
			d |= 0x80;
		if (buf)
			buf[rc++] = d;
		else
			rc++;
	} while (length > 0);
	return rc;
}

int MQTTPacket_decode(networkHandles* net, size_t* value)
{
	int rc = SOCKET_ERROR;
	char c;
	int multiplier = 1;
	int len = 0;
#define MAX_NO_OF_REMAINING_LENGTH_BYTES 4
	*value = 0;
	do
	{
		if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
		{
			rc = SOCKET_ERROR;	/* bad data */
			goto exit;
		}
		rc = WebSocket_getch(net, &c);
		if (rc != TCPSOCKET_COMPLETE)
				goto exit;
		*value += (c & 127) * multiplier;
		multiplier *= 128;
	} while ((c & 128) != 0);
exit:
	return rc;
}

int readInt(char** pptr)
{
	char* ptr = *pptr;
	int len = 256*((unsigned char)(*ptr)) + (unsigned char)(*(ptr+1));
	*pptr += 2;
	return len;
}

static char* readUTFlen(char** pptr, char* enddata, int* len)
{
	char* string = NULL;
	if (enddata - (*pptr) > 1) /* enough length to read the integer? */
	{
		*len = readInt(pptr);
		if (&(*pptr)[*len] <= enddata)
		{
			if ((string = malloc(*len+1)) == NULL)
				goto exit;
			memcpy(string, *pptr, *len);
			string[*len] = '\0';
			*pptr += *len;
		}
	}
exit:
	return string;
}

unsigned char readChar(char** pptr)
{
	unsigned char c = **pptr;
	(*pptr)++;
	return c;
}


void writeChar(char** pptr, char c)
{
	**pptr = c;
	(*pptr)++;
}

void writeInt(char** pptr, int anInt)
{
	**pptr = (char)(anInt / 256);
	(*pptr)++;
	**pptr = (char)(anInt % 256);
	(*pptr)++;
}


void writeUTF(char** pptr, const char* string)
{
	size_t len = strlen(string);
	writeInt(pptr, (int)len);
	memcpy(*pptr, string, len);
	*pptr += len;
}


void writeData(char** pptr, const void* data, int datalen)
{
	writeInt(pptr, datalen);
	memcpy(*pptr, data, datalen);
	*pptr += datalen;
}

void* MQTTPacket_header_only(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen)
{
	static unsigned char header = 0;
	header = aHeader;
	return &header;
}


int MQTTPacket_send_disconnect(Clients* client, enum MQTTReasonCodes reason, MQTTProperties* props)
{
	Header header;
	int rc = 0;
	header.byte = 0;
	header.bits.type = DISCONNECT;

	if (client->MQTTVersion >= 5 && (props || reason != MQTTREASONCODE_SUCCESS))
	{
		size_t buflen = 1 + ((props == NULL) ? 0 : MQTTProperties_len(props));
		char *buf = NULL;
		char *ptr = NULL;

		if ((buf = malloc(buflen)) == NULL)
		{
			rc = SOCKET_ERROR;
			goto exit;
		}
		ptr = buf;
		writeChar(&ptr, reason);
		if (props)
			MQTTProperties_write(&ptr, props);
		if ((rc = MQTTPacket_send(&client->net, header, buf, buflen, 1,
				                   client->MQTTVersion)) != TCPSOCKET_INTERRUPTED)
			free(buf);
	}
	else
		rc = MQTTPacket_send(&client->net, header, NULL, 0, 0, client->MQTTVersion);
exit:
	Log(LOG_PROTOCOL, 28, NULL, client->net.socket, client->clientID, rc);
	return rc;
}

void* MQTTPacket_publish(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen)
{
	Publish* pack = NULL;
	char* curdata = data;
	char* enddata = &data[datalen];
	if ((pack = malloc(sizeof(Publish))) == NULL)
		goto exit;
	memset(pack, '\0', sizeof(Publish));
	pack->MQTTVersion = MQTTVersion;
	pack->header.byte = aHeader;
	if ((pack->topic = readUTFlen(&curdata, enddata, &pack->topiclen)) == NULL) /* Topic name on which to publish */
	{
		free(pack);
		pack = NULL;
		goto exit;
	}
	if (pack->header.bits.qos > 0)  /* Msgid only exists for QoS 1 or 2 */
	{
		if (enddata - curdata < 2)  /* Is there enough data for the msgid? */
		{
			free(pack);
			pack = NULL;
			goto exit;
		}
		pack->msgId = readInt(&curdata);
	}
	else
		pack->msgId = 0;
	pack->payload = curdata;
	pack->payloadlen = (int)(datalen-(curdata-data));
exit:
	return pack;
}


/**
 * Free allocated storage for a publish packet.
 * @param pack pointer to the publish packet structure
 */
void MQTTPacket_freePublish(Publish* pack)
{
	if (pack->topic != NULL)
		free(pack->topic);
	free(pack);
}

static int MQTTPacket_send_ack(int MQTTVersion, int type, int msgid, int dup, networkHandles *net)
{
	Header header;
	int rc = SOCKET_ERROR;
	char *buf = NULL;
	char *ptr = NULL;
	if ((ptr = buf = malloc(2)) == NULL)
		goto exit;
	header.byte = 0;
	header.bits.type = type;
	header.bits.dup = dup;
	if (type == PUBREL)
	    header.bits.qos = 1;
	writeInt(&ptr, msgid);
	if ((rc = MQTTPacket_send(net, header, buf, 2, 1, MQTTVersion)) != TCPSOCKET_INTERRUPTED)
		free(buf);
exit:
	return rc;
}

int MQTTPacket_send_puback(int MQTTVersion, int msgid, networkHandles* net, const char* clientID)
{
	int rc = 0;

	rc =  MQTTPacket_send_ack(MQTTVersion, PUBACK, msgid, 0, net);
	Log(LOG_PROTOCOL, 12, NULL, net->socket, clientID, msgid, rc);
	return rc;
}

void MQTTPacket_freeSuback(Suback* pack)
{
	if (pack->qoss != NULL)
		ListFree(pack->qoss);
	free(pack);
}


void MQTTPacket_freeUnsuback(Unsuback* pack)
{
	free(pack);
}



int MQTTPacket_send_pubrec(int MQTTVersion, int msgid, networkHandles* net, const char* clientID)
{
	int rc = 0;
	rc =  MQTTPacket_send_ack(MQTTVersion, PUBREC, msgid, 0, net);
	Log(LOG_PROTOCOL, 13, NULL, net->socket, clientID, msgid, rc);
	return rc;
}


int MQTTPacket_send_pubrel(int MQTTVersion, int msgid, int dup, networkHandles* net, const char* clientID)
{
	int rc = 0;
	rc = MQTTPacket_send_ack(MQTTVersion, PUBREL, msgid, dup, net);
	Log(LOG_PROTOCOL, 16, NULL, net->socket, clientID, msgid, rc);
	return rc;
}


int MQTTPacket_send_pubcomp(int MQTTVersion, int msgid, networkHandles* net, const char* clientID)
{
	int rc = 0;
	rc = MQTTPacket_send_ack(MQTTVersion, PUBCOMP, msgid, 0, net);
	Log(LOG_PROTOCOL, 18, NULL, net->socket, clientID, msgid, rc);
	return rc;
}

void* MQTTPacket_ack(int MQTTVersion, unsigned char aHeader, char* data, size_t datalen)
{
	Ack* pack = NULL;
	char* curdata = data;
	char* enddata = &data[datalen];

	if ((pack = malloc(sizeof(Ack))) == NULL)
		goto exit;
	pack->MQTTVersion = MQTTVersion;
	pack->header.byte = aHeader;
	if (pack->header.bits.type != DISCONNECT)
	{
		if (enddata - curdata < 2)  /* Is there enough data for the msgid? */
		{
			free(pack);
			pack = NULL;
			goto exit;
		}
		pack->msgId = readInt(&curdata);
	}

exit:
	return pack;
}

int MQTTPacket_send_publish(Publish* pack, int dup, int qos, int retained, networkHandles* net, const char* clientID)
{
	Header header;
	char *topiclen;
	int rc = SOCKET_ERROR;
	topiclen = malloc(2);
	if (topiclen == NULL)
		goto exit;

	header.bits.type = PUBLISH;
	header.bits.dup = dup;
	header.bits.qos = qos;
	header.bits.retain = retained;
	if (qos > 0 || pack->MQTTVersion >= 5)
	{
		int buflen = ((qos > 0) ? 2 : 0) + ((pack->MQTTVersion >= 5) ? MQTTProperties_len(&pack->properties) : 0);
		char *ptr = NULL;
		char* bufs[4] = {topiclen, pack->topic, NULL, pack->payload};
		size_t lens[4] = {2, strlen(pack->topic), buflen, pack->payloadlen};
		int frees[4] = {1, 0, 1, 0};
		PacketBuffers packetbufs = {4, bufs, lens, frees, {pack->mask[0], pack->mask[1], pack->mask[2], pack->mask[3]}};

		bufs[2] = ptr = malloc(buflen);
		if (ptr == NULL)
			goto exit_free;
		if (qos > 0)
			writeInt(&ptr, pack->msgId);
		if (pack->MQTTVersion >= 5)
			MQTTProperties_write(&ptr, &pack->properties);

		ptr = topiclen;
		writeInt(&ptr, (int)lens[1]);
		rc = MQTTPacket_sends(net, header, &packetbufs, pack->MQTTVersion);
		if (rc != TCPSOCKET_INTERRUPTED)
			free(bufs[2]);
		memcpy(pack->mask, packetbufs.mask, sizeof(pack->mask));
	}
	else
	{
		char* ptr = topiclen;
		char* bufs[3] = {topiclen, pack->topic, pack->payload};
		size_t lens[3] = {2, strlen(pack->topic), pack->payloadlen};
		int frees[3] = {1, 0, 0};
		PacketBuffers packetbufs = {3, bufs, lens, frees, {pack->mask[0], pack->mask[1], pack->mask[2], pack->mask[3]}};

		writeInt(&ptr, (int)lens[1]);
		rc = MQTTPacket_sends(net, header, &packetbufs, pack->MQTTVersion);
		memcpy(pack->mask, packetbufs.mask, sizeof(pack->mask));
	}
	if (qos == 0)
		Log(LOG_PROTOCOL, 27, NULL, net->socket, clientID, retained, rc, pack->payloadlen,
				min(20, pack->payloadlen), pack->payload);
	else
		Log(LOG_PROTOCOL, 10, NULL, net->socket, clientID, pack->msgId, qos, retained, rc, pack->payloadlen,
				min(20, pack->payloadlen), pack->payload);
exit_free:
	if (rc != TCPSOCKET_INTERRUPTED)
		free(topiclen);
exit:
	return rc;
}

void writeInt4(char** pptr, int anInt)
{
  **pptr = (char)(anInt / 16777216);
  (*pptr)++;
  anInt %= 16777216;
  **pptr = (char)(anInt / 65536);
  (*pptr)++;
  anInt %= 65536;
	**pptr = (char)(anInt / 256);
	(*pptr)++;
	**pptr = (char)(anInt % 256);
	(*pptr)++;
}
void writeMQTTLenString(char** pptr, MQTTLenString lenstring)
{
  writeInt(pptr, lenstring.len);
  memcpy(*pptr, lenstring.data, lenstring.len);
  *pptr += lenstring.len;
}


int MQTTPacket_VBIlen(int rem_len)
{
	int rc = 0;

	if (rem_len < 128)
		rc = 1;
	else if (rem_len < 16384)
		rc = 2;
	else if (rem_len < 2097152)
		rc = 3;
	else
		rc = 4;
  return rc;
}

int clientSocketCompare(void* a, void* b)
{
    Clients* client = (Clients*)a;
    /*printf("comparing %d with %d\n", (char*)a, (char*)b); */
    return client->net.socket == *(SOCKET*)b;
}


