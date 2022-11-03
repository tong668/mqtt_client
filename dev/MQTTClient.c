//
// Created by Administrator on 2022/11/3.
//

#include <utf-8.h>
#include <string.h>
#include <Log.h>
#include "Socket.h"
#include "WebSocket.h"
#include "MQTTTime.h"
#include "SocketBuffer.h"
#include "MQTTClient.h"
#include "Thread.h"
#include "MQTTProtocol.h"
#include "MQTTPacket.h"


static ClientStates ClientState =
        {
                CLIENT_VERSION, /* version */
                NULL /* client list */
        };

ClientStates *bstate = &ClientState;

MQTTProtocol state;

static pthread_mutex_t mqttclient_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t* mqttclient_mutex = &mqttclient_mutex_store;

static pthread_mutex_t socket_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t* socket_mutex = &socket_mutex_store;

static pthread_mutex_t subscribe_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t* subscribe_mutex = &subscribe_mutex_store;

static pthread_mutex_t unsubscribe_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t* unsubscribe_mutex = &unsubscribe_mutex_store;

static pthread_mutex_t connect_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t* connect_mutex = &connect_mutex_store;


static volatile int library_initialized = 0;
static List *handles = NULL;
static int running = 0;
static int tostop = 0;
static pthread_t run_id = 0;

static int retryLoopIntervalms = 5000;


typedef struct {
    char *serverURI;
    const char *currentServerURI; /* when using HA options, set the currently used serverURI */
    int websocket;
    Clients *c;
    MQTTClient_connectionLost *cl;
    MQTTClient_messageArrived *ma;
    MQTTClient_deliveryComplete *dc;
    void *context;

    MQTTClient_disconnected *disconnected;
    void *disconnected_context; /* the context to be associated with the disconnected callback*/

    MQTTClient_published *published;
    void *published_context; /* the context to be associated with the disconnected callback*/
    sem_t* connect_sem;
    int rc; /* getsockopt return code in connect */
    sem_t* connack_sem;
    sem_t* suback_sem;
    sem_t* unsuback_sem;
    MQTTPacket *pack;

    unsigned long commandTimeout;
} MQTTClients;


static void MQTTClient_emptyMessageQueue(Clients *client);

static int clientSockCompare(void *a, void *b);

static void*  connectionLost_call(void *context);

static int MQTTClient_stop(void);

static void MQTTClient_closeSession(Clients *client, enum MQTTReasonCodes reason, MQTTProperties *props);

static int MQTTClient_cleanSession(Clients *client);

static int
MQTTClient_disconnect1(MQTTClient handle, int timeout, int internal, int stop, enum MQTTReasonCodes, MQTTProperties *);

static int MQTTClient_disconnect_internal(MQTTClient handle, int timeout);

static void MQTTClient_retry(void);

static MQTTPacket *MQTTClient_cycle(SOCKET *sock, uint64_t timeout, int *rc);


/*static int pubCompare(void* a, void* b); */
static void MQTTProtocol_checkPendingWrites(void);

static void MQTTClient_writeComplete(SOCKET socket, int rc);


