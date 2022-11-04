//
// Created by Administrator on 2022/11/3.
//

#include <string.h>
#include <ctype.h>
#include "MQTTTime.h"
#include "Log.h"
#include "SocketBuffer.h"
#include "MQTTProtocol.h"
#include "MQTTPacket.h"

extern MQTTProtocol state;
extern ClientStates *bstate;

static void MQTTProtocol_storeQoS0(Clients *pubclient, Publish *publish);

static int MQTTProtocol_startPublishCommon(
        Clients *pubclient,
        Publish *publish,
        int qos,
        int retained);

static int MQTTProtocol_queueAck(Clients *client, int ackType, int msgId);

typedef struct {
    int messageId;
    int ackType;
} AckRequest;

int messageIDCompare(void *a, void *b) {
    Messages *msg = (Messages *) a;
    return msg->msgid == *(int *) b;
}

int MQTTProtocol_assignMsgId(Clients *client) {
    int start_msgid = client->msgID;
    int msgid = start_msgid;
    msgid = (msgid == MAX_MSG_ID) ? 1 : msgid + 1;
    while (ListFindItem(client->outboundMsgs, &msgid, messageIDCompare) != NULL) {
        msgid = (msgid == MAX_MSG_ID) ? 1 : msgid + 1;
        if (msgid == start_msgid) { /* we've tried them all - none free */
            msgid = 0;
            break;
        }
    }
    if (msgid != 0)
        client->msgID = msgid;
    return msgid;
}

static void MQTTProtocol_storeQoS0(Clients *pubclient, Publish *publish) {
    int len;
    pending_write *pw = NULL;

    /* store the publication until the write is finished */
    if ((pw = malloc(sizeof(pending_write))) == NULL)
        goto exit;
    Log(TRACE_MIN, 12, NULL);
    if ((pw->p = MQTTProtocol_storePublication(publish, &len)) == NULL) {
        free(pw);
        goto exit;
    }
    pw->socket = pubclient->net.socket;
    if (!ListAppend(&(state.pending_writes), pw, sizeof(pending_write) + len)) {
        free(pw->p);
        free(pw);
        goto exit;
    }
    /* we don't copy QoS 0 messages unless we have to, so now we have to tell the socket buffer where
    the saved copy is */
    if (SocketBuffer_updateWrite(pw->socket, pw->p->topic, pw->p->payload) == NULL)
        Log(LOG_SEVERE, 0, "Error updating write");
    publish->payload = publish->topic = NULL;
    exit:
    return;
}

static int MQTTProtocol_startPublishCommon(Clients *pubclient, Publish *publish, int qos, int retained) {
    int rc = TCPSOCKET_COMPLETE;
    rc = MQTTPacket_send_publish(publish, 0, qos, retained, &pubclient->net, pubclient->clientID);
    if (qos == 0 && rc == TCPSOCKET_INTERRUPTED)
        MQTTProtocol_storeQoS0(pubclient, publish);
    return rc;
}

int MQTTProtocol_startPublish(Clients *pubclient, Publish *publish, int qos, int retained, Messages **mm) {
    Publish qos12pub = *publish;
    int rc = 0;
    if (qos > 0) {
        *mm = MQTTProtocol_createMessage(publish, mm, qos, retained, 0);
        ListAppend(pubclient->outboundMsgs, *mm, (*mm)->len);
        /* we change these pointers to the saved message location just in case the packet could not be written
        entirely; the socket buffer will use these locations to finish writing the packet */
        qos12pub.payload = (*mm)->publish->payload;
        qos12pub.topic = (*mm)->publish->topic;
        qos12pub.properties = (*mm)->properties;
        qos12pub.MQTTVersion = (*mm)->MQTTVersion;
        publish = &qos12pub;
    }
    rc = MQTTProtocol_startPublishCommon(pubclient, publish, qos, retained);
    if (qos > 0)
        memcpy((*mm)->publish->mask, publish->mask, sizeof((*mm)->publish->mask));
    return rc;
}

