//
// Created by Administrator on 2022/11/3.
//

#include <utf-8.h>
#include <string.h>
#include <Log.h>
#include <Socket.h>
#include <WebSocket.h>
#include <MQTTTime.h>
#include <SocketBuffer.h>
#include "MQTTClient.h"
#include "utils/TypeDefine.h"
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
static mutex_type mqttclient_mutex = &mqttclient_mutex_store;

static pthread_mutex_t socket_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static mutex_type socket_mutex = &socket_mutex_store;

static pthread_mutex_t subscribe_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static mutex_type subscribe_mutex = &subscribe_mutex_store;

static pthread_mutex_t unsubscribe_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static mutex_type unsubscribe_mutex = &unsubscribe_mutex_store;

static pthread_mutex_t connect_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static mutex_type connect_mutex = &connect_mutex_store;


static volatile int library_initialized = 0;
static List *handles = NULL;
static int running = 0;
static int tostop = 0;
static pthread_t run_id = 0;


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
    sem_type connect_sem;
    int rc; /* getsockopt return code in connect */
    sem_type connack_sem;
    sem_type suback_sem;
    sem_type unsuback_sem;
    MQTTPacket *pack;

    unsigned long commandTimeout;
} MQTTClients;


static void MQTTClient_terminate(void);

static void MQTTClient_emptyMessageQueue(Clients *client);

static int clientSockCompare(void *a, void *b);

static void*  connectionLost_call(void *context);

static void*  MQTTClient_run(void *n);

static int MQTTClient_stop(void);

static void MQTTClient_closeSession(Clients *client, enum MQTTReasonCodes reason, MQTTProperties *props);

static int MQTTClient_cleanSession(Clients *client);

static MQTTResponse MQTTClient_connectURIVersion(
        MQTTClient handle, MQTTClient_connectOptions *options,
        const char *serverURI, int MQTTVersion,
        START_TIME_TYPE start, ELAPSED_TIME_TYPE millisecsTimeout,
        MQTTProperties *connectProperties, MQTTProperties *willProperties);

static MQTTResponse MQTTClient_connectURI(MQTTClient handle, MQTTClient_connectOptions *options, const char *serverURI,
                                          MQTTProperties *connectProperties, MQTTProperties *willProperties);

static int
MQTTClient_disconnect1(MQTTClient handle, int timeout, int internal, int stop, enum MQTTReasonCodes, MQTTProperties *);

static int MQTTClient_disconnect_internal(MQTTClient handle, int timeout);

static void MQTTClient_retry(void);

static MQTTPacket *MQTTClient_cycle(SOCKET *sock, ELAPSED_TIME_TYPE timeout, int *rc);

static MQTTPacket *MQTTClient_waitfor(MQTTClient handle, int packet_type, int *rc, int64_t timeout);

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


static void MQTTClient_terminate(void) {
    MQTTClient_stop();
    if (library_initialized) {
        ListFree(bstate->clients);
        ListFree(handles);
        handles = NULL;
        WebSocket_terminate();
        Log_terminate();
        library_initialized = 0;
    }
}


static void MQTTClient_emptyMessageQueue(Clients *client) {
    /* empty message queue */
    if (client->messageQueue->count > 0) {
        ListElement *current = NULL;
        while (ListNextElement(client->messageQueue, &current)) {
            qEntry *qe = (qEntry *) (current->content);
            free(qe->topicName);
            MQTTProperties_free(&qe->msg->properties);
            free(qe->msg->payload);
            free(qe->msg);
        }
        ListEmpty(client->messageQueue);
    }
}


void MQTTClient_destroy(MQTTClient *handle) {
    MQTTClients *m = *handle;
    Thread_lock_mutex(connect_mutex);
    Thread_lock_mutex(mqttclient_mutex);

    if (m == NULL)
        goto exit;

    if (m->c) {
        SOCKET saved_socket = m->c->net.socket;
        char *saved_clientid = MQTTStrdup(m->c->clientID);
        MQTTClient_emptyMessageQueue(m->c);
        MQTTProtocol_freeClient(m->c);
        if (!ListRemove(bstate->clients, m->c))
            Log(LOG_ERROR, 0, NULL);
        else
            Log(TRACE_MIN, 1, NULL, saved_clientid, saved_socket);
        free(saved_clientid);
    }
    if (m->serverURI)
        free(m->serverURI);
    Thread_destroy_sem(m->connect_sem);
    Thread_destroy_sem(m->connack_sem);
    Thread_destroy_sem(m->suback_sem);
    Thread_destroy_sem(m->unsuback_sem);
    if (!ListRemove(handles, m))
        Log(LOG_ERROR, -1, "free error");
    *handle = NULL;
    if (bstate->clients->count == 0)
        MQTTClient_terminate();

    exit:
    Thread_unlock_mutex(mqttclient_mutex);
    Thread_unlock_mutex(connect_mutex);
}


void MQTTClient_freeMessage(MQTTClient_message **message) {
    MQTTProperties_free(&(*message)->properties);
    free((*message)->payload);
    free(*message);
    *message = NULL;
}


