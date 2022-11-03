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
extern ClientStates *bstate;

static void MQTTProtocol_storeQoS0(Clients *pubclient, Publish *publish);

static void MQTTProtocol_retries(struct timeval now, Clients *client, int regardless);

static int MQTTProtocol_queueAck(Clients *client, int ackType, int msgId);

typedef struct {
    int messageId;
    int ackType;
} AckRequest;

int messageIDCompare(void *a, void *b) {
    Messages *msg = (Messages *) a;
    return msg->msgid == *(int *) b;
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
    client = (Clients *) (ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
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
            rc = MQTTPacket_send_puback(publish->MQTTVersion, publish->msgId, &client->net, client->clientID);
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
    client = (Clients *) (ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
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


/**
 * Process an incoming pubrec packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePubrecs(void *pack, SOCKET sock) {
    Pubrec *pubrec = (Pubrec *) pack;
    Clients *client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    int send_pubrel = 1; /* boolean to send PUBREL or not */

    client = (Clients *) (ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 15, NULL, sock, client->clientID, pubrec->msgId);

    /* look for the message by message id in the records of outbound messages for this client */
    client->outboundMsgs->current = NULL;
    if (ListFindItem(client->outboundMsgs, &(pubrec->msgId), messageIDCompare) == NULL) {
        if (pubrec->header.bits.dup == 0)
            Log(TRACE_MIN, 3, NULL, "PUBREC", client->clientID, pubrec->msgId);
    } else {
        Messages *m = (Messages *) (client->outboundMsgs->current->content);
        if (m->qos != 2) {
            if (pubrec->header.bits.dup == 0)
                Log(TRACE_MIN, 4, NULL, "PUBREC", client->clientID, pubrec->msgId, m->qos);
        } else if (m->nextMessageType != PUBREC) {
            if (pubrec->header.bits.dup == 0)
                Log(TRACE_MIN, 5, NULL, "PUBREC", client->clientID, pubrec->msgId);
        } else {
            {
                m->nextMessageType = PUBCOMP;
                m->lastTouch = MQTTTime_now();
            }
        }
    }

    free(pack);
    return rc;
}


/**
 * Process an incoming pubrel packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePubrels(void *pack, SOCKET sock) {
    Pubrel *pubrel = (Pubrel *) pack;
    Clients *client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    client = (Clients *) (ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 17, NULL, sock, client->clientID, pubrel->msgId);

    /* look for the message by message id in the records of inbound messages for this client */
    if (ListFindItem(client->inboundMsgs, &(pubrel->msgId), messageIDCompare) == NULL) {
        if (pubrel->header.bits.dup == 0)
            Log(TRACE_MIN, 3, NULL, "PUBREL", client->clientID, pubrel->msgId);
    } else {
        Messages *m = (Messages *) (client->inboundMsgs->current->content);
        if (m->qos != 2)
            Log(TRACE_MIN, 4, NULL, "PUBREL", client->clientID, pubrel->msgId, m->qos);
        else if (m->nextMessageType != PUBREL)
            Log(TRACE_MIN, 5, NULL, "PUBREL", client->clientID, pubrel->msgId);
        else {
            Publish publish;

            memset(&publish, '\0', sizeof(publish));

            publish.header.bits.qos = m->qos;
            publish.header.bits.retain = m->retain;
            publish.msgId = m->msgid;
            if (m->publish) {
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

int MQTTProtocol_handlePubcomps(void *pack, SOCKET sock) {
    Pubcomp *pubcomp = (Pubcomp *) pack;
    Clients *client = NULL;
    int rc = TCPSOCKET_COMPLETE;
    client = (Clients *) (ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 19, NULL, sock, client->clientID, pubcomp->msgId);

    /* look for the message by message id in the records of outbound messages for this client */
    if (ListFindItem(client->outboundMsgs, &(pubcomp->msgId), messageIDCompare) == NULL) {
        if (pubcomp->header.bits.dup == 0)
            Log(TRACE_MIN, 3, NULL, "PUBCOMP", client->clientID, pubcomp->msgId);
    } else {
        Messages *m = (Messages *) (client->outboundMsgs->current->content);
        if (m->qos != 2)
            Log(TRACE_MIN, 4, NULL, "PUBCOMP", client->clientID, pubcomp->msgId, m->qos);
        else {
            if (m->nextMessageType != PUBCOMP)
                Log(TRACE_MIN, 5, NULL, "PUBCOMP", client->clientID, pubcomp->msgId);
            else {
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

void MQTTProtocol_keepalive(struct timeval now) {
    ListElement *current = NULL;

    ListNextElement(bstate->clients, &current);
    while (current) {
        Clients *client = (Clients *) (current->content);
        ListNextElement(bstate->clients, &current);

        if (client->connected == 0 || client->keepAliveInterval == 0)
            continue;

        if (client->ping_outstanding == 1) {
            if (MQTTTime_difftime(now, client->net.lastPing) >= (int64_t) (client->keepAliveInterval * 1000)) {
                Log(TRACE_PROTOCOL, -1,
                    "PINGRESP not received in keepalive interval for client %s on socket %d, disconnecting",
                    client->clientID, client->net.socket);
                MQTTProtocol_closeSession(client, 1);
            }
        } else if (client->ping_due == 1 &&
                   (MQTTTime_difftime(now, client->ping_due_time) >= (int64_t) (client->keepAliveInterval * 1000))) {
            /* ping still outstanding after keep alive interval, so close session */
            Log(TRACE_PROTOCOL, -1, "PINGREQ still outstanding for client %s on socket %d, disconnecting",
                client->clientID, client->net.socket);
            MQTTProtocol_closeSession(client, 1);

        } else if (MQTTTime_difftime(now, client->net.lastSent) >= (int64_t) (client->keepAliveInterval * 1000) ||
                   MQTTTime_difftime(now, client->net.lastReceived) >= (int64_t) (client->keepAliveInterval * 1000)) {
            if (Socket_noPendingWrites(client->net.socket)) {
                if (MQTTPacket_send_pingreq(&client->net, client->clientID) != TCPSOCKET_COMPLETE) {
                    Log(TRACE_PROTOCOL, -1, "Error sending PINGREQ for client %s on socket %d, disconnecting",
                        client->clientID, client->net.socket);
                    MQTTProtocol_closeSession(client, 1);
                } else {
                    client->ping_due = 0;
                    client->net.lastPing = now;
                    client->ping_outstanding = 1;
                }
            } else if (client->ping_due == 0) {
                Log(TRACE_PROTOCOL, -1, "Couldn't send PINGREQ for client %s on socket %d, noting",
                    client->clientID, client->net.socket);
                client->ping_due = 1;
                client->ping_due_time = now;
            }
        }
    }
}

static void MQTTProtocol_retries(struct timeval now, Clients *client, int regardless) {
    ListElement *outcurrent = NULL;
    if (!regardless && client->retryInterval <= 0 && /* 0 or -ive retryInterval turns off retry except on reconnect */
        client->connect_sent == client->connect_count)
        goto exit;

    if (regardless)
        client->connect_count = client->outboundMsgs->count; /* remember the number of messages to retry on connect */
    else if (client->connect_sent <
             client->connect_count) /* continue a connect retry which didn't complete first time around */
        regardless = 1;

    while (client && ListNextElement(client->outboundMsgs, &outcurrent) &&
           client->connected && client->good &&        /* client is connected and has no errors */
           Socket_noPendingWrites(
                   client->net.socket)) /* there aren't any previous packets still stacked up on the socket */
    {
        Messages *m = (Messages *) (outcurrent->content);
        if (regardless || MQTTTime_difftime(now, m->lastTouch) > (int64_t) (max(client->retryInterval, 10) * 1000)) {
            if (regardless)
                ++client->connect_sent;
            if (m->qos == 1 || (m->qos == 2 && m->nextMessageType == PUBREC)) {
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
                memcpy(m->publish->mask, publish.mask,
                       sizeof(m->publish->mask)); /* store websocket mask used in send */
                if (rc == SOCKET_ERROR) {
                    client->good = 0;
                    Log(TRACE_PROTOCOL, 29, NULL, client->clientID, client->net.socket,
                        Socket_getpeer(client->net.socket));
                    MQTTProtocol_closeSession(client, 1);
                    client = NULL;
                } else {
                    if (m->qos == 0 && rc == TCPSOCKET_INTERRUPTED)
                        MQTTProtocol_storeQoS0(client, &publish);
                    m->lastTouch = MQTTTime_now();
                }
            } else if (m->qos && m->nextMessageType == PUBCOMP) {
                Log(TRACE_MIN, 7, NULL, "PUBREL", client->clientID, client->net.socket, m->msgid);
            }
        }
    }
    exit:
    return;
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

void MQTTProtocol_retry(struct timeval now, int doRetry, int regardless) {
    ListElement *current = NULL;

    ListNextElement(bstate->clients, &current);
    /* look through the outbound message list of each client, checking to see if a retry is necessary */
    while (current) {
        Clients *client = (Clients *) (current->content);
        ListNextElement(bstate->clients, &current);
        if (client->connected == 0)
            continue;
        if (client->good == 0) {
            MQTTProtocol_closeSession(client, 1);
            continue;
        }
        if (Socket_noPendingWrites(client->net.socket) == 0)
            continue;
        if (doRetry)
            MQTTProtocol_retries(now, client, regardless);
    }
}


void MQTTProtocol_emptyMessageList(List *msgList) {
    ListElement *current = NULL;

    while (ListNextElement(msgList, &current)) {
        Messages *m = (Messages *) (current->content);
        MQTTProtocol_removePublication(m->publish);
    }
    ListEmpty(msgList);
}


void MQTTProtocol_writeAvailable(SOCKET socket) {
    Clients *client = NULL;
    ListElement *current = NULL;
    int rc = 0;

    client = (Clients *) (ListFindItem(bstate->clients, &socket, clientSocketCompare)->content);

    current = NULL;
    while (ListNextElement(client->outboundQueue, &current) && rc == 0) {
        AckRequest *ackReq = (AckRequest *) (current->content);

        switch (ackReq->ackType) {
            case PUBACK:
                rc = MQTTPacket_send_puback(client->MQTTVersion, ackReq->messageId, &client->net, client->clientID);
                break;
            default:
                Log(LOG_ERROR, -1, "unknown ACK type %d, dropping msg", ackReq->ackType);
                break;
        }
    }

    ListEmpty(client->outboundQueue);
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


int MQTTProtocol_handlePingresps(void *pack, SOCKET sock) {
    Clients *client = NULL;
    int rc = TCPSOCKET_COMPLETE;

    client = (Clients *) (ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
    Log(LOG_PROTOCOL, 21, NULL, sock, client->clientID);
    client->ping_outstanding = 0;
    return rc;
}
