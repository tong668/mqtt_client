//
// Created by didi on 2022/10/21.
//
#include <string.h>
#include "MQTTClient.h"
#include "RunningPerformance.h"

#if defined(WIN32)
#define ADDRESS  "tcp://localhost:1883"
#else
#define ADDRESS  "tcp://192.168.37.1:1883"
#endif
#define CLIENTID  "MQTTClient_pub"
#define TOPIC "mqtt/pub/test"
#define PAYLOAD "hello mqtt!"


int main() {
    MQTTClient client;
    MQTTClient_connectOptions connOptions = MQTTClient_connectOptions_initializer;
    MQTTClient_message msg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken deliveryToken;
    int res;

    if (res = MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTCLIENT_SUCCESS) {
        printf("mqtt client create fail , res = %d\n", res);
        return 1;
    }
    printf("client = %p\n", client);
    if (res = MQTTClient_connect(client, &connOptions) != MQTTCLIENT_SUCCESS) {
        printf("mqtt client connect fail, res = %d\n", res);
        return 1;
    }
    int num = 100;
    while (num--) {
        updateMemInfo();
        char buf[256];
        memset(buf,0,sizeof(buf));
        memcpy(buf,&memOut,sizeof(memOut));
//        int n = snprintf(buf, sizeof(buf), "%lld", (long long) memOut.total);
//        printf("buf = %s\n", buf);
//        for (int i = 0; i < sizeof (buf); ++i) {
//            printf( "buf = %d", buf[i]);
//        }

        msg.payload = buf;
        msg.payloadlen = strlen(msg.payload);
        msg.qos = 1;
        if (res = MQTTClient_publishMessage(client, TOPIC, &msg, &deliveryToken) != MQTTCLIENT_SUCCESS) {
            printf("fail to publish mqtt message, res = %d\n", res);
            return 1;
        }
        if (MQTTClient_waitForCompletion(client, deliveryToken, 10000L) != MQTTCLIENT_SUCCESS) {
            printf("mqtt wait for complete fail, delivery token is %d\n", deliveryToken);
            return 1;
        }
        printf("mqtt deliver message successful with delivery token %d\n", deliveryToken);
        sleep(1);
    }


    MQTTClient_destroy(&client);
    return 0;
}