void MQTTClient_free(void *memory) {
    free(memory);
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

/* This is the thread function that handles the calling of callback functions if set */
static void*  MQTTClient_run(void *n) {
    long timeout = 10L; /* first time in we have a small timeout.  Gets things started more quickly */

    running = 1;
    run_id = Thread_getid();

    Thread_lock_mutex(mqttclient_mutex);
    while (!tostop) {
        int rc = SOCKET_ERROR;
        SOCKET sock = -1;
        MQTTClients *m = NULL;
        MQTTPacket *pack = NULL;

        Thread_unlock_mutex(mqttclient_mutex);
        pack = MQTTClient_cycle(&sock, timeout, &rc);
        Thread_lock_mutex(mqttclient_mutex);
        if (tostop)
            break;
        timeout = 100L;

        /* find client corresponding to socket */
        if (ListFindItem(handles, &sock, clientSockCompare) == NULL) {
            /* assert: should not happen */
            continue;
        }
        m = (MQTTClient) (handles->current->content);
        if (m == NULL) {
            /* assert: should not happen */
            continue;
        }
        if (rc == SOCKET_ERROR) {
            if (m->c->connected)
                MQTTClient_disconnect_internal(m, 0);
            else {
                if (m->c->connect_state == SSL_IN_PROGRESS) {
                    Log(TRACE_MIN, -1, "Posting connect semaphore for client %s", m->c->clientID);
                    m->c->connect_state = NOT_IN_PROGRESS;
                    Thread_post_sem(m->connect_sem);
                }
                if (m->c->connect_state == WAIT_FOR_CONNACK) {
                    Log(TRACE_MIN, -1, "Posting connack semaphore for client %s", m->c->clientID);
                    m->c->connect_state = NOT_IN_PROGRESS;
                    Thread_post_sem(m->connack_sem);
                }
            }
        } else {
            if (m->c->messageQueue->count > 0 && m->ma) {
                qEntry *qe = (qEntry *) (m->c->messageQueue->first->content);
                int topicLen = qe->topicLen;

                if (strlen(qe->topicName) == topicLen)
                    topicLen = 0;

                Log(TRACE_MIN, -1, "Calling messageArrived for client %s, queue depth %d",
                    m->c->clientID, m->c->messageQueue->count);
                Thread_unlock_mutex(mqttclient_mutex);
                rc = (*(m->ma))(m->context, qe->topicName, topicLen, qe->msg);
                Thread_lock_mutex(mqttclient_mutex);
                /* if 0 (false) is returned by the callback then it failed, so we don't remove the message from
                 * the queue, and it will be retried later.  If 1 is returned then the message data may have been freed,
                 * so we must be careful how we use it.
                 */
                if (rc) {
                    ListRemove(m->c->messageQueue, qe);
                } else
                    Log(TRACE_MIN, -1, "False returned from messageArrived for client %s, message remains on queue",
                        m->c->clientID);
            }
            if (pack) {
                if (pack->header.bits.type == CONNACK) {
                    Log(TRACE_MIN, -1, "Posting connack semaphore for client %s", m->c->clientID);
                    m->pack = pack;
                    Thread_post_sem(m->connack_sem);
                } else if (pack->header.bits.type == SUBACK) {
                    Log(TRACE_MIN, -1, "Posting suback semaphore for client %s", m->c->clientID);
                    m->pack = pack;
                    Thread_post_sem(m->suback_sem);
                } else if (pack->header.bits.type == UNSUBACK) {
                    Log(TRACE_MIN, -1, "Posting unsuback semaphore for client %s", m->c->clientID);
                    m->pack = pack;
                    Thread_post_sem(m->unsuback_sem);
                }
            } else if (m->c->connect_state == TCP_IN_PROGRESS) {
                int error;
                socklen_t len = sizeof(error);

                if ((m->rc = getsockopt(m->c->net.socket, SOL_SOCKET, SO_ERROR, (char *) &error, &len)) == 0)
                    m->rc = error;
                Log(TRACE_MIN, -1, "Posting connect semaphore for client %s rc %d", m->c->clientID, m->rc);
                m->c->connect_state = NOT_IN_PROGRESS;
                Thread_post_sem(m->connect_sem);
            }
            else if (m->c->connect_state == WEBSOCKET_IN_PROGRESS) {
                if (rc != TCPSOCKET_INTERRUPTED) {
                    Log(TRACE_MIN, -1, "Posting websocket handshake for client %s rc %d", m->c->clientID, m->rc);
                    m->c->connect_state = WAIT_FOR_CONNACK;
                    Thread_post_sem(m->connect_sem);
                }
            }
        }
    }
    run_id = 0;
    running = tostop = 0;
    Thread_unlock_mutex(mqttclient_mutex);
    return 0;
}


static int MQTTClient_stop(void) {
    int rc = 0;

    if (running == 1 && tostop == 0) {
        int conn_count = 0;
        ListElement *current = NULL;

        if (handles != NULL) {
            /* find out how many handles are still connected */
            while (ListNextElement(handles, &current)) {
                if (((MQTTClients *) (current->content))->c->connect_state > NOT_IN_PROGRESS ||
                    ((MQTTClients *) (current->content))->c->connected)
                    ++conn_count;
            }
        }
        Log(TRACE_MIN, -1, "Conn_count is %d", conn_count);
        /* stop the background thread, if we are the last one to be using it */
        if (conn_count == 0) {
            int count = 0;
            tostop = 1;
            if (Thread_getid() != run_id) {
                while (running && ++count < 100) {
                    Thread_unlock_mutex(mqttclient_mutex);
                    Log(TRACE_MIN, -1, "sleeping");
                    MQTTTime_sleep(100L);
                    Thread_lock_mutex(mqttclient_mutex);
                }
            }
            rc = 1;
        }
    }
    return rc;
}


int MQTTClient_setCallbacks(MQTTClient handle, void *context, MQTTClient_connectionLost *cl,
                            MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc) {
    int rc = MQTTCLIENT_SUCCESS;
    MQTTClients *m = handle;

    Thread_lock_mutex(mqttclient_mutex);

    if (m == NULL || ma == NULL || m->c->connect_state != NOT_IN_PROGRESS)
        rc = MQTTCLIENT_FAILURE;
    else {
        m->context = context;
        m->cl = cl;
        m->ma = ma;
        m->dc = dc;
    }

    Thread_unlock_mutex(mqttclient_mutex);
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


static MQTTResponse
MQTTClient_connectURIVersion(MQTTClient handle, MQTTClient_connectOptions *options, const char *serverURI,
                             int MQTTVersion,
                             START_TIME_TYPE start, ELAPSED_TIME_TYPE millisecsTimeout,
                             MQTTProperties *connectProperties, MQTTProperties *willProperties) {
    MQTTClients *m = handle;
    int rc = SOCKET_ERROR;
    int sessionPresent = 0;
    MQTTResponse resp = MQTTResponse_initializer;
    resp.reasonCode = SOCKET_ERROR;
    if (m->ma && !running) {
        Thread_start(MQTTClient_run, handle);
        if (MQTTTime_elapsed(start) >= millisecsTimeout) {
            rc = SOCKET_ERROR;
            goto exit;
        }
        MQTTTime_sleep(100L);
    }

    Log(TRACE_MIN, -1, "Connecting to serverURI %s with MQTT version %d", serverURI, MQTTVersion);
    rc = MQTTProtocol_connect(serverURI, m->c, m->websocket, MQTTVersion, connectProperties, willProperties,
                              millisecsTimeout - MQTTTime_elapsed(start));

    if (rc == SOCKET_ERROR)
        goto exit;

    if (m->c->connect_state == NOT_IN_PROGRESS) {
        rc = SOCKET_ERROR;
        goto exit;
    }

    if (m->c->connect_state == TCP_IN_PROGRESS) /* TCP connect started - wait for completion */
    {
        Thread_unlock_mutex(mqttclient_mutex);
        MQTTClient_waitfor(handle, CONNECT, &rc, millisecsTimeout - MQTTTime_elapsed(start));
        Thread_lock_mutex(mqttclient_mutex);
        if (rc != 0) {
            rc = SOCKET_ERROR;
            goto exit;
        }
        else {
            if (m->c->net.http_proxy) {
                m->c->connect_state = PROXY_CONNECT_IN_PROGRESS;

            }

            if (m->websocket) {
                m->c->connect_state = WEBSOCKET_IN_PROGRESS;
                if (WebSocket_connect(&m->c->net, 0, serverURI) == SOCKET_ERROR) {
                    rc = SOCKET_ERROR;
                    goto exit;
                }
            } else {
                m->c->connect_state = WAIT_FOR_CONNACK; /* TCP connect completed, in which case send the MQTT connect packet */
                if (MQTTPacket_send_connect(m->c, MQTTVersion, connectProperties, willProperties) == SOCKET_ERROR) {
                    rc = SOCKET_ERROR;
                    goto exit;
                }
            }
        }
    }

    if (m->c->connect_state == WEBSOCKET_IN_PROGRESS) /* websocket request sent - wait for upgrade */
    {
        Thread_unlock_mutex(mqttclient_mutex);
        MQTTClient_waitfor(handle, CONNECT, &rc, millisecsTimeout - MQTTTime_elapsed(start));
        Thread_lock_mutex(mqttclient_mutex);
        m->c->connect_state = WAIT_FOR_CONNACK; /* websocket upgrade complete */
        if (MQTTPacket_send_connect(m->c, MQTTVersion, connectProperties, willProperties) == SOCKET_ERROR) {
            rc = SOCKET_ERROR;
            goto exit;
        }
    }

    if (m->c->connect_state == WAIT_FOR_CONNACK) /* MQTT connect sent - wait for CONNACK */
    {
        MQTTPacket *pack = NULL;
        Thread_unlock_mutex(mqttclient_mutex);
        pack = MQTTClient_waitfor(handle, CONNACK, &rc, millisecsTimeout - MQTTTime_elapsed(start));
        Thread_lock_mutex(mqttclient_mutex);
        if (pack == NULL)
            rc = SOCKET_ERROR;
        else {
            Connack *connack = (Connack *) pack;
            Log(TRACE_PROTOCOL, 1, NULL, m->c->net.socket, m->c->clientID, connack->rc);
            if ((rc = connack->rc) == MQTTCLIENT_SUCCESS) {
                m->c->connected = 1;
                m->c->good = 1;
                m->c->connect_state = NOT_IN_PROGRESS;
                if (MQTTVersion == 4)
                    sessionPresent = connack->flags.bits.sessionPresent;
                if (m->c->cleansession || m->c->cleanstart)
                    rc = MQTTClient_cleanSession(m->c);
                if (m->c->outboundMsgs->count > 0) {
                    ListElement *outcurrent = NULL;
                    START_TIME_TYPE zero = START_TIME_ZERO;

                    while (ListNextElement(m->c->outboundMsgs, &outcurrent)) {
                        Messages *m = (Messages *) (outcurrent->content);
                        memset(&m->lastTouch, '\0', sizeof(m->lastTouch));
                    }
                    MQTTProtocol_retry(zero, 1, 1);
                    if (m->c->connected != 1)
                        rc = MQTTCLIENT_DISCONNECTED;
                }
            }
            MQTTPacket_freeConnack(connack);
            m->pack = NULL;
        }
    }
    exit:
    if (rc == MQTTCLIENT_SUCCESS) {
        if (options->struct_version >= 4) /* means we have to fill out return values */
        {
            options->returned.serverURI = serverURI;
            options->returned.MQTTVersion = MQTTVersion;
            options->returned.sessionPresent = sessionPresent;
        }
    } else
        MQTTClient_disconnect1(handle, 0, 0, (MQTTVersion == 3), MQTTREASONCODE_SUCCESS,
                               NULL); /* don't want to call connection lost */

    resp.reasonCode = rc;
    return resp;
}


static int retryLoopIntervalms = 5000;

void setRetryLoopInterval(int keepalive) {
    retryLoopIntervalms = (keepalive * 1000) / 10;

    if (retryLoopIntervalms < 100)
        retryLoopIntervalms = 100;
    else if (retryLoopIntervalms > 5000)
        retryLoopIntervalms = 5000;
}


static MQTTResponse MQTTClient_connectURI(MQTTClient handle, MQTTClient_connectOptions *options, const char *serverURI,
                                          MQTTProperties *connectProperties, MQTTProperties *willProperties) {
    MQTTClients *m = handle;
    START_TIME_TYPE start;
    ELAPSED_TIME_TYPE millisecsTimeout = 30000L;
    MQTTResponse rc = MQTTResponse_initializer;
    int MQTTVersion = 0;
    rc.reasonCode = SOCKET_ERROR;
    millisecsTimeout = options->connectTimeout * 1000;
    start = MQTTTime_start_clock();

    m->currentServerURI = serverURI;
    m->c->keepAliveInterval = options->keepAliveInterval;
    m->c->retryInterval = options->retryInterval;
    setRetryLoopInterval(options->keepAliveInterval);
    m->c->MQTTVersion = options->MQTTVersion;
    m->c->cleanstart = m->c->cleansession = 0;
    m->c->cleansession = options->cleansession;
    m->c->maxInflightMessages = (options->reliable) ? 1 : 10;
    if (options->struct_version >= 6) {
        if (options->maxInflightMessages > 0)
            m->c->maxInflightMessages = options->maxInflightMessages;
    }

    if (options->struct_version >= 7) {
        m->c->net.httpHeaders = options->httpHeaders;
    }
    if (options->struct_version >= 8) {
        if (options->httpProxy)
            m->c->httpProxy = MQTTStrdup(options->httpProxy);
        if (options->httpsProxy)
            m->c->httpsProxy = MQTTStrdup(options->httpsProxy);
    }

    if (m->c->will) {
        free(m->c->will->payload);
        free(m->c->will->topic);
        free(m->c->will);
        m->c->will = NULL;
    }

    if (options->will && (options->will->struct_version == 0 || options->will->struct_version == 1)) {
        const void *source = NULL;

        if ((m->c->will = malloc(sizeof(willMessages))) == NULL) {
            rc.reasonCode = PAHO_MEMORY_ERROR;
            goto exit;
        }
        if (options->will->message || (options->will->struct_version == 1 && options->will->payload.data)) {
            if (options->will->struct_version == 1 && options->will->payload.data) {
                m->c->will->payloadlen = options->will->payload.len;
                source = options->will->payload.data;
            } else {
                m->c->will->payloadlen = (int) strlen(options->will->message);
                source = (void *) options->will->message;
            }
            if ((m->c->will->payload = malloc(m->c->will->payloadlen)) == NULL) {
                free(m->c->will);
                rc.reasonCode = PAHO_MEMORY_ERROR;
                goto exit;
            }
            memcpy(m->c->will->payload, source, m->c->will->payloadlen);
        } else {
            m->c->will->payload = NULL;
            m->c->will->payloadlen = 0;
        }
        m->c->will->qos = options->will->qos;
        m->c->will->retained = options->will->retained;
        m->c->will->topic = MQTTStrdup(options->will->topicName);
    }

    if (m->c->username)
        free((void *) m->c->username);
    if (options->username)
        m->c->username = MQTTStrdup(options->username);
    if (m->c->password)
        free((void *) m->c->password);
    if (options->password) {
        m->c->password = MQTTStrdup(options->password);
        m->c->passwordlen = (int) strlen(options->password);
    } else if (options->struct_version >= 5 && options->binarypwd.data) {
        m->c->passwordlen = options->binarypwd.len;
        if ((m->c->password = malloc(m->c->passwordlen)) == NULL) {
            rc.reasonCode = PAHO_MEMORY_ERROR;
            goto exit;
        }
        memcpy((void *) m->c->password, options->binarypwd.data, m->c->passwordlen);
    }

    if (options->struct_version >= 3)
        MQTTVersion = options->MQTTVersion;
    rc = MQTTClient_connectURIVersion(handle, options, serverURI, MQTTVersion, start, millisecsTimeout,
                                      connectProperties, willProperties);

    exit:
    return rc;
}

MQTTResponse MQTTClient_connectAll(MQTTClient handle, MQTTClient_connectOptions *options,
                                   MQTTProperties *connectProperties, MQTTProperties *willProperties);

int MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions *options) {
    MQTTClients *m = handle;
    MQTTResponse response;

    response = MQTTClient_connectAll(handle, options, NULL, NULL);

    return response.reasonCode;
}

MQTTResponse MQTTClient_connectAll(MQTTClient handle, MQTTClient_connectOptions *options,
                                   MQTTProperties *connectProperties, MQTTProperties *willProperties) {
    MQTTClients *m = handle;
    MQTTResponse rc = MQTTResponse_initializer;
    Thread_lock_mutex(connect_mutex);
    Thread_lock_mutex(mqttclient_mutex);

    rc.reasonCode = SOCKET_ERROR;
    if (!library_initialized) {
        rc.reasonCode = MQTTCLIENT_FAILURE;
        goto exit;
    }

    if (options == NULL) {
        rc.reasonCode = MQTTCLIENT_NULL_PARAMETER;
        goto exit;
    }

    if (strncmp(options->struct_id, "MQTC", 4) != 0 || options->struct_version < 0 || options->struct_version > 8) {
        rc.reasonCode = MQTTCLIENT_BAD_STRUCTURE;
        goto exit;
    }

    if (options->will) /* check validity of will options structure */
    {
        if (strncmp(options->will->struct_id, "MQTW", 4) != 0 ||
            (options->will->struct_version != 0 && options->will->struct_version != 1)) {
            rc.reasonCode = MQTTCLIENT_BAD_STRUCTURE;
            goto exit;
        }
        if (options->will->qos < 0 || options->will->qos > 2) {
            rc.reasonCode = MQTTCLIENT_BAD_QOS;
            goto exit;
        }
        if (options->will->topicName == NULL) {
            rc.reasonCode = MQTTCLIENT_NULL_PARAMETER;
            goto exit;
        } else if (strlen(options->will->topicName) == 0) {
            rc.reasonCode = MQTTCLIENT_0_LEN_WILL_TOPIC;
            goto exit;
        }
    }

    if ((options->username && !UTF8_validateString(options->username)) ||
        (options->password && !UTF8_validateString(options->password))) {
        rc.reasonCode = MQTTCLIENT_BAD_UTF8_STRING;
        goto exit;
    }
    if (options->cleanstart != 0) {
        rc.reasonCode = MQTTCLIENT_BAD_MQTT_OPTION;
        goto exit;
    }

    if (options->struct_version < 2 || options->serverURIcount == 0) {
        if (!m) {
            rc.reasonCode = MQTTCLIENT_NULL_PARAMETER;
            goto exit;
        }
        rc = MQTTClient_connectURI(handle, options, m->serverURI, connectProperties, willProperties);
    } else {
        int i;

        for (i = 0; i < options->serverURIcount; ++i) {
            char *serverURI = options->serverURIs[i];

            if (strncmp(URI_TCP, serverURI, strlen(URI_TCP)) == 0)
                serverURI += strlen(URI_TCP);
            else if (strncmp(URI_WS, serverURI, strlen(URI_WS)) == 0) {
                serverURI += strlen(URI_WS);
                m->websocket = 1;
            }
            rc = MQTTClient_connectURI(handle, options, serverURI, connectProperties, willProperties);
            if (rc.reasonCode == MQTTREASONCODE_SUCCESS)
                break;
        }
    }
    if (rc.reasonCode == MQTTREASONCODE_SUCCESS) {
        if (rc.properties && MQTTProperties_hasProperty(rc.properties, MQTTPROPERTY_CODE_RECEIVE_MAXIMUM)) {
            int recv_max = MQTTProperties_getNumericValue(rc.properties, MQTTPROPERTY_CODE_RECEIVE_MAXIMUM);
            if (m->c->maxInflightMessages > recv_max)
                m->c->maxInflightMessages = recv_max;
        }
    }

    exit:
    if (m && m->c && m->c->will) {
        if (m->c->will->payload)
            free(m->c->will->payload);
        if (m->c->will->topic)
            free(m->c->will->topic);
        free(m->c->will);
        m->c->will = NULL;
    }
    Thread_unlock_mutex(mqttclient_mutex);
    Thread_unlock_mutex(connect_mutex);
    return rc;
}


/**
 * mqttclient_mutex must be locked when you call this function, if multi threaded
 */
static int MQTTClient_disconnect1(MQTTClient handle, int timeout, int call_connection_lost, int stop,
                                  enum MQTTReasonCodes reason, MQTTProperties *props) {
    MQTTClients *m = handle;
    START_TIME_TYPE start;
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
            if (MQTTTime_elapsed(start) >= (ELAPSED_TIME_TYPE) timeout)
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


int MQTTClient_disconnect(MQTTClient handle, int timeout) {
    int rc = 0;

    Thread_lock_mutex(mqttclient_mutex);
    rc = MQTTClient_disconnect1(handle, timeout, 0, 1, MQTTREASONCODE_SUCCESS, NULL);
    Thread_unlock_mutex(mqttclient_mutex);
    return rc;
}

MQTTResponse MQTTClient_subscribeMany5(MQTTClient handle, int count, char *const *topic,
                                       int *qos, MQTTSubscribe_options *opts, MQTTProperties *props) {
    MQTTClients *m = handle;
    List *topics = NULL;
    List *qoss = NULL;
    int i = 0;
    int rc = MQTTCLIENT_FAILURE;
    MQTTResponse resp = MQTTResponse_initializer;
    int msgid = 0;
    Thread_lock_mutex(subscribe_mutex);
    Thread_lock_mutex(mqttclient_mutex);

    resp.reasonCode = MQTTCLIENT_FAILURE;
    if (m == NULL || m->c == NULL) {
        rc = MQTTCLIENT_FAILURE;
        goto exit;
    }
    if (m->c->connected == 0) {
        rc = MQTTCLIENT_DISCONNECTED;
        goto exit;
    }
    for (i = 0; i < count; i++) {
        if (!UTF8_validateString(topic[i])) {
            rc = MQTTCLIENT_BAD_UTF8_STRING;
            goto exit;
        }

        if (qos[i] < 0 || qos[i] > 2) {
            rc = MQTTCLIENT_BAD_QOS;
            goto exit;
        }
    }
    if ((msgid = MQTTProtocol_assignMsgId(m->c)) == 0) {
        rc = MQTTCLIENT_MAX_MESSAGES_INFLIGHT;
        goto exit;
    }

    topics = ListInitialize();
    qoss = ListInitialize();
    for (i = 0; i < count; i++) {
        ListAppend(topics, topic[i], strlen(topic[i]));
        ListAppend(qoss, &qos[i], sizeof(int));
    }

    rc = MQTTProtocol_subscribe(m->c, topics, qoss, msgid, opts, props);
    ListFreeNoContent(topics);
    ListFreeNoContent(qoss);

    if (rc == TCPSOCKET_COMPLETE) {
        MQTTPacket *pack = NULL;

        Thread_unlock_mutex(mqttclient_mutex);
        pack = MQTTClient_waitfor(handle, SUBACK, &rc, m->commandTimeout);
        Thread_lock_mutex(mqttclient_mutex);
        if (pack != NULL) {
            Suback *sub = (Suback *) pack;
            {
                ListElement *current = NULL;
                i = 0;
                while (ListNextElement(sub->qoss, &current)) {
                    int *reqqos = (int *) (current->content);
                    qos[i++] = *reqqos;
                }
                resp.reasonCode = rc;
            }
            rc = MQTTProtocol_handleSubacks(pack, m->c->net.socket);
            m->pack = NULL;
        } else
            rc = SOCKET_ERROR;
    }

    if (rc == SOCKET_ERROR)
        MQTTClient_disconnect_internal(handle, 0);
    else if (rc == TCPSOCKET_COMPLETE)
        rc = MQTTCLIENT_SUCCESS;

    exit:
    if (rc < 0)
        resp.reasonCode = rc;
    Thread_unlock_mutex(mqttclient_mutex);
    Thread_unlock_mutex(subscribe_mutex);
    return resp;
}

MQTTResponse MQTTClient_subscribe5(MQTTClient handle, const char *topic, int qos,
                                   MQTTSubscribe_options *opts, MQTTProperties *props) {
    MQTTResponse rc;
    rc = MQTTClient_subscribeMany5(handle, 1, (char *const *) (&topic), &qos, opts, props);
    if (qos == MQTT_BAD_SUBSCRIBE) /* addition for MQTT 3.1.1 - error code from subscribe */
        rc.reasonCode = MQTT_BAD_SUBSCRIBE;
    return rc;
}


int MQTTClient_subscribe(MQTTClient handle, const char *topic, int qos) {
    MQTTClients *m = handle;
    MQTTResponse response = MQTTResponse_initializer;

    response = MQTTClient_subscribe5(handle, topic, qos, NULL, NULL);
    return response.reasonCode;
}


MQTTResponse MQTTClient_unsubscribeMany5(MQTTClient handle, int count, char *const *topic, MQTTProperties *props) {
    MQTTClients *m = handle;
    List *topics = NULL;
    int i = 0;
    int rc = SOCKET_ERROR;
    MQTTResponse resp = MQTTResponse_initializer;
    int msgid = 0;
    Thread_lock_mutex(unsubscribe_mutex);
    Thread_lock_mutex(mqttclient_mutex);

    resp.reasonCode = MQTTCLIENT_FAILURE;
    if (m == NULL || m->c == NULL) {
        rc = MQTTCLIENT_FAILURE;
        goto exit;
    }
    if (m->c->connected == 0) {
        rc = MQTTCLIENT_DISCONNECTED;
        goto exit;
    }
    for (i = 0; i < count; i++) {
        if (!UTF8_validateString(topic[i])) {
            rc = MQTTCLIENT_BAD_UTF8_STRING;
            goto exit;
        }
    }
    if ((msgid = MQTTProtocol_assignMsgId(m->c)) == 0) {
        rc = MQTTCLIENT_MAX_MESSAGES_INFLIGHT;
        goto exit;
    }

    topics = ListInitialize();
    for (i = 0; i < count; i++)
        ListAppend(topics, topic[i], strlen(topic[i]));
    rc = MQTTProtocol_unsubscribe(m->c, topics, msgid, props);
    ListFreeNoContent(topics);

    if (rc == TCPSOCKET_COMPLETE) {
        MQTTPacket *pack = NULL;

        Thread_unlock_mutex(mqttclient_mutex);
        pack = MQTTClient_waitfor(handle, UNSUBACK, &rc, m->commandTimeout);
        Thread_lock_mutex(mqttclient_mutex);
        if (pack != NULL) {
            Unsuback *unsub = (Unsuback *) pack;
            resp.reasonCode = rc;
            rc = MQTTProtocol_handleUnsubacks(pack, m->c->net.socket);
            m->pack = NULL;
        } else
            rc = SOCKET_ERROR;
    }

    if (rc == SOCKET_ERROR)
        MQTTClient_disconnect_internal(handle, 0);

    exit:
    if (rc < 0)
        resp.reasonCode = rc;
    Thread_unlock_mutex(mqttclient_mutex);
    Thread_unlock_mutex(unsubscribe_mutex);
    return resp;
}

MQTTResponse MQTTClient_unsubscribe5(MQTTClient handle, const char *topic, MQTTProperties *props) {
    MQTTResponse rc;

    rc = MQTTClient_unsubscribeMany5(handle, 1, (char *const *) (&topic), props);
    return rc;
}


int MQTTClient_unsubscribe(MQTTClient handle, const char *topic) {
    MQTTResponse response = MQTTClient_unsubscribe5(handle, topic, NULL);

    return response.reasonCode;
}


MQTTResponse MQTTClient_publish5(MQTTClient handle, const char *topicName, int payloadlen, const void *payload,
                                 int qos, int retained, MQTTProperties *properties,
                                 MQTTClient_deliveryToken *deliveryToken) {
    int rc = MQTTCLIENT_SUCCESS;
    MQTTClients *m = handle;
    Messages *msg = NULL;
    Publish *p = NULL;
    int blocked = 0;
    int msgid = 0;
    MQTTResponse resp = MQTTResponse_initializer;
    Thread_lock_mutex(mqttclient_mutex);

    if (m == NULL || m->c == NULL)
        rc = MQTTCLIENT_FAILURE;
    else if (m->c->connected == 0)
        rc = MQTTCLIENT_DISCONNECTED;
    else if (!UTF8_validateString(topicName))
        rc = MQTTCLIENT_BAD_UTF8_STRING;

    if (rc != MQTTCLIENT_SUCCESS)
        goto exit;

    /* If outbound queue is full, block until it is not */
    while (m->c->outboundMsgs->count >= m->c->maxInflightMessages ||
           Socket_noPendingWrites(m->c->net.socket) ==
           0) /* wait until the socket is free of large packets being written */
    {
        if (blocked == 0) {
            blocked = 1;
            Log(TRACE_MIN, -1, "Blocking publish on queue full for client %s", m->c->clientID);
        }
        Thread_unlock_mutex(mqttclient_mutex);
        MQTTClient_yield();
        Thread_lock_mutex(mqttclient_mutex);
        if (m->c->connected == 0) {
            rc = MQTTCLIENT_FAILURE;
            goto exit;
        }
    }
    if (blocked == 1)
        Log(TRACE_MIN, -1, "Resuming publish now queue not full for client %s", m->c->clientID);
    if (qos > 0 && (msgid = MQTTProtocol_assignMsgId(m->c)) ==
                   0) {    /* this should never happen as we've waited for spaces in the queue */
        rc = MQTTCLIENT_MAX_MESSAGES_INFLIGHT;
        goto exit;
    }

    if ((p = malloc(sizeof(Publish))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit_and_free;
    }
    memset(p->mask, '\0', sizeof(p->mask));
    p->payload = NULL;
    p->payloadlen = payloadlen;
    if (payloadlen > 0) {
        if ((p->payload = malloc(payloadlen)) == NULL) {
            rc = PAHO_MEMORY_ERROR;
            goto exit_and_free;
        }
        memcpy(p->payload, payload, payloadlen);
    }
    if ((p->topic = MQTTStrdup(topicName)) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit_and_free;
    }
    p->msgId = msgid;
    p->MQTTVersion = m->c->MQTTVersion;

    rc = MQTTProtocol_startPublish(m->c, p, qos, retained, &msg);

    /* If the packet was partially written to the socket, wait for it to complete.
     * However, if the client is disconnected during this time and qos is not 0, still return success, as
     * the packet has already been written to persistence and assigned a message id so will
     * be sent when the client next connects.
     */
    if (rc == TCPSOCKET_INTERRUPTED) {
        while (m->c->connected == 1) {
            pending_writes *writing = NULL;

            Thread_lock_mutex(socket_mutex);
            writing = SocketBuffer_getWrite(m->c->net.socket);
            Thread_unlock_mutex(socket_mutex);

            if (writing == NULL)
                break;

            Thread_unlock_mutex(mqttclient_mutex);
            MQTTClient_yield();
            Thread_lock_mutex(mqttclient_mutex);
        }
        rc = (qos > 0 || m->c->connected == 1) ? MQTTCLIENT_SUCCESS : MQTTCLIENT_FAILURE;
    }

    if (deliveryToken && qos > 0)
        *deliveryToken = msg->msgid;

    exit_and_free:
    if (p) {
        if (p->topic)
            free(p->topic);
        if (p->payload)
            free(p->payload);
        free(p);
    }

    if (rc == SOCKET_ERROR) {
        MQTTClient_disconnect_internal(handle, 0);
        /* Return success for qos > 0 as the send will be retried automatically */
        rc = (qos > 0) ? MQTTCLIENT_SUCCESS : MQTTCLIENT_FAILURE;
    }

    exit:
    Thread_unlock_mutex(mqttclient_mutex);
    resp.reasonCode = rc;
    return resp;
}

MQTTResponse MQTTClient_publishMessage5(MQTTClient handle, const char *topicName, MQTTClient_message *message,
                                        MQTTClient_deliveryToken *deliveryToken) {
    MQTTResponse rc = MQTTResponse_initializer;
    MQTTProperties *props = NULL;
    if (message == NULL) {
        rc.reasonCode = MQTTCLIENT_NULL_PARAMETER;
        goto exit;
    }

    if (strncmp(message->struct_id, "MQTM", 4) != 0 ||
        (message->struct_version != 0 && message->struct_version != 1)) {
        rc.reasonCode = MQTTCLIENT_BAD_STRUCTURE;
        goto exit;
    }

    if (message->struct_version >= 1)
        props = &message->properties;

    rc = MQTTClient_publish5(handle, topicName, message->payloadlen, message->payload,
                             message->qos, message->retained, props, deliveryToken);
    exit:
    return rc;
}


int MQTTClient_publishMessage(MQTTClient handle, const char *topicName, MQTTClient_message *message,
                              MQTTClient_deliveryToken *deliveryToken) {
    MQTTClients *m = handle;
    MQTTResponse rc = MQTTResponse_initializer;

    if (strncmp(message->struct_id, "MQTM", 4) != 0 ||
        (message->struct_version != 0 && message->struct_version != 1))
        rc.reasonCode = MQTTCLIENT_BAD_STRUCTURE;
    else
        rc = MQTTClient_publishMessage5(handle, topicName, message, deliveryToken);
    return rc.reasonCode;
}


static void MQTTClient_retry(void) {
    static START_TIME_TYPE last = START_TIME_ZERO;
    START_TIME_TYPE now;
    now = MQTTTime_now();
    if (MQTTTime_difftime(now, last) >= (DIFF_TIME_TYPE) (retryLoopIntervalms)) {
        last = MQTTTime_now();
        MQTTProtocol_keepalive(now);
        MQTTProtocol_retry(now, 1, 0);
    } else
        MQTTProtocol_retry(now, 0, 0);
}


static MQTTPacket *MQTTClient_cycle(SOCKET *sock, ELAPSED_TIME_TYPE timeout, int *rc) {
    static Ack ack;
    MQTTPacket *pack = NULL;
    int rc1 = 0;
    START_TIME_TYPE start;

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


static MQTTPacket *MQTTClient_waitfor(MQTTClient handle, int packet_type, int *rc, int64_t timeout) {
    MQTTPacket *pack = NULL;
    MQTTClients *m = handle;
    START_TIME_TYPE start = MQTTTime_start_clock();

    if (((MQTTClients *) handle) == NULL || timeout <= 0L) {
        *rc = MQTTCLIENT_FAILURE;
        goto exit;
    }

    if (running) {
        if (packet_type == CONNECT) {
            if ((*rc = Thread_wait_sem(m->connect_sem, (int) timeout)) == 0)
                *rc = m->rc;
        } else if (packet_type == CONNACK)
            *rc = Thread_wait_sem(m->connack_sem, (int) timeout);
        else if (packet_type == SUBACK)
            *rc = Thread_wait_sem(m->suback_sem, (int) timeout);
        else if (packet_type == UNSUBACK)
            *rc = Thread_wait_sem(m->unsuback_sem, (int) timeout);
        if (*rc == 0 && packet_type != CONNECT && m->pack == NULL)
            Log(LOG_ERROR, -1, "waitfor unexpectedly is NULL for client %s, packet_type %d, timeout %ld",
                m->c->clientID, packet_type, timeout);
        pack = m->pack;
    } else {
        *rc = TCPSOCKET_COMPLETE;
        while (1) {
            SOCKET sock = -1;
            pack = MQTTClient_cycle(&sock, 100L, rc);
            if (sock == m->c->net.socket) {
                if (*rc == SOCKET_ERROR)
                    break;
                if (pack && (pack->header.bits.type == packet_type))
                    break;
                if (m->c->connect_state == TCP_IN_PROGRESS) {
                    int error;
                    socklen_t len = sizeof(error);

                    if ((*rc = getsockopt(m->c->net.socket, SOL_SOCKET, SO_ERROR, (char *) &error, &len)) == 0)
                        *rc = error;
                    break;
                }
                else if (m->c->connect_state == WEBSOCKET_IN_PROGRESS && *rc != TCPSOCKET_INTERRUPTED) {
                    *rc = 1;
                    break;
                } else if (m->c->connect_state == PROXY_CONNECT_IN_PROGRESS) {
                    *rc = 1;
                    break;
                } else if (m->c->connect_state == WAIT_FOR_CONNACK) {
                    int error;
                    socklen_t len = sizeof(error);
                    if (getsockopt(m->c->net.socket, SOL_SOCKET, SO_ERROR, (char *) &error, &len) == 0) {
                        if (error) {
                            *rc = error;
                            break;
                        }
                    }
                }
            }
            if (MQTTTime_elapsed(start) > (uint64_t) timeout) {
                pack = NULL;
                break;
            }
        }
    }

    exit:
    return pack;
}

void MQTTClient_yield(void) {
    START_TIME_TYPE start = MQTTTime_start_clock();
    ELAPSED_TIME_TYPE elapsed = 0L;
    ELAPSED_TIME_TYPE timeout = 100L;
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


int MQTTClient_waitForCompletion(MQTTClient handle, MQTTClient_deliveryToken mdt, unsigned long timeout) {
    int rc = MQTTCLIENT_FAILURE;
    START_TIME_TYPE start = MQTTTime_start_clock();
    ELAPSED_TIME_TYPE elapsed = 0L;
    MQTTClients *m = handle;

    Thread_lock_mutex(mqttclient_mutex);

    if (m == NULL || m->c == NULL) {
        rc = MQTTCLIENT_FAILURE;
        goto exit;
    }

    elapsed = MQTTTime_elapsed(start);
    while (elapsed < timeout) {
        if (m->c->connected == 0) {
            rc = MQTTCLIENT_DISCONNECTED;
            goto exit;
        }
        if (ListFindItem(m->c->outboundMsgs, &mdt, messageIDCompare) == NULL) {
            rc = MQTTCLIENT_SUCCESS; /* well we couldn't find it */
            goto exit;
        }
        Thread_unlock_mutex(mqttclient_mutex);
        MQTTClient_yield();
        Thread_lock_mutex(mqttclient_mutex);
        elapsed = MQTTTime_elapsed(start);
    }

    exit:
    Thread_unlock_mutex(mqttclient_mutex);
    return rc;
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
