/*******************************************************************************
 * Copyright (c) 2009, 2022 IBM Corp., Ian Craggs and others
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    https://www.eclipse.org/legal/epl-2.0/
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial implementation and documentation
 *    Ian Craggs, Allan Stockdill-Mander - SSL support
 *    Ian Craggs - multiple server connection support
 *    Ian Craggs - fix for bug 413429 - connectionLost not called
 *    Ian Craggs - fix for bug 415042 - using already freed structure
 *    Ian Craggs - fix for bug 419233 - mutexes not reporting errors
 *    Ian Craggs - fix for bug 420851
 *    Ian Craggs - fix for bug 432903 - queue persistence
 *    Ian Craggs - MQTT 3.1.1 support
 *    Rong Xiang, Ian Craggs - C++ compatibility
 *    Ian Craggs - fix for bug 442400: reconnecting after network cable unplugged
 *    Ian Craggs - fix for bug 444934 - incorrect free in freeCommand1
 *    Ian Craggs - fix for bug 445891 - assigning msgid is not thread safe
 *    Ian Craggs - fix for bug 465369 - longer latency than expected
 *    Ian Craggs - fix for bug 444103 - success/failure callbacks not invoked
 *    Ian Craggs - fix for bug 484363 - segfault in getReadySocket
 *    Ian Craggs - automatic reconnect and offline buffering (send while disconnected)
 *    Ian Craggs - fix for bug 472250
 *    Ian Craggs - fix for bug 486548
 *    Ian Craggs - SNI support
 *    Ian Craggs - auto reconnect timing fix #218
 *    Ian Craggs - fix for issue #190
 *    Ian Craggs - check for NULL SSL options #334
 *    Ian Craggs - allocate username/password buffers #431
 *    Ian Craggs - MQTT 5.0 support
 *    Ian Craggs - refactor to reduce module size
 *******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "MQTTAsync.h"
#include "MQTTAsyncUtils.h"
#include "utf-8.h"
#include "MQTTProtocol.h"
#include "MQTTProtocolOut.h"
#include "Thread.h"
#include "SocketBuffer.h"
#include "WebSocket.h"

static void MQTTAsync_freeServerURIs(MQTTAsyncs *m);

#include "VersionInfo.h"

volatile int global_initialized = 0;
List *MQTTAsync_handles = NULL;
List *MQTTAsync_commands = NULL;
int MQTTAsync_tostop = 0;

static ClientStates ClientState =
        {
                CLIENT_VERSION, /* version */
                NULL /* client list */
        };

MQTTProtocol state;
ClientStates *bstate = &ClientState;

enum MQTTAsync_threadStates sendThread_state = STOPPED;
enum MQTTAsync_threadStates receiveThread_state = STOPPED;
thread_id_type sendThread_id = 0,
        receiveThread_id = 0;

#if !defined(min)
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif


void MQTTAsync_init_rand(void) {
    START_TIME_TYPE now = MQTTTime_start_clock();
    srand(now.tv_usec);
}

static pthread_mutex_t mqttasync_mutex_store = PTHREAD_MUTEX_INITIALIZER;
mutex_type mqttasync_mutex = &mqttasync_mutex_store;

static pthread_mutex_t socket_mutex_store = PTHREAD_MUTEX_INITIALIZER;
mutex_type socket_mutex = &socket_mutex_store;

static pthread_mutex_t mqttcommand_mutex_store = PTHREAD_MUTEX_INITIALIZER;
mutex_type mqttcommand_mutex = &mqttcommand_mutex_store;

