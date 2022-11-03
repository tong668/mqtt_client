
/**
 * @file
 * \brief functions to deal with reading and writing of MQTT packets from and to sockets
 *
 * Some other related functions are in the MQTTPacket module
 */


#include "MQTTPacketOut.h"
#include "Log.h"
#include <string.h>
#include <stdlib.h>

int MQTTPacket_send_connect(Clients *client, int MQTTVersion,
                            MQTTProperties *connectProperties, MQTTProperties *willProperties) {
    char *buf, *ptr;
    Connect packet;
    int rc = SOCKET_ERROR, len;
    packet.header.byte = 0;
    packet.header.bits.type = CONNECT;

    len = ((MQTTVersion == 3/*MQTTVERSION_3_1*/) ? 12 : 10) + (int) strlen(client->clientID) + 2;
    if (client->will)
        len += (int) strlen(client->will->topic) + 2 + client->will->payloadlen + 2;
    if (client->username)
        len += (int) strlen(client->username) + 2;
    if (client->password)
        len += client->passwordlen + 2;

    ptr = buf = malloc(len);
    if (ptr == NULL)
        goto exit_nofree;

    writeUTF(&ptr, "MQTT");
    writeChar(&ptr, (char) MQTTVersion);

    packet.flags.all = 0;
        packet.flags.bits.cleanstart = client->cleansession;
    packet.flags.bits.will = (client->will) ? 1 : 0;
    if (packet.flags.bits.will) {
        packet.flags.bits.willQoS = client->will->qos;
        packet.flags.bits.willRetain = client->will->retained;
    }
    if (client->username)
        packet.flags.bits.username = 1;
    if (client->password)
        packet.flags.bits.password = 1;

    writeChar(&ptr, packet.flags.all);
    writeInt(&ptr, client->keepAliveInterval);
    writeUTF(&ptr, client->clientID);
    if (client->will) {

        writeUTF(&ptr, client->will->topic);
        writeData(&ptr, client->will->payload, client->will->payloadlen);
    }
    if (client->username)
        writeUTF(&ptr, client->username);
    if (client->password)
        writeData(&ptr, client->password, client->passwordlen);

    rc = MQTTPacket_send(&client->net, packet.header, buf, len, 1, MQTTVersion);
    Log(LOG_PROTOCOL, 0, NULL, client->net.socket, client->clientID,
        MQTTVersion, client->cleansession, rc);
    exit:
    if (rc != TCPSOCKET_INTERRUPTED)
        free(buf);
    exit_nofree:
    return rc;
}

void *MQTTPacket_connack(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen) {
    Connack *pack = NULL;
    char *curdata = data;
    char *enddata = &data[datalen];
    if ((pack = malloc(sizeof(Connack))) == NULL)
        goto exit;
    pack->MQTTVersion = MQTTVersion;
    pack->header.byte = aHeader;
    if (datalen < 2) /* enough data for connect flags and reason code? */
    {
        free(pack);
        pack = NULL;
        goto exit;
    }
    pack->flags.all = readChar(&curdata); /* connect flags */
    pack->rc = readChar(&curdata); /* reason code */

    exit:
    return pack;
}

void MQTTPacket_freeConnack(Connack *pack) {
    free(pack);
}


int MQTTPacket_send_pingreq(networkHandles *net, const char *clientID) {
    Header header;
    int rc = 0;
    header.byte = 0;
    header.bits.type = PINGREQ;
    rc = MQTTPacket_send(net, header, NULL, 0, 0, MQTTVERSION_3_1_1);
    Log(LOG_PROTOCOL, 20, NULL, net->socket, clientID, rc);
    return rc;
}

