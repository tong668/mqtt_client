//
// Created by didi on 2022/10/21.
//


#include <stdlib.h>
#include "MQTTClient.h"

#if defined(WIN32)
#define ADDRESS  "tcp://localhost:1883"
#else
#define ADDRESS  "tcp://192.168.37.1:1883"
#endif
#define CLIENTID "MQTTClient_sub"
#define TOPIC "mqtt/pub/test"

void ConnLost(void *context, char *cause) {
    printf("\nConnection lost\n");
    printf("cause: %s\n", cause);
}

int MsgArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("Message arrived\n");
    printf("  topic: %s\n", topicName);
    printf("message: %.*s\n", message->payloadlen, (char *) message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

MQTTClient_deliveryToken deliveryToken;

void DeliveryComplete(void *context, MQTTClient_deliveryToken dt) {
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveryToken = dt;
}

int main() {
    MQTTClient client;
    MQTTClient_connectOptions connOptions = MQTTClient_connectOptions_initializer;

    int res;
    if (res = MQTTClient_create(&client, ADDRESS, CLIENTID, 1, NULL) != MQTTCLIENT_SUCCESS) {
        printf("create mqtt client fail, res = %d\n", res);
        return 1;
    }
    if (res = MQTTClient_setCallbacks(client, NULL, ConnLost, MsgArrived, DeliveryComplete) != MQTTCLIENT_SUCCESS) {
        printf("mqtt client set callback fail, res = %d\n", res);
        return 1;
    }
    if (res = MQTTClient_connect(client, &connOptions) != MQTTCLIENT_SUCCESS) {
        printf("connect mqtt client fail, res = %d\n", res);
        return 1;
    }
    printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, CLIENTID, 1);

    if (res = MQTTClient_subscribe(client, TOPIC, 1) != MQTTCLIENT_SUCCESS) {
        printf("mqtt client subscribe fail, res = %d\n", res);
        return 1;
    } else{
        int ch;
        do
        {
            ch = getchar();
        } while (ch!='Q' && ch != 'q');

        if ((res = MQTTClient_unsubscribe(client, TOPIC)) != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to unsubscribe, return code %d\n", res);
            res = EXIT_FAILURE;
        }

        if (MQTTClient_disconnect(client,10000L) != MQTTCLIENT_SUCCESS){
            printf("mqtt client disconnect fail, res = %d\n",res);
            return 1;
        }
    }

    return 0;
}