static cond_type_struct send_cond_store = {PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
cond_type send_cond = &send_cond_store;

int MQTTAsync_createWithOptions(MQTTAsync *handle, const char *serverURI, const char *clientId,
                                int persistence_type, void *persistence_context, MQTTAsync_createOptions *options) {
    int rc = 0;
    MQTTAsyncs *m = NULL;

    MQTTAsync_lock_mutex(mqttasync_mutex);

    if (serverURI == NULL || clientId == NULL) {
        rc = MQTTASYNC_NULL_PARAMETER;
        goto exit;
    }

    if (!UTF8_validateString(clientId)) {
        rc = MQTTASYNC_BAD_UTF8_STRING;
        goto exit;
    }

    if (strlen(clientId) == 0 && persistence_type == MQTTCLIENT_PERSISTENCE_DEFAULT) {
        rc = MQTTASYNC_PERSISTENCE_ERROR;
        goto exit;
    }

    if (strstr(serverURI, "://") != NULL) {
        if (strncmp(URI_TCP, serverURI, strlen(URI_TCP)) != 0
            && strncmp(URI_WS, serverURI, strlen(URI_WS)) != 0
                ) {
            rc = MQTTASYNC_BAD_PROTOCOL;
            goto exit;
        }
    }

    if (options && options->maxBufferedMessages <= 0) {
        rc = MQTTASYNC_MAX_BUFFERED;
        goto exit;
    }

    if (options && (strncmp(options->struct_id, "MQCO", 4) != 0 ||
                    options->struct_version < 0 || options->struct_version > 2)) {
        rc = MQTTASYNC_BAD_STRUCTURE;
        goto exit;
    }

    if (!global_initialized) {
        Log_initialize((Log_nameValue *) MQTTAsync_getVersionInfo());
        bstate->clients = ListInitialize();
        Socket_outInitialize();
        Socket_setWriteCompleteCallback(MQTTAsync_writeComplete);
        Socket_setWriteAvailableCallback(MQTTProtocol_writeAvailable);
        MQTTAsync_handles = ListInitialize();
        MQTTAsync_commands = ListInitialize();
        global_initialized = 1;
    }
    if ((m = malloc(sizeof(MQTTAsyncs))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    *handle = m;
    memset(m, '\0', sizeof(MQTTAsyncs));
    if (strncmp(URI_TCP, serverURI, strlen(URI_TCP)) == 0)
        serverURI += strlen(URI_TCP);
    else if (strncmp(URI_WS, serverURI, strlen(URI_WS)) == 0) {
        serverURI += strlen(URI_WS);
        m->websocket = 1;
    }
    if ((m->serverURI = MQTTStrdup(serverURI)) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    m->responses = ListInitialize();
    ListAppend(MQTTAsync_handles, m, sizeof(MQTTAsyncs));

    if ((m->c = malloc(sizeof(Clients))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    memset(m->c, '\0', sizeof(Clients));
    m->c->context = m;
    m->c->outboundMsgs = ListInitialize();
    m->c->inboundMsgs = ListInitialize();
    m->c->messageQueue = ListInitialize();
    m->c->outboundQueue = ListInitialize();
    m->c->clientID = MQTTStrdup(clientId);
    if (m->c->context == NULL || m->c->outboundMsgs == NULL || m->c->inboundMsgs == NULL ||
        m->c->messageQueue == NULL || m->c->outboundQueue == NULL || m->c->clientID == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    m->c->MQTTVersion = MQTTVERSION_DEFAULT;

    m->shouldBeConnected = 0;
    if (options) {
        if ((m->createOptions = malloc(sizeof(MQTTAsync_createOptions))) == NULL) {
            rc = PAHO_MEMORY_ERROR;
            goto exit;
        }
        memcpy(m->createOptions, options, sizeof(MQTTAsync_createOptions));
        if (options->struct_version > 0)
            m->c->MQTTVersion = options->MQTTVersion;
    }

    ListAppend(bstate->clients, m->c, sizeof(Clients) + 3 * sizeof(List));

    exit:
    MQTTAsync_unlock_mutex(mqttasync_mutex);
    return rc;
}


int MQTTAsync_create(MQTTAsync *handle, const char *serverURI, const char *clientId,
                     int persistence_type, void *persistence_context) {
    MQTTAsync_init_rand();

    return MQTTAsync_createWithOptions(handle, serverURI, clientId, persistence_type,
                                       persistence_context, NULL);
}


void MQTTAsync_destroy(MQTTAsync *handle) {
    MQTTAsyncs *m = *handle;
    MQTTAsync_lock_mutex(mqttasync_mutex);

    if (m == NULL)
        goto exit;

    MQTTAsync_closeSession(m->c, MQTTREASONCODE_SUCCESS, NULL);

    MQTTAsync_freeResponses(m);
    MQTTAsync_freeCommands(m);
    ListFree(m->responses);

    if (m->c) {
        SOCKET saved_socket = m->c->net.socket;
        char *saved_clientid = MQTTStrdup(m->c->clientID);
        MQTTAsync_emptyMessageQueue(m->c);
        MQTTProtocol_freeClient(m->c);
        if (!ListRemove(bstate->clients, m->c))
            Log(LOG_ERROR, 0, NULL);
        else
            Log(TRACE_MIN, 1, NULL, saved_clientid, saved_socket);
        free(saved_clientid);
    }

    if (m->serverURI)
        free(m->serverURI);
    if (m->createOptions)
        free(m->createOptions);
    MQTTAsync_freeServerURIs(m);
    if (m->connectProps) {
        MQTTProperties_free(m->connectProps);
        free(m->connectProps);
        m->connectProps = NULL;
    }
    if (m->willProps) {
        MQTTProperties_free(m->willProps);
        free(m->willProps);
        m->willProps = NULL;
    }
    if (!ListRemove(MQTTAsync_handles, m))
        Log(LOG_ERROR, -1, "free error");
    *handle = NULL;
    if (bstate->clients->count == 0)
        MQTTAsync_terminate();

    exit:
    MQTTAsync_unlock_mutex(mqttasync_mutex);
}


int MQTTAsync_connect(MQTTAsync handle, const MQTTAsync_connectOptions *options) {
    MQTTAsyncs *m = handle;
    int rc = MQTTASYNC_SUCCESS;
    MQTTAsync_queuedCommand *conn;
    thread_id_type thread_id = 0;
    int locked = 0;
    if (options == NULL) {
        rc = MQTTASYNC_NULL_PARAMETER;
        goto exit;
    }

    if (strncmp(options->struct_id, "MQTC", 4) != 0 || options->struct_version < 0 || options->struct_version > 8) {
        rc = MQTTASYNC_BAD_STRUCTURE;
        goto exit;
    }
    if (options->will) /* check validity of will options structure */
    {
        if (strncmp(options->will->struct_id, "MQTW", 4) != 0 ||
            (options->will->struct_version != 0 && options->will->struct_version != 1)) {
            rc = MQTTASYNC_BAD_STRUCTURE;
            goto exit;
        }
        if (options->will->qos < 0 || options->will->qos > 2) {
            rc = MQTTASYNC_BAD_QOS;
            goto exit;
        }
        if (options->will->topicName == NULL) {
            rc = MQTTASYNC_NULL_PARAMETER;
            goto exit;
        } else if (strlen(options->will->topicName) == 0) {
            rc = MQTTASYNC_0_LEN_WILL_TOPIC;
            goto exit;
        }
    }
    if (options->struct_version != 0 && options->ssl) /* check validity of SSL options structure */
    {
        if (strncmp(options->ssl->struct_id, "MQTS", 4) != 0 || options->ssl->struct_version < 0 ||
            options->ssl->struct_version > 5) {
            rc = MQTTASYNC_BAD_STRUCTURE;
            goto exit;
        }
    }
    if (options->MQTTVersion >= MQTTVERSION_5 && m->c->MQTTVersion < MQTTVERSION_5) {
        rc = MQTTASYNC_WRONG_MQTT_VERSION;
        goto exit;
    }
    if ((options->username && !UTF8_validateString(options->username)) ||
        (options->password && !UTF8_validateString(options->password))) {
        rc = MQTTASYNC_BAD_UTF8_STRING;
        goto exit;
    }
    if (options->MQTTVersion >= MQTTVERSION_5 && options->struct_version < 6) {
        rc = MQTTASYNC_BAD_STRUCTURE;
        goto exit;
    }
    if (options->MQTTVersion >= MQTTVERSION_5 && options->cleansession != 0) {
        rc = MQTTASYNC_BAD_MQTT_OPTION;
        goto exit;
    }
    if (options->MQTTVersion < MQTTVERSION_5 && options->struct_version >= 6) {
        if (options->cleanstart != 0 || options->onFailure5 || options->onSuccess5 ||
            options->connectProperties || options->willProperties) {
            rc = MQTTASYNC_BAD_MQTT_OPTION;
            goto exit;
        }
    }

    m->connect.onSuccess = options->onSuccess;
    m->connect.onFailure = options->onFailure;
    if (options->struct_version >= 6) {
        m->connect.onSuccess5 = options->onSuccess5;
        m->connect.onFailure5 = options->onFailure5;
    }
    m->connect.context = options->context;
    m->connectTimeout = options->connectTimeout;

    /* don't lock async mutex if we are being called from a callback */
    thread_id = Thread_getid();
    if (thread_id != sendThread_id && thread_id != receiveThread_id) {
        MQTTAsync_lock_mutex(mqttasync_mutex);
        locked = 1;
    }
    MQTTAsync_tostop = 0;
    if (sendThread_state != STARTING && sendThread_state != RUNNING) {
        sendThread_state = STARTING;
        Thread_start(MQTTAsync_sendThread, NULL);
    }
    if (receiveThread_state != STARTING && receiveThread_state != RUNNING) {
        receiveThread_state = STARTING;
        Thread_start(MQTTAsync_receiveThread, handle);
    }
    if (locked)
        MQTTAsync_unlock_mutex(mqttasync_mutex);

    m->c->keepAliveInterval = options->keepAliveInterval;
    setRetryLoopInterval(options->keepAliveInterval);
    m->c->cleansession = options->cleansession;
    m->c->maxInflightMessages = options->maxInflight;
    if (options->struct_version >= 3)
        m->c->MQTTVersion = options->MQTTVersion;
    else
        m->c->MQTTVersion = MQTTVERSION_DEFAULT;
    if (options->struct_version >= 4) {
        m->automaticReconnect = options->automaticReconnect;
        m->minRetryInterval = options->minRetryInterval;
        m->maxRetryInterval = options->maxRetryInterval;
    }
    if (options->struct_version >= 7) {
        m->c->net.httpHeaders = (const MQTTClient_nameValue *) options->httpHeaders;
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
            rc = PAHO_MEMORY_ERROR;
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
                rc = PAHO_MEMORY_ERROR;
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

    if (options->struct_version != 0 && options->ssl) {
        rc = MQTTASYNC_SSL_NOT_SUPPORTED;
        goto exit;
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
            rc = PAHO_MEMORY_ERROR;
            goto exit;
        }
        memcpy((void *) m->c->password, options->binarypwd.data, m->c->passwordlen);
    }

    m->c->retryInterval = options->retryInterval;
    m->shouldBeConnected = 1;

    m->connectTimeout = options->connectTimeout;

    MQTTAsync_freeServerURIs(m);
    if (options->struct_version >= 2 && options->serverURIcount > 0) {
        int i;

        m->serverURIcount = options->serverURIcount;
        if ((m->serverURIs = malloc(options->serverURIcount * sizeof(char *))) == NULL) {
            rc = PAHO_MEMORY_ERROR;
            goto exit;
        }
        for (i = 0; i < options->serverURIcount; ++i)
            m->serverURIs[i] = MQTTStrdup(options->serverURIs[i]);
    }

    if (m->connectProps) {
        MQTTProperties_free(m->connectProps);
        free(m->connectProps);
        m->connectProps = NULL;
    }
    if (m->willProps) {
        MQTTProperties_free(m->willProps);
        free(m->willProps);
        m->willProps = NULL;
    }
    if (options->struct_version >= 6) {
        if (options->connectProperties) {
            MQTTProperties initialized = MQTTProperties_initializer;

            if ((m->connectProps = malloc(sizeof(MQTTProperties))) == NULL) {
                rc = PAHO_MEMORY_ERROR;
                goto exit;
            }
            *m->connectProps = initialized;
            *m->connectProps = MQTTProperties_copy(options->connectProperties);

            if (MQTTProperties_hasProperty(options->connectProperties, MQTTPROPERTY_CODE_SESSION_EXPIRY_INTERVAL))
                m->c->sessionExpiry = MQTTProperties_getNumericValue(options->connectProperties,
                                                                     MQTTPROPERTY_CODE_SESSION_EXPIRY_INTERVAL);

        }
        if (options->willProperties) {
            MQTTProperties initialized = MQTTProperties_initializer;

            if ((m->willProps = malloc(sizeof(MQTTProperties))) == NULL) {
                rc = PAHO_MEMORY_ERROR;
                goto exit;
            }
            *m->willProps = initialized;
            *m->willProps = MQTTProperties_copy(options->willProperties);
        }
        m->c->cleanstart = options->cleanstart;
    }

    /* Add connect request to operation queue */
    if ((conn = malloc(sizeof(MQTTAsync_queuedCommand))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    memset(conn, '\0', sizeof(MQTTAsync_queuedCommand));
    conn->client = m;
    if (options) {
        conn->command.onSuccess = options->onSuccess;
        conn->command.onFailure = options->onFailure;
        conn->command.onSuccess5 = options->onSuccess5;
        conn->command.onFailure5 = options->onFailure5;
        conn->command.context = options->context;
    }
    conn->command.type = CONNECT;
    conn->command.details.conn.currentURI = 0;
    rc = MQTTAsync_addCommand(conn, sizeof(conn));

    exit:
    return rc;
}

int MQTTAsync_subscribeMany(MQTTAsync handle, int count, char *const *topic, const int *qos,
                            MQTTAsync_responseOptions *response) {
    MQTTAsyncs *m = handle;
    int i = 0;
    int rc = MQTTASYNC_SUCCESS;
    MQTTAsync_queuedCommand *sub;
    int msgid = 0;
    if (m == NULL || m->c == NULL)
        rc = MQTTASYNC_FAILURE;
    else if (m->c->connected == 0)
        rc = MQTTASYNC_DISCONNECTED;
    else
        for (i = 0; i < count; i++) {
            if (!UTF8_validateString(topic[i])) {
                rc = MQTTASYNC_BAD_UTF8_STRING;
                break;
            }
            if (qos[i] < 0 || qos[i] > 2) {
                rc = MQTTASYNC_BAD_QOS;
                break;
            }
        }
    if (rc != MQTTASYNC_SUCCESS); /* don't overwrite a previous error code */
    else if ((msgid = MQTTAsync_assignMsgId(m)) == 0)
        rc = MQTTASYNC_NO_MORE_MSGIDS;
    else if (m->c->MQTTVersion >= MQTTVERSION_5 && count > 1 && (count != response->subscribeOptionsCount
                                                                 && response->subscribeOptionsCount != 0))
        rc = MQTTASYNC_BAD_MQTT_OPTION;
    else if (response) {
        if (m->c->MQTTVersion >= MQTTVERSION_5) {
            if (response->struct_version == 0 || response->onFailure || response->onSuccess)
                rc = MQTTASYNC_BAD_MQTT_OPTION;
        } else if (m->c->MQTTVersion < MQTTVERSION_5) {
            if (response->struct_version >= 1 && (response->onFailure5 || response->onSuccess5))
                rc = MQTTASYNC_BAD_MQTT_OPTION;
        }
    }
    if (rc != MQTTASYNC_SUCCESS)
        goto exit;

    /* Add subscribe request to operation queue */
    if ((sub = malloc(sizeof(MQTTAsync_queuedCommand))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    memset(sub, '\0', sizeof(MQTTAsync_queuedCommand));
    sub->client = m;
    sub->command.token = msgid;
    if (response) {
        sub->command.onSuccess = response->onSuccess;
        sub->command.onFailure = response->onFailure;
        sub->command.onSuccess5 = response->onSuccess5;
        sub->command.onFailure5 = response->onFailure5;
        sub->command.context = response->context;
        response->token = sub->command.token;
        if (m->c->MQTTVersion >= MQTTVERSION_5) {
            sub->command.properties = MQTTProperties_copy(&response->properties);
            sub->command.details.sub.opts = response->subscribeOptions;
            if (count > 1) {
                if ((sub->command.details.sub.optlist = malloc(sizeof(MQTTSubscribe_options) * count)) == NULL) {
                    rc = PAHO_MEMORY_ERROR;
                    goto exit;
                }
                if (response->subscribeOptionsCount == 0) {
                    MQTTSubscribe_options initialized = MQTTSubscribe_options_initializer;
                    for (i = 0; i < count; ++i)
                        sub->command.details.sub.optlist[i] = initialized;
                } else {
                    for (i = 0; i < count; ++i)
                        sub->command.details.sub.optlist[i] = response->subscribeOptionsList[i];
                }
            }
        }
    }
    sub->command.type = SUBSCRIBE;
    sub->command.details.sub.count = count;
    sub->command.details.sub.topics = malloc(sizeof(char *) * count);
    sub->command.details.sub.qoss = malloc(sizeof(int) * count);
    if (sub->command.details.sub.topics && sub->command.details.sub.qoss) {
        for (i = 0; i < count; ++i) {
            if ((sub->command.details.sub.topics[i] = MQTTStrdup(topic[i])) == NULL) {
                rc = PAHO_MEMORY_ERROR;
                goto exit;
            }
            sub->command.details.sub.qoss[i] = qos[i];
        }
        rc = MQTTAsync_addCommand(sub, sizeof(sub));
    } else
        rc = PAHO_MEMORY_ERROR;

    exit:
    return rc;
}


int MQTTAsync_subscribe(MQTTAsync handle, const char *topic, int qos, MQTTAsync_responseOptions *response) {
    int rc = 0;
    rc = MQTTAsync_subscribeMany(handle, 1, (char *const *) (&topic), &qos, response);
    return rc;
}


int MQTTAsync_unsubscribeMany(MQTTAsync handle, int count, char *const *topic, MQTTAsync_responseOptions *response) {
    MQTTAsyncs *m = handle;
    int i = 0;
    int rc = MQTTASYNC_SUCCESS;
    MQTTAsync_queuedCommand *unsub;
    int msgid = 0;

    if (m == NULL || m->c == NULL)
        rc = MQTTASYNC_FAILURE;
    else if (m->c->connected == 0)
        rc = MQTTASYNC_DISCONNECTED;
    else
        for (i = 0; i < count; i++) {
            if (!UTF8_validateString(topic[i])) {
                rc = MQTTASYNC_BAD_UTF8_STRING;
                break;
            }
        }
    if (rc != MQTTASYNC_SUCCESS); /* don't overwrite a previous error code */
    else if ((msgid = MQTTAsync_assignMsgId(m)) == 0)
        rc = MQTTASYNC_NO_MORE_MSGIDS;
    else if (response) {
        if (m->c->MQTTVersion >= MQTTVERSION_5) {
            if (response->struct_version == 0 || response->onFailure || response->onSuccess)
                rc = MQTTASYNC_BAD_MQTT_OPTION;
        } else if (m->c->MQTTVersion < MQTTVERSION_5) {
            if (response->struct_version >= 1 && (response->onFailure5 || response->onSuccess5))
                rc = MQTTASYNC_BAD_MQTT_OPTION;
        }
    }
    if (rc != MQTTASYNC_SUCCESS)
        goto exit;

    /* Add unsubscribe request to operation queue */
    if ((unsub = malloc(sizeof(MQTTAsync_queuedCommand))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    memset(unsub, '\0', sizeof(MQTTAsync_queuedCommand));
    unsub->client = m;
    unsub->command.type = UNSUBSCRIBE;
    unsub->command.token = msgid;
    if (response) {
        unsub->command.onSuccess = response->onSuccess;
        unsub->command.onFailure = response->onFailure;
        unsub->command.onSuccess5 = response->onSuccess5;
        unsub->command.onFailure5 = response->onFailure5;
        unsub->command.context = response->context;
        response->token = unsub->command.token;
        if (m->c->MQTTVersion >= MQTTVERSION_5)
            unsub->command.properties = MQTTProperties_copy(&response->properties);
    }
    unsub->command.details.unsub.count = count;
    if ((unsub->command.details.unsub.topics = malloc(sizeof(char *) * count)) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    for (i = 0; i < count; ++i)
        unsub->command.details.unsub.topics[i] = MQTTStrdup(topic[i]);
    rc = MQTTAsync_addCommand(unsub, sizeof(unsub));

    exit:
    return rc;
}


int MQTTAsync_unsubscribe(MQTTAsync handle, const char *topic, MQTTAsync_responseOptions *response) {
    int rc = 0;
    rc = MQTTAsync_unsubscribeMany(handle, 1, (char *const *) (&topic), response);
    return rc;
}


int MQTTAsync_send(MQTTAsync handle, const char *destinationName, int payloadlen, const void *payload,
                   int qos, int retained, MQTTAsync_responseOptions *response) {
    int rc = MQTTASYNC_SUCCESS;
    MQTTAsyncs *m = handle;
    MQTTAsync_queuedCommand *pub;
    int msgid = 0;
    if (m == NULL || m->c == NULL)
        rc = MQTTASYNC_FAILURE;
    else if (m->c->connected == 0) {
        if (m->createOptions == NULL)
            rc = MQTTASYNC_DISCONNECTED;
        else if (m->createOptions->sendWhileDisconnected == 0)
            rc = MQTTASYNC_DISCONNECTED;
        else if (m->shouldBeConnected == 0 &&
                 (m->createOptions->struct_version < 2 || m->createOptions->allowDisconnectedSendAtAnyTime == 0))
            rc = MQTTASYNC_DISCONNECTED;
    }

    if (rc != MQTTASYNC_SUCCESS)
        goto exit;

    if (!UTF8_validateString(destinationName))
        rc = MQTTASYNC_BAD_UTF8_STRING;
    else if (qos < 0 || qos > 2)
        rc = MQTTASYNC_BAD_QOS;
    else if (qos > 0 && (msgid = MQTTAsync_assignMsgId(m)) == 0)
        rc = MQTTASYNC_NO_MORE_MSGIDS;
    else if (m->createOptions &&
             (m->createOptions->struct_version < 2 || m->createOptions->deleteOldestMessages == 0) &&
             (MQTTAsync_getNoBufferedMessages(m) >= m->createOptions->maxBufferedMessages))
        rc = MQTTASYNC_MAX_BUFFERED_MESSAGES;
    else if (response) {
        if (m->c->MQTTVersion >= MQTTVERSION_5) {
            if (response->struct_version == 0 || response->onFailure || response->onSuccess)
                rc = MQTTASYNC_BAD_MQTT_OPTION;
        } else if (m->c->MQTTVersion < MQTTVERSION_5) {
            if (response->struct_version >= 1 && (response->onFailure5 || response->onSuccess5))
                rc = MQTTASYNC_BAD_MQTT_OPTION;
        }
    }

    if (rc != MQTTASYNC_SUCCESS)
        goto exit;

    /* Add publish request to operation queue */
    if ((pub = malloc(sizeof(MQTTAsync_queuedCommand))) == NULL) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    memset(pub, '\0', sizeof(MQTTAsync_queuedCommand));
    pub->client = m;
    pub->command.type = PUBLISH;
    pub->command.token = msgid;
    if (response) {
        pub->command.onSuccess = response->onSuccess;
        pub->command.onFailure = response->onFailure;
        pub->command.onSuccess5 = response->onSuccess5;
        pub->command.onFailure5 = response->onFailure5;
        pub->command.context = response->context;
        response->token = pub->command.token;
        if (m->c->MQTTVersion >= MQTTVERSION_5)
            pub->command.properties = MQTTProperties_copy(&response->properties);
    }
    if ((pub->command.details.pub.destinationName = MQTTStrdup(destinationName)) == NULL) {
        free(pub);
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    pub->command.details.pub.payloadlen = payloadlen;
    if ((pub->command.details.pub.payload = malloc(payloadlen)) == NULL) {
        free(pub->command.details.pub.destinationName);
        free(pub);
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }
    memcpy(pub->command.details.pub.payload, payload, payloadlen);
    pub->command.details.pub.qos = qos;
    pub->command.details.pub.retained = retained;
    rc = MQTTAsync_addCommand(pub, sizeof(pub));

    exit:
    return rc;
}


int MQTTAsync_sendMessage(MQTTAsync handle, const char *destinationName, const MQTTAsync_message *message,
                          MQTTAsync_responseOptions *response) {
    int rc = MQTTASYNC_SUCCESS;
    MQTTAsyncs *m = handle;
    if (message == NULL) {
        rc = MQTTASYNC_NULL_PARAMETER;
        goto exit;
    }
    if (strncmp(message->struct_id, "MQTM", 4) != 0 ||
        (message->struct_version != 0 && message->struct_version != 1)) {
        rc = MQTTASYNC_BAD_STRUCTURE;
        goto exit;
    }

    if (m->c->MQTTVersion >= MQTTVERSION_5 && response)
        response->properties = message->properties;

    rc = MQTTAsync_send(handle, destinationName, message->payloadlen, message->payload,
                        message->qos, message->retained, response);
    exit:
    FUNC_EXIT_RC(rc);
    return rc;
}


int MQTTAsync_disconnect(MQTTAsync handle, const MQTTAsync_disconnectOptions *options) {
    if (options != NULL &&
        (strncmp(options->struct_id, "MQTD", 4) != 0 || options->struct_version < 0 || options->struct_version > 1))
        return MQTTASYNC_BAD_STRUCTURE;
    else
        return MQTTAsync_disconnect1(handle, options, 0);
}

int MQTTAsync_isComplete(MQTTAsync handle, MQTTAsync_token dt) {
    int rc = MQTTASYNC_SUCCESS;
    MQTTAsyncs *m = handle;
    ListElement *current = NULL;

    MQTTAsync_lock_mutex(mqttasync_mutex);

    if (m == NULL) {
        rc = MQTTASYNC_FAILURE;
        goto exit;
    }

    /* First check unprocessed commands */
    current = NULL;
    while (ListNextElement(MQTTAsync_commands, &current)) {
        MQTTAsync_queuedCommand *cmd = (MQTTAsync_queuedCommand *) (current->content);

        if (cmd->client == m && cmd->command.token == dt)
            goto exit;
    }

    /* Now check the inflight messages */
    if (m->c && m->c->outboundMsgs->count > 0) {
        current = NULL;
        while (ListNextElement(m->c->outboundMsgs, &current)) {
            Messages *m = (Messages *) (current->content);
            if (m->msgid == dt)
                goto exit;
        }
    }
    rc = MQTTASYNC_TRUE; /* Can't find it, so it must be complete */

    exit:
    MQTTAsync_unlock_mutex(mqttasync_mutex);
    return rc;
}


int MQTTAsync_waitForCompletion(MQTTAsync handle, MQTTAsync_token dt, unsigned long timeout) {
    int rc = MQTTASYNC_FAILURE;
    START_TIME_TYPE start = MQTTTime_start_clock();
    ELAPSED_TIME_TYPE elapsed = 0L;
    MQTTAsyncs *m = handle;
    MQTTAsync_lock_mutex(mqttasync_mutex);

    if (m == NULL || m->c == NULL) {
        MQTTAsync_unlock_mutex(mqttasync_mutex);
        rc = MQTTASYNC_FAILURE;
        goto exit;
    }
    if (m->c->connected == 0) {
        MQTTAsync_unlock_mutex(mqttasync_mutex);
        rc = MQTTASYNC_DISCONNECTED;
        goto exit;
    }
    MQTTAsync_unlock_mutex(mqttasync_mutex);

    if (MQTTAsync_isComplete(handle, dt) == 1) {
        rc = MQTTASYNC_SUCCESS; /* well we couldn't find it */
        goto exit;
    }

    elapsed = MQTTTime_elapsed(start);
    while (elapsed < timeout && rc == MQTTASYNC_FAILURE) {
        MQTTTime_sleep(100);
        if (MQTTAsync_isComplete(handle, dt) == 1)
            rc = MQTTASYNC_SUCCESS; /* well we couldn't find it */
        MQTTAsync_lock_mutex(mqttasync_mutex);
        if (m->c->connected == 0)
            rc = MQTTASYNC_DISCONNECTED;
        MQTTAsync_unlock_mutex(mqttasync_mutex);
        elapsed = MQTTTime_elapsed(start);
    }
    exit:
    return rc;
}


int MQTTAsync_getPendingTokens(MQTTAsync handle, MQTTAsync_token **tokens) {
    int rc = MQTTASYNC_SUCCESS;
    MQTTAsyncs *m = handle;
    ListElement *current = NULL;
    int count = 0;
    MQTTAsync_lock_mutex(mqttasync_mutex);
    MQTTAsync_lock_mutex(mqttcommand_mutex);
    *tokens = NULL;

    if (m == NULL) {
        rc = MQTTASYNC_FAILURE;
        goto exit;
    }

    /* calculate the number of pending tokens - commands plus inflight */
    while (ListNextElement(MQTTAsync_commands, &current)) {
        MQTTAsync_queuedCommand *cmd = (MQTTAsync_queuedCommand *) (current->content);

        if (cmd->client == m && cmd->command.type == PUBLISH)
            count++;
    }
    if (m->c)
        count += m->c->outboundMsgs->count;
    if (count == 0)
        goto exit; /* no tokens to return */
    *tokens = malloc(sizeof(MQTTAsync_token) * (count + 1));  /* add space for sentinel at end of list */
    if (!*tokens) {
        rc = PAHO_MEMORY_ERROR;
        goto exit;
    }

    /* First add the unprocessed commands to the pending tokens */
    current = NULL;
    count = 0;
    while (ListNextElement(MQTTAsync_commands, &current)) {
        MQTTAsync_queuedCommand *cmd = (MQTTAsync_queuedCommand *) (current->content);

        if (cmd->client == m && cmd->command.type == PUBLISH)
            (*tokens)[count++] = cmd->command.token;
    }

    /* Now add the inflight messages */
    if (m->c && m->c->outboundMsgs->count > 0) {
        current = NULL;
        while (ListNextElement(m->c->outboundMsgs, &current)) {
            Messages *m = (Messages *) (current->content);
            (*tokens)[count++] = m->msgid;
        }
    }
    (*tokens)[count] = -1; /* indicate end of list */

    exit:
    MQTTAsync_unlock_mutex(mqttcommand_mutex);
    MQTTAsync_unlock_mutex(mqttasync_mutex);
    return rc;
}


int MQTTAsync_setCallbacks(MQTTAsync handle, void *context,
                           MQTTAsync_connectionLost *cl,
                           MQTTAsync_messageArrived *ma,
                           MQTTAsync_deliveryComplete *dc) {
    int rc = MQTTASYNC_SUCCESS;
    MQTTAsyncs *m = handle;
    MQTTAsync_lock_mutex(mqttasync_mutex);

    if (m == NULL || ma == NULL || m->c == NULL || m->c->connect_state != NOT_IN_PROGRESS)
        rc = MQTTASYNC_FAILURE;
    else {
        m->clContext = m->maContext = m->dcContext = context;
        m->cl = cl;
        m->ma = ma;
        m->dc = dc;
    }

    MQTTAsync_unlock_mutex(mqttasync_mutex);
    return rc;
}

void MQTTAsync_setTraceLevel(enum MQTTASYNC_TRACE_LEVELS level) {
    Log_setTraceLevel((enum LOG_LEVELS) level);
}


void MQTTAsync_setTraceCallback(MQTTAsync_traceCallback *callback) {
    Log_setTraceCallback((Log_traceCallback *) callback);
}


MQTTAsync_nameValue *MQTTAsync_getVersionInfo(void) {
#define MAX_INFO_STRINGS 8
    static MQTTAsync_nameValue libinfo[MAX_INFO_STRINGS + 1];
    int i = 0;

    libinfo[i].name = "Product name";
    libinfo[i++].value = "Eclipse Paho Asynchronous MQTT C Client Library";

    libinfo[i].name = "Version";
    libinfo[i++].value = CLIENT_VERSION;

    libinfo[i].name = "Build level";
    libinfo[i++].value = BUILD_TIMESTAMP;
    libinfo[i].name = NULL;
    libinfo[i].value = NULL;
    return libinfo;
}

void MQTTAsync_freeMessage(MQTTAsync_message **message) {
    MQTTProperties_free(&(*message)->properties);
    free((*message)->payload);
    free(*message);
    *message = NULL;
}


void MQTTAsync_free(void *memory) {
    free(memory);
}

static void MQTTAsync_freeServerURIs(MQTTAsyncs *m) {
    int i;

    for (i = 0; i < m->serverURIcount; ++i)
        free(m->serverURIs[i]);
    m->serverURIcount = 0;
    if (m->serverURIs)
        free(m->serverURIs);
    m->serverURIs = NULL;
}