int MQTTClient_createWithOptions(MQTTClient *handle, const char *serverURI, const char *clientId,
                                 int persistence_type, void *persistence_context, MQTTClient_createOptions *options) {
    int rc = 0;
    MQTTClients *m = NULL;
    if ((rc = Thread_lock_mutex(mqttclient_mutex)) != 0)
        goto exit;

    if (serverURI == NULL || clientId == NULL) {
        rc = MQTTCLIENT_NULL_PARAMETER;
        goto exit;
    }

    if (!UTF8_validateString(clientId)) {
        rc = MQTTCLIENT_BAD_UTF8_STRING;
        goto exit;
    }

    if (strlen(clientId) == 0 && persistence_type == MQTTCLIENT_PERSISTENCE_DEFAULT) {
        rc = MQTTCLIENT_PERSISTENCE_ERROR;
        goto exit;
    }

    if (strstr(serverURI, "://") != NULL) {
        if (strncmp(URI_TCP, serverURI, strlen(URI_TCP)) != 0
            && strncmp(URI_WS, serverURI, strlen(URI_WS)) != 0
                ) {
            rc = MQTTCLIENT_BAD_PROTOCOL;
            goto exit;
        }
    }

    if (options && (strncmp(options->struct_id, "MQCO", 4) != 0 || options->struct_version != 0)) {
        rc = MQTTCLIENT_BAD_STRUCTURE;
        goto exit;
    }

    if (!library_initialized) {
        Log_initialize((Log_nameValue *) MQTTClient_getVersionInfo());
        bstate->clients = ListInitialize();
        Socket_outInitialize();
        Socket_setWriteCompleteCallback(MQTTClient_writeComplete);
        Socket_setWriteAvailableCallback(MQTTProtocol_writeAvailable);
        handles = ListInitialize();
        library_initialized = 1;
    }

    if ((m = malloc(sizeof(MQTTClients))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    *handle = m;
    memset(m, '\0', sizeof(MQTTClients));
    m->commandTimeout = 10000L;
    if (strncmp(URI_TCP, serverURI, strlen(URI_TCP)) == 0)
        serverURI += strlen(URI_TCP);
    else if (strncmp(URI_WS, serverURI, strlen(URI_WS)) == 0) {
        serverURI += strlen(URI_WS);
        m->websocket = 1;
    } else if (strncmp(URI_SSL, serverURI, strlen(URI_SSL)) == 0) {

        rc = MQTTCLIENT_SSL_NOT_SUPPORTED;
        goto exit;
    } else if (strncmp(URI_WSS, serverURI, strlen(URI_WSS)) == 0) {

        rc = MQTTCLIENT_SSL_NOT_SUPPORTED;
        goto exit;
    }
    m->serverURI = MQTTStrdup(serverURI);
    ListAppend(handles, m, sizeof(MQTTClients));

    if ((m->c = malloc(sizeof(Clients))) == NULL) {
        ListRemove(handles, m);
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    memset(m->c, '\0', sizeof(Clients));
    m->c->context = m;
    m->c->MQTTVersion = (options) ? options->MQTTVersion : MQTTVERSION_3_1_1;
    m->c->outboundMsgs = ListInitialize();
    m->c->inboundMsgs = ListInitialize();
    m->c->messageQueue = ListInitialize();
    m->c->outboundQueue = ListInitialize();
    m->c->clientID = MQTTStrdup(clientId);
    m->connect_sem = Thread_create_sem(&rc);
    m->connack_sem = Thread_create_sem(&rc);
    m->suback_sem = Thread_create_sem(&rc);
    m->unsuback_sem = Thread_create_sem(&rc);

    ListAppend(bstate->clients, m->c, sizeof(Clients) + 3 * sizeof(List));

    exit:
    Thread_unlock_mutex(mqttclient_mutex);
    return rc;
}


int MQTTClient_create(MQTTClient *handle, const char *serverURI, const char *clientId,
                      int persistence_type, void *persistence_context) {
    return MQTTClient_createWithOptions(handle, serverURI, clientId, persistence_type,
                                        persistence_context, NULL);
}


static void MQTTClient_emptyMessageQueue(Clients *client) {

}


static int clientSockCompare(void *a, void *b) {
    MQTTClients *m = (MQTTClients *) a;
    return m->c->net.socket == *(int *) b;
}

static void* connectionLost_call(void *context) {
    MQTTClients *m = (MQTTClients *) context;

    (*(m->cl))(m->context, NULL);
    return 0;
}

static int MQTTClient_stop(void) {
    int rc = 0;

//    if (running == 1 && tostop == 0) {
//        int conn_count = 0;
//        ListElement *current = NULL;
//
//        if (handles != NULL) {
//            /* find out how many handles are still connected */
//            while (ListNextElement(handles, &current)) {
//                if (((MQTTClients *) (current->content))->c->connect_state > NOT_IN_PROGRESS ||
//                    ((MQTTClients *) (current->content))->c->connected)
//                    ++conn_count;
//            }
//        }
//        Log(TRACE_MIN, -1, "Conn_count is %d", conn_count);
//        /* stop the background thread, if we are the last one to be using it */
//        if (conn_count == 0) {
//            int count = 0;
//            tostop = 1;
//            if (Thread_getid() != run_id) {
//                while (running && ++count < 100) {
//                    Thread_unlock_mutex(mqttclient_mutex);
//                    Log(TRACE_MIN, -1, "sleeping");
//                    MQTTTime_sleep(100L);
//                    Thread_lock_mutex(mqttclient_mutex);
//                }
//            }
//            rc = 1;
//        }
//    }
    return rc;
}


static void MQTTClient_closeSession(Clients *client, enum MQTTReasonCodes reason, MQTTProperties *props) {
    client->good = 0;
    client->ping_outstanding = 0;
    client->ping_due = 0;
    if (client->net.socket > 0) {
        if (client->connected)
            MQTTPacket_send_disconnect(client, reason, props);
        Thread_lock_mutex(socket_mutex);
        WebSocket_close(&client->net, WebSocket_CLOSE_NORMAL, NULL);

        Socket_close(client->net.socket);
        Thread_unlock_mutex(socket_mutex);
        client->net.socket = 0;
    }
    client->connected = 0;
    client->connect_state = NOT_IN_PROGRESS;

    if (client->MQTTVersion < 5/*MQTTVERSION_5*/ && client->cleansession)
        MQTTClient_cleanSession(client);
}


static int MQTTClient_cleanSession(Clients *client) {
    int rc = 0;
    MQTTProtocol_emptyMessageList(client->inboundMsgs);
    MQTTProtocol_emptyMessageList(client->outboundMsgs);
    MQTTClient_emptyMessageQueue(client);
    client->msgID = 0;
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



/**
 * mqttclient_mutex must be locked when you call this function, if multi threaded
 */
static int MQTTClient_disconnect1(MQTTClient handle, int timeout, int call_connection_lost, int stop,
                                  enum MQTTReasonCodes reason, MQTTProperties *props) {
    MQTTClients *m = handle;
    struct timeval start;
    int rc = MQTTCLIENT_SUCCESS;
    int was_connected = 0;
    if (m == NULL || m->c == NULL) {
        rc = MQTTCLIENT_FAILURE;
        goto exit;
    }
    was_connected = m->c->connected; /* should be 1 */
    if (m->c->connected != 0) {
        start = MQTTTime_start_clock();
        m->c->connect_state = DISCONNECTING; /* indicate disconnecting */
        while (m->c->inboundMsgs->count > 0 ||
               m->c->outboundMsgs->count > 0) { /* wait for all inflight message flows to finish, up to timeout */
            if (MQTTTime_elapsed(start) >= (uint64_t) timeout)
                break;
            Thread_unlock_mutex(mqttclient_mutex);
            MQTTClient_yield();
            Thread_lock_mutex(mqttclient_mutex);
        }
    }

    MQTTClient_closeSession(m->c, reason, props);

    exit:
    if (stop)
        MQTTClient_stop();
    if (call_connection_lost && m->cl && was_connected) {
        Log(TRACE_MIN, -1, "Calling connectionLost for client %s", m->c->clientID);
        Thread_start(connectionLost_call, m);
    }
    return rc;
}


/**
 * mqttclient_mutex must be locked when you call this function, if multi threaded
 */
static int MQTTClient_disconnect_internal(MQTTClient handle, int timeout) {
    return MQTTClient_disconnect1(handle, timeout, 1, 1, MQTTREASONCODE_SUCCESS, NULL);
}


/**
 * mqttclient_mutex must be locked when you call this function, if multi threaded
 */
void MQTTProtocol_closeSession(Clients *c, int sendwill) {
    MQTTClient_disconnect_internal((MQTTClient) c->context, 0);
}


static void MQTTClient_retry(void) {
    static struct timeval last = {0, 0};
    struct timeval now;
    now = MQTTTime_now();
    if (MQTTTime_difftime(now, last) >= (int64_t) (retryLoopIntervalms)) {
        last = MQTTTime_now();
        MQTTProtocol_keepalive(now);
        MQTTProtocol_retry(now, 1, 0);
    } else
        MQTTProtocol_retry(now, 0, 0);
}


static MQTTPacket *MQTTClient_cycle(SOCKET *sock, uint64_t timeout, int *rc) {
    static Ack ack;
    MQTTPacket *pack = NULL;
    int rc1 = 0;
    struct timeval start;

    start = MQTTTime_start_clock();
    *sock = Socket_getReadySocket(0, (int) timeout, socket_mutex, rc);
    *rc = rc1;
    if (*sock == 0 && timeout >= 100L && MQTTTime_elapsed(start) < (int64_t) 10)
        MQTTTime_sleep(100L);
    Thread_lock_mutex(mqttclient_mutex);
    if (*sock > 0 && rc1 == 0) {
        MQTTClients *m = NULL;
        if (ListFindItem(handles, sock, clientSockCompare) != NULL)
            m = (MQTTClient) (handles->current->content);
        if (m != NULL) {
            if (m->c->connect_state == TCP_IN_PROGRESS || m->c->connect_state == SSL_IN_PROGRESS)
                *rc = 0;  /* waiting for connect state to clear */
            else if (m->c->connect_state == WEBSOCKET_IN_PROGRESS)
                *rc = WebSocket_upgrade(&m->c->net);
            else {
                pack = MQTTPacket_Factory(m->c->MQTTVersion, &m->c->net, rc);
                if (*rc == TCPSOCKET_INTERRUPTED)
                    *rc = 0;
            }
        }

        if (pack) {
            int freed = 1;

            /* Note that these handle... functions free the packet structure that they are dealing with */
            if (pack->header.bits.type == PUBLISH)
                *rc = MQTTProtocol_handlePublishes(pack, *sock);
            else if (pack->header.bits.type == PUBACK || pack->header.bits.type == PUBCOMP) {
                int msgid;

                ack = (pack->header.bits.type == PUBCOMP) ? *(Pubcomp *) pack : *(Puback *) pack;
                msgid = ack.msgId;
                *rc = (pack->header.bits.type == PUBCOMP) ?
                      MQTTProtocol_handlePubcomps(pack, *sock) : MQTTProtocol_handlePubacks(pack, *sock);
                if (m && m->dc) {
                    Log(TRACE_MIN, -1, "Calling deliveryComplete for client %s, msgid %d", m->c->clientID, msgid);
                    (*(m->dc))(m->context, msgid);
                }
            } else if (pack->header.bits.type == PUBREC) {
                Pubrec *pubrec = (Pubrec *) pack;

                *rc = MQTTProtocol_handlePubrecs(pack, *sock);
            } else if (pack->header.bits.type == PUBREL)
                *rc = MQTTProtocol_handlePubrels(pack, *sock);
            else if (pack->header.bits.type == PINGRESP)
                *rc = MQTTProtocol_handlePingresps(pack, *sock);
            else
                freed = 0;
            if (freed)
                pack = NULL;
        }
    }
    MQTTClient_retry();
    Thread_unlock_mutex(mqttclient_mutex);
    return pack;
}


void MQTTClient_yield(void) {
    struct timeval start = MQTTTime_start_clock();
    uint64_t elapsed = 0L;
    uint64_t timeout = 100L;
    int rc = 0;
    if (running) /* yield is not meant to be called in a multi-thread environment */
    {
        MQTTTime_sleep(timeout);
        goto exit;
    }

    elapsed = MQTTTime_elapsed(start);
    do {
        SOCKET sock = -1;
        MQTTClient_cycle(&sock, (timeout > elapsed) ? timeout - elapsed : 0L, &rc);
        Thread_lock_mutex(mqttclient_mutex);
        if (rc == SOCKET_ERROR && ListFindItem(handles, &sock, clientSockCompare)) {
            MQTTClients *m = (MQTTClient) (handles->current->content);
            if (m->c->connect_state != DISCONNECTING)
                MQTTClient_disconnect_internal(m, 0);
        }
        Thread_unlock_mutex(mqttclient_mutex);
        elapsed = MQTTTime_elapsed(start);
    } while (elapsed < timeout);
    exit:
    return;
}


MQTTClient_nameValue *MQTTClient_getVersionInfo(void) {
#define MAX_INFO_STRINGS 8
    static MQTTClient_nameValue libinfo[MAX_INFO_STRINGS + 1];
    int i = 0;

    libinfo[i].name = "Product name";
    libinfo[i++].value = "Eclipse Paho Synchronous MQTT C Client Library";

    libinfo[i].name = "Version";
    libinfo[i++].value = CLIENT_VERSION;

    libinfo[i].name = "Build level";
    libinfo[i++].value = BUILD_TIMESTAMP;
    libinfo[i].name = NULL;
    libinfo[i].value = NULL;
    return libinfo;
}

static void MQTTProtocol_checkPendingWrites(void) {
    if (state.pending_writes.count > 0) {
        ListElement *le = state.pending_writes.first;
        while (le) {
            if (Socket_noPendingWrites(((pending_write *) (le->content))->socket)) {
                MQTTProtocol_removePublication(((pending_write *) (le->content))->p);
                state.pending_writes.current = le;
                ListRemove(&(state.pending_writes), le->content); /* does NextElement itself */
                le = state.pending_writes.current;
            } else
                ListNextElement(&(state.pending_writes), &le);
        }
    }
}


static void MQTTClient_writeComplete(SOCKET socket, int rc) {
    ListElement *found = NULL;
    /* a partial write is now complete for a socket - this will be on a publish*/

    MQTTProtocol_checkPendingWrites();

    /* find the client using this socket */
    if ((found = ListFindItem(handles, &socket, clientSockCompare)) != NULL) {
        MQTTClients *m = (MQTTClients *) (found->content);

        m->c->net.lastSent = MQTTTime_now();
    }
}
