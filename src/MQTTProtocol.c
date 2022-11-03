//
// Created by Administrator on 2022/11/3.
//

#include <string.h>
#include <ctype.h>
#include "MQTTTime.h"
#include "Log.h"
#include "SocketBuffer.h"
#include "Base64.h"
#include "WebSocket.h"
#include "MQTTProtocol.h"
#include "MQTTPacket.h"

extern MQTTProtocol state;
extern ClientStates* bstate;

static void MQTTProtocol_storeQoS0(Clients* pubclient, Publish* publish);
static int MQTTProtocol_startPublishCommon(
        Clients* pubclient,
        Publish* publish,
        int qos,
        int retained);
static void MQTTProtocol_retries(struct timeval now, Clients* client, int regardless);

static int MQTTProtocol_queueAck(Clients* client, int ackType, int msgId);

typedef struct {
    int messageId;
    int ackType;
} AckRequest;

int messageIDCompare(void* a, void* b)
{
    Messages* msg = (Messages*)a;
    return msg->msgid == *(int*)b;
}

int MQTTProtocol_assignMsgId(Clients* client)
{
    int start_msgid = client->msgID;
    int msgid = start_msgid;
    msgid = (msgid == MAX_MSG_ID) ? 1 : msgid + 1;
    while (ListFindItem(client->outboundMsgs, &msgid, messageIDCompare) != NULL)
    {
        msgid = (msgid == MAX_MSG_ID) ? 1 : msgid + 1;
        if (msgid == start_msgid)
        { /* we've tried them all - none free */
            msgid = 0;
            break;
        }
    }
    if (msgid != 0)
        client->msgID = msgid;
    return msgid;
}


