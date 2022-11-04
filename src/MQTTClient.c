//
// Created by Administrator on 2022/11/3.
//

#include <string.h>
#include <Log.h>
#include "Socket.h"
#include "WebSocket.h"
#include "MQTTTime.h"
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
static pthread_mutex_t *mqttclient_mutex = &mqttclient_mutex_store;

static pthread_mutex_t socket_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *socket_mutex = &socket_mutex_store;

static pthread_mutex_t subscribe_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *subscribe_mutex = &subscribe_mutex_store;

static pthread_mutex_t unsubscribe_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *unsubscribe_mutex = &unsubscribe_mutex_store;

static pthread_mutex_t connect_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *connect_mutex = &connect_mutex_store;


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
    sem_t *connect_sem;
    int rc; /* getsockopt return code in connect */
    sem_t *connack_sem;
    sem_t *suback_sem;
    sem_t *unsuback_sem;
    MQTTPacket *pack;

    unsigned long commandTimeout;
} MQTTClients;


static void MQTTClient_terminate(void);

static void MQTTClient_emptyMessageQueue(Clients *client);

static int clientSockCompare(void *a, void *b);

static void *MQTTClient_run(void *n);

static int MQTTClient_stop(void);


static int MQTTClient_cleanSession(Clients *client);

static MQTTResponse MQTTClient_connectURIVersion(
        MQTTClient handle, MQTTClient_connectOptions *options,
        const char *serverURI, int MQTTVersion,
        struct timeval start, uint64_t millisecsTimeout,
        MQTTProperties *connectProperties, MQTTProperties *willProperties);

static MQTTResponse MQTTClient_connectURI(MQTTClient handle, MQTTClient_connectOptions *options, const char *serverURI,
                                          MQTTProperties *connectProperties, MQTTProperties *willProperties);


static MQTTPacket *MQTTClient_cycle(SOCKET *sock, uint64_t timeout, int *rc);

static MQTTPacket *MQTTClient_waitfor(MQTTClient handle, int packet_type, int *rc, int64_t timeout);

int MQTTClient_createWithOptions(MQTTClient *handle, const char *serverURI, const char *clientId,
                                 MQTTClient_createOptions *options) {
    int rc = 0;
    MQTTClients *m = NULL;
    if (!library_initialized) {
        Log_initialize((Log_nameValue *) MQTTClient_getVersionInfo());
        bstate->clients = ListInitialize();
        Socket_outInitialize();
        handles = ListInitialize();
        library_initialized = 1;
    }

    if ((m = malloc(sizeof(MQTTClients))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
    }
    *handle = m;
    memset(m, '\0', sizeof(MQTTClients));
    m->commandTimeout = 10000L;
    if (strncmp(URI_TCP, serverURI, strlen(URI_TCP)) == 0)
        serverURI += strlen(URI_TCP);

    m->serverURI = MQTTStrdup(serverURI);
    ListAppend(handles, m, sizeof(MQTTClients));

    if ((m->c = malloc(sizeof(Clients))) == NULL) {
        ListRemove(handles, m);
        rc = PAHO_MEMORY_ERROR;
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

    return rc;
}


int MQTTClient_create(MQTTClient *handle, const char *serverURI, const char *clientId) {
    return MQTTClient_createWithOptions(handle, serverURI, clientId, NULL);
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

/* This is the thread function that handles the calling of callback functions if set */
static void *MQTTClient_run(void *n) {
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
        {
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
            } else if (m->c->connect_state == WEBSOCKET_IN_PROGRESS) {
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
                             struct timeval start, uint64_t millisecsTimeout,
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
        } else {
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
                    struct timeval zero = {0, 0};

                    while (ListNextElement(m->c->outboundMsgs, &outcurrent)) {
                        Messages *m = (Messages *) (outcurrent->content);
                        memset(&m->lastTouch, '\0', sizeof(m->lastTouch));
                    }
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
    }

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
    struct timeval start;
    uint64_t millisecsTimeout = 30000L;
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
    rc = MQTTClient_connectURI(handle, options, m->serverURI, connectProperties, willProperties);
    Thread_unlock_mutex(mqttclient_mutex);
    Thread_unlock_mutex(connect_mutex);
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

    topics = ListInitialize();
    qoss = ListInitialize();
    for (i = 0; i < count; i++) {
        ListAppend(topics, topic[i], strlen(topic[i]));
        ListAppend(qoss, &qos[i], sizeof(int));
    }

    rc = MQTTProtocol_subscribe(m->c, topics, qoss, msgid, opts, props);
    ListFreeNoContent(topics);
    ListFreeNoContent(qoss);

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

    topics = ListInitialize();
    for (i = 0; i < count; i++)
        ListAppend(topics, topic[i], strlen(topic[i]));
    rc = MQTTProtocol_unsubscribe(m->c, topics, msgid, props);
    ListFreeNoContent(topics);

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

    exit:
    Thread_unlock_mutex(mqttclient_mutex);
    resp.reasonCode = rc;
    return resp;
}

MQTTResponse MQTTClient_publishMessage5(MQTTClient handle, const char *topicName, MQTTClient_message *message,
                                        MQTTClient_deliveryToken *deliveryToken) {
    MQTTResponse rc = MQTTResponse_initializer;
    MQTTProperties *props = NULL;

    rc = MQTTClient_publish5(handle, topicName, message->payloadlen, message->payload,
                             message->qos, message->retained, props, deliveryToken);
    return rc;
}


int MQTTClient_publishMessage(MQTTClient handle, const char *topicName, MQTTClient_message *message,
                              MQTTClient_deliveryToken *deliveryToken) {
    MQTTClients *m = handle;
    MQTTResponse rc = MQTTResponse_initializer;

    rc = MQTTClient_publishMessage5(handle, topicName, message, deliveryToken);
    return rc.reasonCode;
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
            } else
                freed = 0;
            if (freed)
                pack = NULL;
        }
    }
    Thread_unlock_mutex(mqttclient_mutex);
    return pack;
}


static MQTTPacket *MQTTClient_waitfor(MQTTClient handle, int packet_type, int *rc, int64_t timeout) {
    MQTTPacket *pack = NULL;
    MQTTClients *m = handle;
    struct timeval start = MQTTTime_start_clock();

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
                } else if (m->c->connect_state == WEBSOCKET_IN_PROGRESS && *rc != TCPSOCKET_INTERRUPTED) {
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