Messages *MQTTProtocol_createMessage(Publish *publish, Messages **mm, int qos, int retained, int allocatePayload) {
    Messages *m = malloc(sizeof(Messages));
    if (!m)
        goto exit;
    m->len = sizeof(Messages);
    if (*mm == NULL || (*mm)->publish == NULL) {
        int len1;
        *mm = m;
        if ((m->publish = MQTTProtocol_storePublication(publish, &len1)) == NULL) {
            free(m);
            goto exit;
        }
        m->len += len1;
        if (allocatePayload) {
            char *temp = m->publish->payload;

            if ((m->publish->payload = malloc(m->publish->payloadlen)) == NULL) {
                free(m);
                goto exit;
            }
            memcpy(m->publish->payload, temp, m->publish->payloadlen);
        }
    } else /* this is now never used, I think */
    {
        ++(((*mm)->publish)->refcount);
        m->publish = (*mm)->publish;
    }
    m->msgid = publish->msgId;
    m->qos = qos;
    m->retain = retained;
    m->MQTTVersion = publish->MQTTVersion;
    if (m->MQTTVersion >= 5)
        m->properties = MQTTProperties_copy(&publish->properties);
    m->lastTouch = MQTTTime_now();
    if (qos == 2)
        m->nextMessageType = PUBREC;
    exit:
    return m;
}

Publications *MQTTProtocol_storePublication(Publish *publish, int *len) {
    Publications *p = malloc(sizeof(Publications));
    if (!p)
        goto exit;
    p->refcount = 1;
    *len = (int) strlen(publish->topic) + 1;
    p->topic = publish->topic;
    publish->topic = NULL;
    *len += sizeof(Publications);
    p->topiclen = publish->topiclen;
    p->payloadlen = publish->payloadlen;
    p->payload = publish->payload;
    publish->payload = NULL;
    *len += publish->payloadlen;
    memcpy(p->mask, publish->mask, sizeof(p->mask));

    if ((ListAppend(&(state.publications), p, *len)) == NULL) {
        free(p);
        p = NULL;
    }
    exit:
    return p;
}


void MQTTProtocol_removePublication(Publications *p) {
    if (p && --(p->refcount) == 0) {
        free(p->payload);
        p->payload = NULL;
        free(p->topic);
        p->topic = NULL;
        ListRemove(&(state.publications), p);
    }
}

int MQTTProtocol_handlePublishes(void *pack, SOCKET sock) {
    Publish *publish = (Publish *) pack;
    Clients *client = NULL;
    char *clientid = NULL;
    int rc = TCPSOCKET_COMPLETE;
    int socketHasPendingWrites = 0;
    client = (Clients *) (ListFindItem(bstate->clients, &sock, MQTTProperties_socketCompare)->content);
    clientid = client->clientID;
    Log(LOG_PROTOCOL, 11, NULL, sock, clientid, publish->msgId, publish->header.bits.qos,
        publish->header.bits.retain, publish->payloadlen, min(20, publish->payloadlen), publish->payload);

    if (publish->header.bits.qos == 0) {
        Protocol_processPublication(publish, client, 1);
        goto exit;
    }

    socketHasPendingWrites = !Socket_noPendingWrites(sock);

    if (publish->header.bits.qos == 1) {
        Protocol_processPublication(publish, client, 1);

        if (socketHasPendingWrites)
            rc = MQTTProtocol_queueAck(client, PUBACK, publish->msgId);
        else
            rc = MQTTPacket_send_puback(publish->msgId, &client->net, client->clientID);
    } else if (publish->header.bits.qos == 2) {
        /* store publication in inbound list */
        int len;
        int already_received = 0;
        ListElement *listElem = NULL;
        Messages *m = malloc(sizeof(Messages));
        Publications *p = NULL;
        if (!m) {
            rc = PAHO_MEMORY_ERROR;
            goto exit;
        }
        p = MQTTProtocol_storePublication(publish, &len);

        m->publish = p;
        m->msgid = publish->msgId;
        m->qos = publish->header.bits.qos;
        m->retain = publish->header.bits.retain;
        m->MQTTVersion = publish->MQTTVersion;
        m->nextMessageType = PUBREL;
        if ((listElem = ListFindItem(client->inboundMsgs, &(m->msgid), messageIDCompare)) !=
            NULL) {   /* discard queued publication with same msgID that the current incoming message */
            Messages *msg = (Messages *) (listElem->content);
            MQTTProtocol_removePublication(msg->publish);
            ListInsert(client->inboundMsgs, m, sizeof(Messages) + len, listElem);
            ListRemove(client->inboundMsgs, msg);
            already_received = 1;
        } else
            ListAppend(client->inboundMsgs, m, sizeof(Messages) + len);

        {    /* allocate and copy payload data as it's needed for pubrel.
		       For other cases, it's done in Protocol_processPublication */
            char *temp = m->publish->payload;

            if ((m->publish->payload = malloc(m->publish->payloadlen)) == NULL) {
                rc = PAHO_MEMORY_ERROR;
                goto exit;
            }
            memcpy(m->publish->payload, temp, m->publish->payloadlen);
        }
        if (socketHasPendingWrites)
            rc = MQTTProtocol_queueAck(client, PUBREC, publish->msgId);
        publish->topic = NULL;
    }
    exit:
    MQTTPacket_freePublish(publish);
    return rc;
}

