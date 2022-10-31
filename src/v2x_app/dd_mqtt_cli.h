/**
 * @file dd_mqtt_cli.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-12-20
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef __DD_MQTT_CLI_H__
#define __DD_MQTT_CLI_H__

#include <stdint.h>
#include <string.h>
//#include "queue_msg.h"
#include "MQTTClient.h"

//自定义功能操作接口
typedef void (*dd_mqtt_cli_init_func)();
typedef int (*dd_mqtt_cli_data_type_func)(char *topic);
typedef int (*dd_mqtt_cli_handle_func)(char *arr, int16_t len, char* topic);
typedef void (*dd_mqtt_cli_register_func)();
typedef void (*dd_mqtt_cli_unregister_func)();

typedef struct dd_mqtt_cli_opt
{
    dd_mqtt_cli_init_func init_opt; //加入cli前初始化操作
    dd_mqtt_cli_data_type_func data_type_opt; //判断接收数据类型，topic符合，转化为int的type
    dd_mqtt_cli_handle_func handle_opt; //命令解析使用
    dd_mqtt_cli_register_func register_opt; //链接和重连后注册服务，如果需要第一次链接启动，在此处加static实现
    dd_mqtt_cli_unregister_func unregister_opt; //取消注册服务
    // int type; 
}dd_mqtt_cli_opt_st;

typedef struct
{
    MQTTClient cli; //建立连接客户端句柄
    MQTTClient_message msg;
    MQTTClient_deliveryToken delivered_token; //用于判断发送成功令牌
} dd_paho_mqtt_cli_st;

typedef struct dd_mqtt_slt_mode_st
{
    uint8_t mode; //0不验证，1单向认证；2双向认证
    char* ca_crt;
    // char* srv_key;
    // char* srv_crt;
    char* cli_key;
    char* cli_crt;
}dd_mqtt_slt_mode_t;
typedef struct dd_mqtt_cli
{
    char *srv; //服务端url
    char *id; //客户端id
//    GQueue topics; //主题队列
//    GQueue opts; //各种服务操作队列
//    queue_msg_st msgs; //接收数据队列
    dd_paho_mqtt_cli_st *cli; //客户端操作句柄
    dd_mqtt_slt_mode_t slt_mode; //slt操作模式
    char *username;
    char *passwd;
    int link_flag; //链路链接状态
    int count; //引用计数
    // int type; //注册topic对应的数值类型
//    GMutex mtx;
}dd_mqtt_cli_st;

#ifdef __cplusplus
extern "C"
{
#endif
int dd_mqtt_cli_async_sub_msg(dd_paho_mqtt_cli_st* cli, const char *topic, int qos);
int dd_mqtt_cli_async_unsub_msg(dd_paho_mqtt_cli_st* cli, const char *topic);
int dd_mqtt_cli_sync_pub_msg(dd_paho_mqtt_cli_st* mqtt_cli, const char *topic, const char *buf, size_t len, int qos);
int dd_mqtt_cli_async_pub_msg(dd_paho_mqtt_cli_st* mqtt_cli, const char *topic, const char *buf, size_t len, int qos);
int8_t dd_mqtt_clis_init();
// dd_mqtt_cli_st* dd_mqtt_topic_add(char *srv, char *id, char *topic, dd_mqtt_cli_opt_st *opt);
dd_mqtt_cli_st* dd_mqtt_topic_add(dd_mqtt_cli_st** outcli, char *srv, char *id, char *topic, char *username, char* passwd, dd_mqtt_slt_mode_t stl, dd_mqtt_cli_opt_st *opt);
int dd_mqtt_topic_del(char *srv, char *id, char *topic);
#ifdef __cplusplus
}
#endif
#endif
