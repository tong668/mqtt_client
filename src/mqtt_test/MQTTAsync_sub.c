//
// Created by didi on 2022/10/25.
//

#include "MQTTAsync.h"

#if defined(WIN32)
#include <Windows.h>
#define ADDRESS  "tcp://localhost:1883"
#else
#define ADDRESS  "tcp://172.29.86.87:1883"

#include <unistd.h>

#endif

#define CLIENTID "MQTTAsync_sub"
#define TOPIC "mqtt/pub/test"

void ConnLost() {

}

int MessageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message) {
    printf("Message arrived\n");
    printf("  topic: %s\n", topicName);
    printf("message: %.*s\n", message->payloadlen, (char *) message->payload);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void OnConnect(void *context, MQTTAsync_successData *response) {
    MQTTAsync client = context;
    MQTTAsync_responseOptions responseOptions = MQTTAsync_responseOptions_initializer;

    int res;
    responseOptions.context = client;

    if (res = MQTTAsync_subscribe(client, TOPIC, 1, &responseOptions) != MQTTASYNC_SUCCESS) {
        printf("mqtt client subscribe fail, res = %d\n", res);
        return;
    }
}

int main() {
    MQTTAsync client;
    MQTTAsync_connectOptions connOpts = MQTTAsync_connectOptions_initializer;

    int res;
    if (res = MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTASYNC_SUCCESS) {
        printf("mqtt create client fail, res = %d\n", res);
        return 1;
    }
    if (res = MQTTAsync_setCallbacks(client, NULL, ConnLost, MessageArrived, NULL) != MQTTASYNC_SUCCESS) {

    }
    connOpts.onSuccess = OnConnect;
    connOpts.context = client;
    if (res = MQTTAsync_connect(client, &connOpts) != MQTTASYNC_SUCCESS) {
        printf("mqtt connect fail, res = %d", res);
        return 1;
    }

    while (1) {
#if defined(WIN32)

        Sleep(100);
    }
#else
        sleep(10);
    }
#endif
    return 0;
}