//
// Created by Administrator on 2022/11/2.
//

#include <string.h>
#include <utf-8.h>
#include "mqttclient.h"
#include "Thread.h"


int MQTTClient_create(MQTTClient *handle, const char *serverURI, const char *clientId,
                      int persistence_type, void *persistence_context) {
    return MQTTClient_createWithOptions(handle, serverURI, clientId, persistence_type,
                                        persistence_context, NULL);
}
int MQTTClient_createWithOptions(MQTTClient *handle, const char *serverURI, const char *clientId,
                                 int persistence_type, void *persistence_context, MQTTClient_createOptions *options) {
    int rc = 0;
//    MQTTClients *m = NULL;
//    if ((rc = Thread_lock_mutex(mqttclient_mutex)) != 0)
//        goto exit;
//
//    if (serverURI == NULL || clientId == NULL) {
//        rc = MQTTCLIENT_NULL_PARAMETER;
//        goto exit;
//    }
//
//    if (!UTF8_validateString(clientId)) {
//        rc = MQTTCLIENT_BAD_UTF8_STRING;
//        goto exit;
//    }
//
//    if (strlen(clientId) == 0 && persistence_type == MQTTCLIENT_PERSISTENCE_DEFAULT) {
//        rc = MQTTCLIENT_PERSISTENCE_ERROR;
//        goto exit;
//    }
//
//    if (strstr(serverURI, "://") != NULL) {
//        if (strncmp(URI_TCP, serverURI, strlen(URI_TCP)) != 0
//            && strncmp(URI_WS, serverURI, strlen(URI_WS)) != 0
//                ) {
//            rc = MQTTCLIENT_BAD_PROTOCOL;
//            goto exit;
//        }
//    }
//
//    if (options && (strncmp(options->struct_id, "MQCO", 4) != 0 || options->struct_version != 0)) {
//        rc = MQTTCLIENT_BAD_STRUCTURE;
//        goto exit;
//    }
//
//    if (!library_initialized) {
//        Log_initialize((Log_nameValue *) MQTTClient_getVersionInfo());
//        bstate->clients = ListInitialize();
//        Socket_outInitialize();
//        Socket_setWriteCompleteCallback(MQTTClient_writeComplete);
//        Socket_setWriteAvailableCallback(MQTTProtocol_writeAvailable);
//        handles = ListInitialize();
//        library_initialized = 1;
//    }
//
//    if ((m = malloc(sizeof(MQTTClients))) == NULL) {
//        rc = PAHO_MEMORY_ERROR;
//        goto exit;
//    }
//    *handle = m;
//    memset(m, '\0', sizeof(MQTTClients));
//    m->commandTimeout = 10000L;
//    if (strncmp(URI_TCP, serverURI, strlen(URI_TCP)) == 0)
//        serverURI += strlen(URI_TCP);
//    else if (strncmp(URI_WS, serverURI, strlen(URI_WS)) == 0) {
//        serverURI += strlen(URI_WS);
//        m->websocket = 1;
//    } else if (strncmp(URI_SSL, serverURI, strlen(URI_SSL)) == 0) {
//
//        rc = MQTTCLIENT_SSL_NOT_SUPPORTED;
//        goto exit;
//    } else if (strncmp(URI_WSS, serverURI, strlen(URI_WSS)) == 0) {
//
//        rc = MQTTCLIENT_SSL_NOT_SUPPORTED;
//        goto exit;
//    }
//    m->serverURI = MQTTStrdup(serverURI);
//    ListAppend(handles, m, sizeof(MQTTClients));
//
//    if ((m->c = malloc(sizeof(Clients))) == NULL) {
//        ListRemove(handles, m);
//        rc = PAHO_MEMORY_ERROR;
//        goto exit;
//    }
//    memset(m->c, '\0', sizeof(Clients));
//    m->c->context = m;
//    m->c->MQTTVersion = (options) ? options->MQTTVersion : MQTTVERSION_DEFAULT;
//    m->c->outboundMsgs = ListInitialize();
//    m->c->inboundMsgs = ListInitialize();
//    m->c->messageQueue = ListInitialize();
//    m->c->outboundQueue = ListInitialize();
//    m->c->clientID = MQTTStrdup(clientId);
//    m->connect_sem = Thread_create_sem(&rc);
//    m->connack_sem = Thread_create_sem(&rc);
//    m->suback_sem = Thread_create_sem(&rc);
//    m->unsuback_sem = Thread_create_sem(&rc);
//
//    ListAppend(bstate->clients, m->c, sizeof(Clients) + 3 * sizeof(List));
//
//    exit:
//    Thread_unlock_mutex(mqttclient_mutex);
    return rc;
}