static void MQTTProtocol_storeQoS0(Clients* pubclient, Publish* publish)
{
    int len;
    pending_write* pw = NULL;

    /* store the publication until the write is finished */
    if ((pw = malloc(sizeof(pending_write))) == NULL)
        goto exit;
    Log(TRACE_MIN, 12, NULL);
    if ((pw->p = MQTTProtocol_storePublication(publish, &len)) == NULL)
    {
        free(pw);
        goto exit;
    }
    pw->socket = pubclient->net.socket;
    if (!ListAppend(&(state.pending_writes), pw, sizeof(pending_write)+len))
    {
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

static int MQTTProtocol_startPublishCommon(Clients* pubclient, Publish* publish, int qos, int retained)
{
    int rc = TCPSOCKET_COMPLETE;
    rc = MQTTPacket_send_publish(publish, 0, qos, retained, &pubclient->net, pubclient->clientID);
    if (qos == 0 && rc == TCPSOCKET_INTERRUPTED)
        MQTTProtocol_storeQoS0(pubclient, publish);
    return rc;
}

int MQTTProtocol_startPublish(Clients* pubclient, Publish* publish, int qos, int retained, Messages** mm)
{
    Publish qos12pub = *publish;
    int rc = 0;
    if (qos > 0)
    {
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

Messages* MQTTProtocol_createMessage(Publish* publish, Messages **mm, int qos, int retained, int allocatePayload)
{
    Messages* m = malloc(sizeof(Messages));
    if (!m)
        goto exit;
    m->len = sizeof(Messages);
    if (*mm == NULL || (*mm)->publish == NULL)
    {
        int len1;
        *mm = m;
        if ((m->publish = MQTTProtocol_storePublication(publish, &len1)) == NULL)
        {
            free(m);
            goto exit;
        }
        m->len += len1;
        if (allocatePayload)
        {
            char *temp = m->publish->payload;

            if ((m->publish->payload = malloc(m->publish->payloadlen)) == NULL)
            {
                free(m);
                goto exit;
            }
            memcpy(m->publish->payload, temp, m->publish->payloadlen);
        }
    }
    else /* this is now never used, I think */
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

Publications* MQTTProtocol_storePublication(Publish* publish, int* len)
{
    Publications* p = malloc(sizeof(Publications));
    if (!p)
        goto exit;
    p->refcount = 1;
    *len = (int)strlen(publish->topic)+1;
    p->topic = publish->topic;
    publish->topic = NULL;
    *len += sizeof(Publications);
    p->topiclen = publish->topiclen;
    p->payloadlen = publish->payloadlen;
    p->payload = publish->payload;
    publish->payload = NULL;
    *len += publish->payloadlen;
    memcpy(p->mask, publish->mask, sizeof(p->mask));

    if ((ListAppend(&(state.publications), p, *len)) == NULL)
    {
        free(p);
        p = NULL;
    }
    exit:
    return p;
}


void MQTTProtocol_removePublication(Publications* p)
{
    if (p && --(p->refcount) == 0)
    {
        free(p->payload);
        p->payload = NULL;
        free(p->topic);
        p->topic = NULL;
        ListRemove(&(state.publications), p);
    }
}

int MQTTProtocol_handlePublishes(void* pack, SOCKET sock)
{
    Publish* publish = (Publish*)pack;
    Clients* client = NULL;
    char* clientid = NULL;
    int rc = TCPSOCKET_COMPLETE;
    int socketHasPendingWrites = 0;
    client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    clientid = client->clientID;
    Log(LOG_PROTOCOL, 11, NULL, sock, clientid, publish->msgId, publish->header.bits.qos,
        publish->header.bits.retain, publish->payloadlen, min(20, publish->payloadlen), publish->payload);

    if (publish->header.bits.qos == 0)
    {
        Protocol_processPublication(publish, client, 1);
        goto exit;
    }

    socketHasPendingWrites = !Socket_noPendingWrites(sock);

    if (publish->header.bits.qos == 1)
    {
        Protocol_processPublication(publish, client, 1);

        if (socketHasPendingWrites)
            rc = MQTTProtocol_queueAck(client, PUBACK, publish->msgId);
        else
            rc = MQTTPacket_send_puback(publish->MQTTVersion, publish->msgId, &client->net, client->clientID);
    }
    else if (publish->header.bits.qos == 2)
    {
        /* store publication in inbound list */
        int len;
        int already_received = 0;
        ListElement* listElem = NULL;
        Messages* m = malloc(sizeof(Messages));
        Publications* p = NULL;
        if (!m)
        {
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
        if ((listElem = ListFindItem(client->inboundMsgs, &(m->msgid), messageIDCompare)) != NULL)
        {   /* discard queued publication with same msgID that the current incoming message */
            Messages* msg = (Messages*)(listElem->content);
            MQTTProtocol_removePublication(msg->publish);
            ListInsert(client->inboundMsgs, m, sizeof(Messages) + len, listElem);
            ListRemove(client->inboundMsgs, msg);
            already_received = 1;
        } else
            ListAppend(client->inboundMsgs, m, sizeof(Messages) + len);

        {	/* allocate and copy payload data as it's needed for pubrel.
		       For other cases, it's done in Protocol_processPublication */
            char *temp = m->publish->payload;

            if ((m->publish->payload = malloc(m->publish->payloadlen)) == NULL)
            {
                rc = PAHO_MEMORY_ERROR;
                goto exit;
            }
            memcpy(m->publish->payload, temp, m->publish->payloadlen);
        }
        if (socketHasPendingWrites)
            rc = MQTTProtocol_queueAck(client, PUBREC, publish->msgId);
        else
            rc = MQTTPacket_send_pubrec(publish->MQTTVersion, publish->msgId, &client->net, client->clientID);
        publish->topic = NULL;
    }
    exit:
    MQTTPacket_freePublish(publish);
    return rc;
}

int MQTTProtocol_handlePubacks(void* pack, SOCKET sock)
{
    Puback* puback = (Puback*)pack;
    Clients* client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 14, NULL, sock, client->clientID, puback->msgId);

    /* look for the message by message id in the records of outbound messages for this client */
    if (ListFindItem(client->outboundMsgs, &(puback->msgId), messageIDCompare) == NULL)
        Log(TRACE_MIN, 3, NULL, "PUBACK", client->clientID, puback->msgId);
    else
    {
        Messages* m = (Messages*)(client->outboundMsgs->current->content);
        if (m->qos != 1)
            Log(TRACE_MIN, 4, NULL, "PUBACK", client->clientID, puback->msgId, m->qos);
        else
        {
            Log(TRACE_MIN, 6, NULL, "PUBACK", client->clientID, puback->msgId);
            MQTTProtocol_removePublication(m->publish);

            ListRemove(client->outboundMsgs, m);
        }
    }
    free(pack);
    return rc;
}


/**
 * Process an incoming pubrec packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePubrecs(void* pack, SOCKET sock)
{
    Pubrec* pubrec = (Pubrec*)pack;
    Clients* client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    int send_pubrel = 1; /* boolean to send PUBREL or not */

    client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 15, NULL, sock, client->clientID, pubrec->msgId);

    /* look for the message by message id in the records of outbound messages for this client */
    client->outboundMsgs->current = NULL;
    if (ListFindItem(client->outboundMsgs, &(pubrec->msgId), messageIDCompare) == NULL)
    {
        if (pubrec->header.bits.dup == 0)
            Log(TRACE_MIN, 3, NULL, "PUBREC", client->clientID, pubrec->msgId);
    }
    else
    {
        Messages* m = (Messages*)(client->outboundMsgs->current->content);
        if (m->qos != 2)
        {
            if (pubrec->header.bits.dup == 0)
                Log(TRACE_MIN, 4, NULL, "PUBREC", client->clientID, pubrec->msgId, m->qos);
        }
        else if (m->nextMessageType != PUBREC)
        {
            if (pubrec->header.bits.dup == 0)
                Log(TRACE_MIN, 5, NULL, "PUBREC", client->clientID, pubrec->msgId);
        }
        else
        {
            {
                m->nextMessageType = PUBCOMP;
                m->lastTouch = MQTTTime_now();
            }
        }
    }
    if (!send_pubrel)
        ; /* only don't send ack on MQTT v5 PUBREC error, otherwise send ack under all circumstances because MQTT state can get out of step */
    else if (!Socket_noPendingWrites(sock))
        rc = MQTTProtocol_queueAck(client, PUBREL, pubrec->msgId);
    else
        rc = MQTTPacket_send_pubrel(pubrec->MQTTVersion, pubrec->msgId, 0, &client->net, client->clientID);

    free(pack);
    return rc;
}


/**
 * Process an incoming pubrel packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePubrels(void* pack, SOCKET sock)
{
    Pubrel* pubrel = (Pubrel*)pack;
    Clients* client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 17, NULL, sock, client->clientID, pubrel->msgId);

    /* look for the message by message id in the records of inbound messages for this client */
    if (ListFindItem(client->inboundMsgs, &(pubrel->msgId), messageIDCompare) == NULL)
    {
        if (pubrel->header.bits.dup == 0)
            Log(TRACE_MIN, 3, NULL, "PUBREL", client->clientID, pubrel->msgId);
    }
    else
    {
        Messages* m = (Messages*)(client->inboundMsgs->current->content);
        if (m->qos != 2)
            Log(TRACE_MIN, 4, NULL, "PUBREL", client->clientID, pubrel->msgId, m->qos);
        else if (m->nextMessageType != PUBREL)
            Log(TRACE_MIN, 5, NULL, "PUBREL", client->clientID, pubrel->msgId);
        else
        {
            Publish publish;

            memset(&publish, '\0', sizeof(publish));

            publish.header.bits.qos = m->qos;
            publish.header.bits.retain = m->retain;
            publish.msgId = m->msgid;
            if (m->publish)
            {
                publish.topic = m->publish->topic;
                publish.topiclen = m->publish->topiclen;
                publish.payload = m->publish->payload;
                publish.payloadlen = m->publish->payloadlen;
            }
            publish.MQTTVersion = m->MQTTVersion;
            Protocol_processPublication(&publish, client, 0); /* only for 3.1.1 and lower */
            if (m->publish)
                ListRemove(&(state.publications), m->publish);
            ListRemove(client->inboundMsgs, m);
            ++(state.msgs_received);
        }
    }
    /* Send ack under all circumstances because MQTT state can get out of step - this standard also says to do this */
    if (!Socket_noPendingWrites(sock))
        rc = MQTTProtocol_queueAck(client, PUBCOMP, pubrel->msgId);
    else
        rc = MQTTPacket_send_pubcomp(pubrel->MQTTVersion, pubrel->msgId, &client->net, client->clientID);

    free(pack);
    return rc;
}

int MQTTProtocol_handlePubcomps(void* pack, SOCKET sock)
{
    Pubcomp* pubcomp = (Pubcomp*)pack;
    Clients* client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 19, NULL, sock, client->clientID, pubcomp->msgId);

    /* look for the message by message id in the records of outbound messages for this client */
    if (ListFindItem(client->outboundMsgs, &(pubcomp->msgId), messageIDCompare) == NULL)
    {
        if (pubcomp->header.bits.dup == 0)
            Log(TRACE_MIN, 3, NULL, "PUBCOMP", client->clientID, pubcomp->msgId);
    }
    else
    {
        Messages* m = (Messages*)(client->outboundMsgs->current->content);
        if (m->qos != 2)
            Log(TRACE_MIN, 4, NULL, "PUBCOMP", client->clientID, pubcomp->msgId, m->qos);
        else
        {
            if (m->nextMessageType != PUBCOMP)
                Log(TRACE_MIN, 5, NULL, "PUBCOMP", client->clientID, pubcomp->msgId);
            else
            {
                Log(TRACE_MIN, 6, NULL, "PUBCOMP", client->clientID, pubcomp->msgId);
                MQTTProtocol_removePublication(m->publish);
                ListRemove(client->outboundMsgs, m);
                (++state.msgs_sent);
            }
        }
    }
    free(pack);
    return rc;
}

void MQTTProtocol_keepalive(struct timeval now)
{
    ListElement* current = NULL;

    ListNextElement(bstate->clients, &current);
    while (current)
    {
        Clients* client =	(Clients*)(current->content);
        ListNextElement(bstate->clients, &current);

        if (client->connected == 0 || client->keepAliveInterval == 0)
            continue;

        if (client->ping_outstanding == 1)
        {
            if (MQTTTime_difftime(now, client->net.lastPing) >= (int64_t)(client->keepAliveInterval * 1000))
            {
                Log(TRACE_PROTOCOL, -1, "PINGRESP not received in keepalive interval for client %s on socket %d, disconnecting", client->clientID, client->net.socket);
                MQTTProtocol_closeSession(client, 1);
            }
        }
        else if (client->ping_due == 1 &&
                 (MQTTTime_difftime(now, client->ping_due_time) >= (int64_t)(client->keepAliveInterval * 1000)))
        {
            /* ping still outstanding after keep alive interval, so close session */
            Log(TRACE_PROTOCOL, -1, "PINGREQ still outstanding for client %s on socket %d, disconnecting", client->clientID, client->net.socket);
            MQTTProtocol_closeSession(client, 1);

        }
        else if (MQTTTime_difftime(now, client->net.lastSent) >= (int64_t)(client->keepAliveInterval * 1000) ||
                 MQTTTime_difftime(now, client->net.lastReceived) >= (int64_t)(client->keepAliveInterval * 1000))
        {
            if (Socket_noPendingWrites(client->net.socket))
            {
                if (MQTTPacket_send_pingreq(&client->net, client->clientID) != TCPSOCKET_COMPLETE)
                {
                    Log(TRACE_PROTOCOL, -1, "Error sending PINGREQ for client %s on socket %d, disconnecting", client->clientID, client->net.socket);
                    MQTTProtocol_closeSession(client, 1);
                }
                else
                {
                    client->ping_due = 0;
                    client->net.lastPing = now;
                    client->ping_outstanding = 1;
                }
            }
            else if (client->ping_due == 0)
            {
                Log(TRACE_PROTOCOL, -1, "Couldn't send PINGREQ for client %s on socket %d, noting",
                    client->clientID, client->net.socket);
                client->ping_due = 1;
                client->ping_due_time = now;
            }
        }
    }
}

static void MQTTProtocol_retries(struct timeval now, Clients* client, int regardless)
{
    ListElement* outcurrent = NULL;
    if (!regardless && client->retryInterval <= 0 && /* 0 or -ive retryInterval turns off retry except on reconnect */
        client->connect_sent == client->connect_count)
        goto exit;

    if (regardless)
        client->connect_count = client->outboundMsgs->count; /* remember the number of messages to retry on connect */
    else if (client->connect_sent < client->connect_count) /* continue a connect retry which didn't complete first time around */
        regardless = 1;

    while (client && ListNextElement(client->outboundMsgs, &outcurrent) &&
           client->connected && client->good &&        /* client is connected and has no errors */
           Socket_noPendingWrites(client->net.socket)) /* there aren't any previous packets still stacked up on the socket */
    {
        Messages* m = (Messages*)(outcurrent->content);
        if (regardless || MQTTTime_difftime(now, m->lastTouch) > (int64_t)(max(client->retryInterval, 10) * 1000))
        {
            if (regardless)
                ++client->connect_sent;
            if (m->qos == 1 || (m->qos == 2 && m->nextMessageType == PUBREC))
            {
                Publish publish;
                int rc;

                Log(TRACE_MIN, 7, NULL, "PUBLISH", client->clientID, client->net.socket, m->msgid);
                publish.msgId = m->msgid;
                publish.topic = m->publish->topic;
                publish.payload = m->publish->payload;
                publish.payloadlen = m->publish->payloadlen;
                publish.properties = m->properties;
                publish.MQTTVersion = m->MQTTVersion;
                memcpy(publish.mask, m->publish->mask, sizeof(publish.mask));
                rc = MQTTPacket_send_publish(&publish, 1, m->qos, m->retain, &client->net, client->clientID);
                memcpy(m->publish->mask, publish.mask, sizeof(m->publish->mask)); /* store websocket mask used in send */
                if (rc == SOCKET_ERROR)
                {
                    client->good = 0;
                    Log(TRACE_PROTOCOL, 29, NULL, client->clientID, client->net.socket,
                        Socket_getpeer(client->net.socket));
                    MQTTProtocol_closeSession(client, 1);
                    client = NULL;
                }
                else
                {
                    if (m->qos == 0 && rc == TCPSOCKET_INTERRUPTED)
                        MQTTProtocol_storeQoS0(client, &publish);
                    m->lastTouch = MQTTTime_now();
                }
            }
            else if (m->qos && m->nextMessageType == PUBCOMP)
            {
                Log(TRACE_MIN, 7, NULL, "PUBREL", client->clientID, client->net.socket, m->msgid);
                if (MQTTPacket_send_pubrel(m->MQTTVersion, m->msgid, 0, &client->net, client->clientID) != TCPSOCKET_COMPLETE)
                {
                    client->good = 0;
                    Log(TRACE_PROTOCOL, 29, NULL, client->clientID, client->net.socket,
                        Socket_getpeer(client->net.socket));
                    MQTTProtocol_closeSession(client, 1);
                    client = NULL;
                }
                else
                    m->lastTouch = MQTTTime_now();
            }
        }
    }
    exit:
    return;
}

int MQTTProtocol_queueAck(Clients* client, int ackType, int msgId)
{
    int rc = 0;
    AckRequest* ackReq = NULL;
    ackReq = malloc(sizeof(AckRequest));
    if (!ackReq)
        rc = PAHO_MEMORY_ERROR;
    else
    {
        ackReq->messageId = msgId;
        ackReq->ackType = ackType;
        ListAppend(client->outboundQueue, ackReq, sizeof(AckRequest));
    }
    return rc;
}

void MQTTProtocol_retry(struct timeval now, int doRetry, int regardless)
{
    ListElement* current = NULL;

    ListNextElement(bstate->clients, &current);
    /* look through the outbound message list of each client, checking to see if a retry is necessary */
    while (current)
    {
        Clients* client = (Clients*)(current->content);
        ListNextElement(bstate->clients, &current);
        if (client->connected == 0)
            continue;
        if (client->good == 0)
        {
            MQTTProtocol_closeSession(client, 1);
            continue;
        }
        if (Socket_noPendingWrites(client->net.socket) == 0)
            continue;
        if (doRetry)
            MQTTProtocol_retries(now, client, regardless);
    }
}

void MQTTProtocol_freeClient(Clients* client)
{
    /* free up pending message lists here, and any other allocated data */
    MQTTProtocol_freeMessageList(client->outboundMsgs);
    MQTTProtocol_freeMessageList(client->inboundMsgs);
    ListFree(client->messageQueue);
    ListFree(client->outboundQueue);
    free(client->clientID);
    client->clientID = NULL;
    if (client->will)
    {
        free(client->will->payload);
        free(client->will->topic);
        free(client->will);
        client->will = NULL;
    }
    if (client->username)
        free((void*)client->username);
    if (client->password)
        free((void*)client->password);
    if (client->httpProxy)
        free(client->httpProxy);
    if (client->httpsProxy)
        free(client->httpsProxy);
    if (client->net.http_proxy_auth)
        free(client->net.http_proxy_auth);
    /* don't free the client structure itself... this is done elsewhere */
}

void MQTTProtocol_emptyMessageList(List* msgList)
{
    ListElement* current = NULL;

    while (ListNextElement(msgList, &current))
    {
        Messages* m = (Messages*)(current->content);
        MQTTProtocol_removePublication(m->publish);
    }
    ListEmpty(msgList);
}


/**
 * Empty and free up all storage used by a message list
 * @param msgList the message list to empty and free
 */
void MQTTProtocol_freeMessageList(List* msgList)
{
    MQTTProtocol_emptyMessageList(msgList);
    ListFree(msgList);
}

void MQTTProtocol_writeAvailable(SOCKET socket)
{
    Clients* client = NULL;
    ListElement* current = NULL;
    int rc = 0;

    client = (Clients*)(ListFindItem(bstate->clients, &socket, clientSocketCompare)->content);

    current = NULL;
    while (ListNextElement(client->outboundQueue, &current) && rc == 0)
    {
        AckRequest* ackReq = (AckRequest*)(current->content);

        switch (ackReq->ackType)
        {
            case PUBACK:
                rc = MQTTPacket_send_puback(client->MQTTVersion, ackReq->messageId, &client->net, client->clientID);
                break;
            case PUBREC:
                rc = MQTTPacket_send_pubrec(client->MQTTVersion, ackReq->messageId, &client->net, client->clientID);
                break;
            case PUBREL:
                rc = MQTTPacket_send_pubrel(client->MQTTVersion, ackReq->messageId, 0, &client->net, client->clientID);
                break;
            case PUBCOMP:
                rc = MQTTPacket_send_pubcomp(client->MQTTVersion, ackReq->messageId, &client->net, client->clientID);
                break;
            default:
                Log(LOG_ERROR, -1, "unknown ACK type %d, dropping msg", ackReq->ackType);
                break;
        }
    }

    ListEmpty(client->outboundQueue);
}

char* MQTTStrncpy(char *dest, const char *src, size_t dest_size)
{
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


char* MQTTStrdup(const char* src)
{
    size_t mlen = strlen(src) + 1;
    char* temp = malloc(mlen);
    if (temp)
        MQTTStrncpy(temp, src, mlen);
    else
        Log(LOG_ERROR, -1, "memory allocation error in MQTTStrdup");
    return temp;
}

size_t MQTTProtocol_addressPort(const char* uri, int* port, const char **topic, int default_port)
{
    char* buf = (char*)uri;
    char* colon_pos;
    size_t len;
    char* topic_pos;
    colon_pos = strrchr(uri, ':'); /* reverse find to allow for ':' in IPv6 addresses */

    if (uri[0] == '[')
    {  /* ip v6 */
        if (colon_pos < strrchr(uri, ']'))
            colon_pos = NULL;  /* means it was an IPv6 separator, not for host:port */
    }

    if (colon_pos) /* have to strip off the port */
    {
        len = colon_pos - uri;
        *port = atoi(colon_pos + 1);
    }
    else
    {
        len = strlen(buf);
        *port = default_port;
    }

    /* find any topic portion */
    topic_pos = (char*)uri;
    if (colon_pos)
        topic_pos = colon_pos;
    topic_pos = strchr(topic_pos, '/');
    if (topic_pos)
    {
        if (topic)
            *topic = topic_pos;
        if (!colon_pos)
            len = topic_pos - uri;
    }

    if (buf[len - 1] == ']')
    {
        /* we are stripping off the final ], so length is 1 shorter */
        --len;
    }
    return len;
}


void MQTTProtocol_specialChars(char* p0, char* p1, b64_size_t *basic_auth_in_len)
{
    while (*p1 != '@')
    {
        if (*p1 != '%')
        {
            *p0++ = *p1++;
        }
        else if (isxdigit(*(p1 + 1)) && isxdigit(*(p1 + 2)))
        {
            /* next 2 characters are hexa digits */
            char hex[3];
            p1++;
            hex[0] = *p1++;
            hex[1] = *p1++;
            hex[2] = '\0';
            *p0++ = (char)strtol(hex, 0, 16);
            /* 3 input char => 1 output char */
            *basic_auth_in_len -= 2;
        }
    }
    *p0 = 0x0;
}


int MQTTProtocol_setHTTPProxy(Clients* aClient, char* source, char** dest, char** auth_dest, char* prefix)
{
    b64_size_t basic_auth_in_len, basic_auth_out_len;
    b64_data_t *basic_auth;
    char *p1;
    int rc = 0;

    if (*auth_dest)
    {
        free(*auth_dest);
        *auth_dest = NULL;
    }

    if (source)
    {
        if ((p1 = strstr(source, prefix)) != NULL) /* skip http:// prefix, if any */
            source += strlen(prefix);
        *dest = source;
        if ((p1 = strchr(source, '@')) != NULL) /* find user.pass separator */
            *dest = p1 + 1;

        if (p1)
        {
            /* basic auth len is string between http:// and @ */
            basic_auth_in_len = (b64_size_t)(p1 - source);
            if (basic_auth_in_len > 0)
            {
                basic_auth = (b64_data_t *)malloc(sizeof(char)*(basic_auth_in_len+1));
                if (!basic_auth)
                {
                    rc = PAHO_MEMORY_ERROR;
                    goto exit;
                }
                MQTTProtocol_specialChars((char*)basic_auth, source, &basic_auth_in_len);
                basic_auth_out_len = Base64_encodeLength(basic_auth, basic_auth_in_len) + 1; /* add 1 for trailing NULL */
                if ((*auth_dest = (char *)malloc(sizeof(char)*basic_auth_out_len)) == NULL)
                {
                    free(basic_auth);
                    rc = PAHO_MEMORY_ERROR;
                    goto exit;
                }
                Base64_encode(*auth_dest, basic_auth_out_len, basic_auth, basic_auth_in_len);
                free(basic_auth);
            }
        }
    }
    exit:
    return rc;
}

int MQTTProtocol_connect(const char* ip_address, Clients* aClient, int websocket, int MQTTVersion,
                         MQTTProperties* connectProperties, MQTTProperties* willProperties, long timeout)

{
    int rc = 0,
            port;
    size_t addr_len;
    char* p0;

    aClient->good = 1;

    if (aClient->httpProxy)
        p0 = aClient->httpProxy;
    else
        p0 = getenv("http_proxy");

    if (p0)
    {
        if ((rc = MQTTProtocol_setHTTPProxy(aClient, p0, &aClient->net.http_proxy, &aClient->net.http_proxy_auth, "http://")) != 0)
            goto exit;
        Log(TRACE_PROTOCOL, -1, "Setting http proxy to %s", aClient->net.http_proxy);
        if (aClient->net.http_proxy_auth)
            Log(TRACE_PROTOCOL, -1, "Setting http proxy auth to %s", aClient->net.http_proxy_auth);
    }

    if (aClient->net.http_proxy) {

        addr_len = MQTTProtocol_addressPort(aClient->net.http_proxy, &port, NULL, PROXY_DEFAULT_PORT);
        if (timeout < 0)
            rc = -1;
        else
            rc = Socket_new(aClient->net.http_proxy, addr_len, port, &(aClient->net.socket), timeout);

    }

    else {
        addr_len = MQTTProtocol_addressPort(ip_address, &port, NULL, websocket ? WS_DEFAULT_PORT : MQTT_DEFAULT_PORT);
        if (timeout < 0)
            rc = -1;
        else
            rc = Socket_new(ip_address, addr_len, port, &(aClient->net.socket), timeout);

    }
    if (rc == EINPROGRESS || rc == EWOULDBLOCK)
        aClient->connect_state = TCP_IN_PROGRESS; /* TCP connect called - wait for connect completion */
    else if (rc == 0)
    {	/* TCP connect completed. If SSL, send SSL connect */

        if (aClient->net.http_proxy) {
            aClient->connect_state = PROXY_CONNECT_IN_PROGRESS;
//			rc = Proxy_connect( &aClient->net, 0, ip_address);
        }
        if ( websocket )
        {
            rc = WebSocket_connect(&aClient->net, 0, ip_address);
            if ( rc == TCPSOCKET_INTERRUPTED )
                aClient->connect_state = WEBSOCKET_IN_PROGRESS; /* Websocket connect called - wait for completion */
        }
        if (rc == 0)
        {
            /* Now send the MQTT connect packet */
            if ((rc = MQTTPacket_send_connect(aClient, MQTTVersion, connectProperties, willProperties)) == 0)
                aClient->connect_state = WAIT_FOR_CONNACK; /* MQTT Connect sent - wait for CONNACK */
            else
                aClient->connect_state = NOT_IN_PROGRESS;
        }
    }

    exit:
    return rc;
}


int MQTTProtocol_handlePingresps(void* pack, SOCKET sock)
{
    Clients* client = NULL;
    int rc = TCPSOCKET_COMPLETE;

    client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 21, NULL, sock, client->clientID);
    client->ping_outstanding = 0;
    return rc;
}


int MQTTProtocol_subscribe(Clients* client, List* topics, List* qoss, int msgID,
                           MQTTSubscribe_options* opts, MQTTProperties* props)
{
    int rc = 0;
    rc = MQTTPacket_send_subscribe(topics, qoss, opts, props, msgID, 0, client);
    return rc;
}


int MQTTProtocol_handleSubacks(void* pack, SOCKET sock)
{
    Suback* suback = (Suback*)pack;
    Clients* client = NULL;
    int rc = TCPSOCKET_COMPLETE;

    client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 23, NULL, sock, client->clientID, suback->msgId);
    MQTTPacket_freeSuback(suback);
    return rc;
}

int MQTTProtocol_unsubscribe(Clients* client, List* topics, int msgID, MQTTProperties* props)
{
    int rc = 0;
    rc = MQTTPacket_send_unsubscribe(topics, props, msgID, 0, client);
    return rc;
}

int MQTTProtocol_handleUnsubacks(void* pack, SOCKET sock)
{
    Unsuback* unsuback = (Unsuback*)pack;
    Clients* client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 24, NULL, sock, client->clientID, unsuback->msgId);
    MQTTPacket_freeUnsuback(unsuback);
    return rc;
}