int MQTTProtocol_handlePubacks(void *pack, SOCKET sock) {
    Puback *puback = (Puback *) pack;
    Clients *client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    client = (Clients *) (ListFindItem(bstate->clients, &sock, MQTTProperties_socketCompare)->content);
    Log(LOG_PROTOCOL, 14, NULL, sock, client->clientID, puback->msgId);

    /* look for the message by message id in the records of outbound messages for this client */
    if (ListFindItem(client->outboundMsgs, &(puback->msgId), messageIDCompare) == NULL)
        Log(TRACE_MIN, 3, NULL, "PUBACK", client->clientID, puback->msgId);
    else {
        Messages *m = (Messages *) (client->outboundMsgs->current->content);
        if (m->qos != 1)
            Log(TRACE_MIN, 4, NULL, "PUBACK", client->clientID, puback->msgId, m->qos);
        else {
            Log(TRACE_MIN, 6, NULL, "PUBACK", client->clientID, puback->msgId);
            MQTTProtocol_removePublication(m->publish);

            ListRemove(client->outboundMsgs, m);
        }
    }
    free(pack);
    return rc;
}

int MQTTProtocol_queueAck(Clients *client, int ackType, int msgId) {
    int rc = 0;
    AckRequest *ackReq = NULL;
    ackReq = malloc(sizeof(AckRequest));
    if (!ackReq)
        rc = PAHO_MEMORY_ERROR;
    else {
        ackReq->messageId = msgId;
        ackReq->ackType = ackType;
        ListAppend(client->outboundQueue, ackReq, sizeof(AckRequest));
    }
    return rc;
}

void MQTTProtocol_freeClient(Clients *client) {
    /* free up pending message lists here, and any other allocated data */
    MQTTProtocol_freeMessageList(client->outboundMsgs);
    MQTTProtocol_freeMessageList(client->inboundMsgs);
    ListFree(client->messageQueue);
    ListFree(client->outboundQueue);
    free(client->clientID);
    client->clientID = NULL;
    if (client->username)
        free((void *) client->username);
    if (client->password)
        free((void *) client->password);
}

void MQTTProtocol_emptyMessageList(List *msgList) {
    ListElement *current = NULL;

    while (ListNextElement(msgList, &current)) {
        Messages *m = (Messages *) (current->content);
        MQTTProtocol_removePublication(m->publish);
    }
    ListEmpty(msgList);
}

void MQTTProtocol_freeMessageList(List *msgList) {
    MQTTProtocol_emptyMessageList(msgList);
    ListFree(msgList);
}

char *MQTTStrncpy(char *dest, const char *src, size_t dest_size) {
    size_t count = dest_size;
    char *temp = dest;
    if (dest_size < strlen(src))
        Log(TRACE_MIN, -1, "the src string is truncated");

    /* We must copy only the first (dest_size - 1) bytes */
    while (count > 1 && (*temp++ = *src++))
        count--;

    *temp = '\0';
    return dest;
}


char *MQTTStrdup(const char *src) {
    size_t mlen = strlen(src) + 1;
    char *temp = malloc(mlen);
    if (temp)
        MQTTStrncpy(temp, src, mlen);
    else
        Log(LOG_ERROR, -1, "memory allocation error in MQTTStrdup");
    return temp;
}

