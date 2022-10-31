//
// Created by didi on 2022/10/26.
//

#include "MQTTAsync.h"
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#if defined(WIN32)
#include <Windows.h>
#define ADDRESS  "tcp://localhost:1883"
#else
#define ADDRESS  "tcp://172.29.86.87:1883"

#include <unistd.h>
#include <string.h>

#endif
#define CLIENTID        "MQTTAsync_pub_time"
#define TOPIC           "mqtt/pub/test"
#define QOS             1
#define TIMEOUT         10000L
#define SAMPLE_PERIOD   10L    // in ms

bool connected = false;
bool finished = false;

void OnConnect() {
    printf("mqtt client connect success.\n");
    connected = true;

}

int64_t GetTime() {
#if defined(WIN32)
#todo
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    printf("sec = %ld , nsec = %ld \n", ts.tv_sec, ts.tv_nsec);
    return (int64_t) ts.tv_sec * 1000 + (int64_t) ts.tv_nsec / 1000000;
#endif
}

void OnSend(){

}
int main() {
    MQTTAsync client;
    MQTTAsync_connectOptions connOpts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_responseOptions responseOpts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message msg = MQTTAsync_message_initializer;

    int res;
    if (res = MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTASYNC_SUCCESS) {
        printf("mqtt client create client object fail, res = %d\n", res);
        exit(1);
    }
    connOpts.onSuccess = OnConnect;
    connOpts.context = client;
    if (res = MQTTAsync_connect(client, &connOpts) != MQTTASYNC_SUCCESS) {
        printf("mqtt client connect fail, res = %d", res);
        exit(1);
    }

    while (!connected) {
#if defined(_WIN32)
        Sleep(100);
#else
        usleep(100000L);
#endif
    }

    while (!finished) {

        int64_t data = GetTime();

        char buf[256];
        int n = snprintf(buf, sizeof(buf), "%lld", (long long) data);
        printf("buf = %s\n", buf);

        msg.payload = buf;
        msg.payloadlen = n;
        msg.qos = 1;
        msg.retained = 0;

        responseOpts.onSuccess = OnSend;
        responseOpts.context = client;

        if (res = MQTTAsync_sendMessage(client, TOPIC, &msg, &responseOpts) != MQTTASYNC_SUCCESS) {
            printf("mqtt client send message fail, res =%d \n", res);
            exit(1);
        }
        sleep(1);
    }

    MQTTAsync_destroy(&client);

    return 0;
}