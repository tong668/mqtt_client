//
// Created by didi on 2022/10/24.
//

#include <string.h>
#include "MQTTAsync.h"
#include <stdbool.h>
#include <unistd.h>

#if defined(WIN32)
#include <Windows.h>
#define ADDRESS  "tcp://localhost:1883"
#else
#define ADDRESS  "tcp://172.29.86.87:1883"
#endif

#define CLIENTID  "MQTTAsync_pub"
#define TOPIC "mqtt/pub/test"
#define PAYLOAD "hello mqtt!"


int finished = false;

void ConnLost(void *context, char *cause) {
    MQTTAsync client = (MQTTAsync) context;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    int rc;

    printf("\nConnection lost\n");
    printf(" cause: %s\n", cause);

    printf("Reconnecting\n");
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to start connect, return code %d\n", rc);
        finished = 1;
    }
}

int MessageArrive(void *context, char *topicName, int topicLen, MQTTAsync_message *m) {
    /*no expect message coming*/
    return 0;
}

void OnDisconnectFailure(void *context, MQTTAsync_failureData *response) {
    printf("Disconnect failed\n");
    finished = 1;
}

void OnDisconnect(void *context, MQTTAsync_successData *response) {
    printf("Successful disconnection\n");
    finished = 1;
}

void OnSend(void *context, MQTTAsync_successData *response) {
    MQTTAsync client = (MQTTAsync) context;
    MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
    int rc;

    printf("Message with token value %d delivery confirmed\n", response->token);
    opts.onSuccess = OnDisconnect;
    opts.onFailure = OnDisconnectFailure;
    opts.context = client;
    if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to start disconnect, return code %d\n", rc);
        return;
    }
}

void OnConnect(void *context, MQTTAsync_successData *response) {
    MQTTAsync client = (MQTTAsync) context;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message msg = MQTTAsync_message_initializer;
    int res;

    printf("Successful connection\n");
    msg.payload = PAYLOAD;
    msg.payloadlen = strlen(PAYLOAD);
    msg.qos = 1;

    opts.onSuccess = OnSend;
    opts.context = client;
    if (res = MQTTAsync_sendMessage(client, TOPIC, &msg, &opts) != MQTTASYNC_SUCCESS) {
        printf("mqtt client send message fail, res = %d", res);
        return;
    }


}

void OnConnectFail(void *context, MQTTAsync_failureData *response) {
    printf("Connect failed, rc %d\n", response ? response->code : 0);
    finished = 1;
}

int main() {
    MQTTAsync client;
    MQTTAsync_connectOptions connOpts = MQTTAsync_connectOptions_initializer;

    int res = 0;

    if (res = MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTASYNC_SUCCESS) {
        printf("mqtt create client fail, res = %d\n", res);
        return 1;
    }

//    if (MQTTAsync_setCallbacks(client, NULL, ConnLost, MessageArrive, NULL)) {
//
//    }
//    connOpts.keepAliveInterval = 20;
//    connOpts.cleansession = 1;
    connOpts.onSuccess = OnConnect;
//    connOpts.onFailure = OnConnectFail;
    connOpts.context = client;
    if (res = MQTTAsync_connect(client, &connOpts) != MQTTASYNC_SUCCESS) {
        printf("mqtt connect client fail, rs = %d\n", res);
    }

    printf("Waiting for publication of %s\n"
           "on topic %s for client with ClientID: %s\n",
           PAYLOAD, TOPIC, CLIENTID);
    while (!finished)
#if defined(_WIN32)
        Sleep(100);
#else
    usleep(10000L);
#endif
    return 0;
}