size_t MQTTProtocol_addressPort(const char *uri, int *port, const char **topic, int default_port) {
    char *buf = (char *) uri;
    char *colon_pos;
    size_t len;
    char *topic_pos;
    colon_pos = strrchr(uri, ':'); /* reverse find to allow for ':' in IPv6 addresses */

    if (uri[0] == '[') {  /* ip v6 */
        if (colon_pos < strrchr(uri, ']'))
            colon_pos = NULL;  /* means it was an IPv6 separator, not for host:port */
    }

    if (colon_pos) /* have to strip off the port */
    {
        len = colon_pos - uri;
        *port = atoi(colon_pos + 1);
    } else {
        len = strlen(buf);
        *port = default_port;
    }

    /* find any topic portion */
    topic_pos = (char *) uri;
    if (colon_pos)
        topic_pos = colon_pos;
    topic_pos = strchr(topic_pos, '/');
    if (topic_pos) {
        if (topic)
            *topic = topic_pos;
        if (!colon_pos)
            len = topic_pos - uri;
    }

    if (buf[len - 1] == ']') {
        /* we are stripping off the final ], so length is 1 shorter */
        --len;
    }
    return len;
}


int MQTTProtocol_connect(const char *ip_address, Clients *aClient, int websocket, int MQTTVersion, long timeout) {
    int rc = 0,
            port;
    size_t addr_len;
    char *p0;

    aClient->good = 1;

    addr_len = MQTTProtocol_addressPort(ip_address, &port, NULL, websocket ? WS_DEFAULT_PORT : MQTT_DEFAULT_PORT);
    if (timeout < 0)
        rc = -1;
    else
        rc = Socket_new(ip_address, addr_len, port, &(aClient->net.socket), timeout);

    if (rc == EINPROGRESS || rc == EWOULDBLOCK)
        aClient->connect_state = TCP_IN_PROGRESS; /* TCP connect called - wait for connect completion */
    else if (rc == 0) {    /* TCP connect completed. If SSL, send SSL connect */

        /* Now send the MQTT connect packet */
        if ((rc = MQTTPacket_send_connect(aClient, MQTTVersion)) == 0)
            aClient->connect_state = WAIT_FOR_CONNACK; /* MQTT Connect sent - wait for CONNACK */
        else
            aClient->connect_state = NOT_IN_PROGRESS;
    }
    return rc;
}

int MQTTProtocol_subscribe(Clients *client, List *topics, List *qoss, int msgID, MQTTProperties *props) {
    int rc = 0;
    rc = MQTTPacket_send_subscribe(topics, qoss, msgID, 0, client);
    return rc;
}

void Protocol_processPublication(Publish *publish, Clients *client, int allocatePayload) {
    qEntry *qe = NULL;
    MQTTClient_message *mm = NULL;
    MQTTClient_message initialized = MQTTClient_message_initializer;
    qe = malloc(sizeof(qEntry));
    if (!qe)
        goto exit;
    mm = malloc(sizeof(MQTTClient_message));
    if (!mm) {
        free(qe);
        goto exit;
    }
    memcpy(mm, &initialized, sizeof(MQTTClient_message));

    qe->msg = mm;
    qe->topicName = publish->topic;
    qe->topicLen = publish->topiclen;
    publish->topic = NULL;
    if (allocatePayload) {
        mm->payload = malloc(publish->payloadlen);
        if (mm->payload == NULL) {
            free(mm);
            free(qe);
            goto exit;
        }
        memcpy(mm->payload, publish->payload, publish->payloadlen);
    } else
        mm->payload = publish->payload;
    mm->payloadlen = publish->payloadlen;
    mm->qos = publish->header.bits.qos;
    mm->retained = publish->header.bits.retain;
    if (publish->header.bits.qos == 2)
        mm->dup = 0;  /* ensure that a QoS2 message is not passed to the application with dup = 1 */
    else
        mm->dup = publish->header.bits.dup;
    mm->msgid = publish->msgId;

    if (publish->MQTTVersion >= 5)
        mm->properties = MQTTProperties_copy(&publish->properties);
    ListAppend(client->messageQueue, qe, sizeof(qe) + sizeof(mm) + mm->payloadlen + strlen(qe->topicName) + 1);
    exit:
    FUNC_EXIT;
}