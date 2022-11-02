//
// Created by Administrator on 2022/11/2.
//
#include <stdio.h>
#include "mqttclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TypeDefine.h"

#define ADDRESS     "tcp://192.168.101.4:1883"
#define CLIENTID    "mqtt_pub"
#define TOPIC       "MQTT Examples"
#define PAYLOAD     "hello mqtt!"
#define QOS         1
#define TIMEOUT     10000L


int main (){
    printf("hello mqtt!\n");

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
//    MQTTClient_message pubmsg = MQTTClient_message_initializer;
//    MQTTClient_deliveryToken token;
    int rc;
    if ((rc = MQTTClient_create(&client, ADDRESS, CLIENTID,
                                MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to create client, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    return 0;
}