int MQTTPacket_send_subscribe(List *topics, List *qoss, MQTTSubscribe_options *opts, MQTTProperties *props,
                              int msgid, int dup, Clients *client) {
    Header header;
    char *data, *ptr;
    int rc = -1;
    ListElement *elem = NULL, *qosElem = NULL;
    int datalen, i = 0;
    header.bits.type = SUBSCRIBE;
    header.bits.dup = dup;
    header.bits.qos = 1;
    header.bits.retain = 0;

    datalen = 2 + topics->count * 3; /* utf length + char qos == 3 */
    while (ListNextElement(topics, &elem))
        datalen += (int) strlen((char *) (elem->content));

    ptr = data = malloc(datalen);
    if (ptr == NULL)
        goto exit;
    writeInt(&ptr, msgid);

    elem = NULL;
    while (ListNextElement(topics, &elem)) {
        char subopts = 0;

        ListNextElement(qoss, &qosElem);
        writeUTF(&ptr, (char *) (elem->content));
        subopts = *(int *) (qosElem->content);
        writeChar(&ptr, subopts);
        ++i;
    }
    rc = MQTTPacket_send(&client->net, header, data, datalen, 1, client->MQTTVersion);
    Log(LOG_PROTOCOL, 22, NULL, client->net.socket, client->clientID, msgid, rc);
    if (rc != TCPSOCKET_INTERRUPTED)
        free(data);
    exit:
    return rc;
}


void *MQTTPacket_suback(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen) {
    Suback *pack = NULL;
    char *curdata = data;
    char *enddata = &data[datalen];
    if ((pack = malloc(sizeof(Suback))) == NULL)
        goto exit;
    pack->MQTTVersion = MQTTVersion;
    pack->header.byte = aHeader;
    if (enddata - curdata < 2)  /* Is there enough data to read the msgid? */
    {
        free(pack);
        pack = NULL;
        goto exit;
    }
    pack->msgId = readInt(&curdata);
    pack->qoss = ListInitialize();
    while ((size_t) (curdata - data) < datalen) {
        unsigned int *newint;
        newint = malloc(sizeof(unsigned int));
        if (newint == NULL) {
            if (pack->properties.array)
                free(pack->properties.array);
            if (pack)
                free(pack);
            pack = NULL; /* signal protocol error */
            goto exit;
        }
        *newint = (unsigned int) readChar(&curdata);
        ListAppend(pack->qoss, newint, sizeof(unsigned int));
    }
    if (pack->qoss->count == 0) {
        if (pack->properties.array)
            free(pack->properties.array);
        ListFree(pack->qoss);
        free(pack);
        pack = NULL;
    }
    exit:
    return pack;
}

int MQTTPacket_send_unsubscribe(List *topics, MQTTProperties *props, int msgid, int dup, Clients *client) {
    Header header;
    char *data, *ptr;
    int rc = SOCKET_ERROR;
    ListElement *elem = NULL;
    int datalen;
    header.bits.type = UNSUBSCRIBE;
    header.bits.dup = dup;
    header.bits.qos = 1;
    header.bits.retain = 0;

    datalen = 2 + topics->count * 2; /* utf length == 2 */
    while (ListNextElement(topics, &elem))
        datalen += (int) strlen((char *) (elem->content));
    ptr = data = malloc(datalen);
    if (ptr == NULL)
        goto exit;

    writeInt(&ptr, msgid);
    elem = NULL;
    while (ListNextElement(topics, &elem))
        writeUTF(&ptr, (char *) (elem->content));
    rc = MQTTPacket_send(&client->net, header, data, datalen, 1, client->MQTTVersion);
    Log(LOG_PROTOCOL, 25, NULL, client->net.socket, client->clientID, msgid, rc);
    if (rc != TCPSOCKET_INTERRUPTED)
        free(data);
    exit:
    return rc;
}

void *MQTTPacket_unsuback(int MQTTVersion, unsigned char aHeader, char *data, size_t datalen) {
    Unsuback *pack = NULL;
    char *curdata = data;
    char *enddata = &data[datalen];
    if ((pack = malloc(sizeof(Unsuback))) == NULL)
        goto exit;
    pack->MQTTVersion = MQTTVersion;
    pack->header.byte = aHeader;
    if (enddata - curdata < 2)  /* Is there enough data? */
    {
        free(pack);
        pack = NULL;
        goto exit;
    }
    pack->msgId = readInt(&curdata);
    pack->reasonCodes = NULL;

    exit:
    return pack;
}
