///**
// * @file dd_mqtt_cli.c
// * @author your name (you@domain.com)
// * @brief
// * @version 0.1
// * @date 2021-12-20
// *
// * @copyright Copyright (c) 2021
// *
// */
//
//#include "osal_api.h"
//#include "dd_mqtt_cli.h"
//#include <pthread.h>
//#include <malloc.h>
//#include <unistd.h>
//
////paho库操作接口
//typedef void (*dd_mqtt_cli_delivered_fun)(void *, MQTTClient_deliveryToken);
//typedef int (*dd_mqtt_cli_msg_arrvd_fun)(void *, char *, int, MQTTClient_message*);
//typedef void (*dd_mqtt_cli_conn_lost_fun)(void *, char *);
//queue_msg_st mqtt_clis; //mqtt clis 管理链表
//
///**
// * @brief mqtt初始化，初始化后可以应用于mqtt同步发送、mqtt异步收发
// *
// * @param srv 服务器地址
// * @param id 节点名称
// * @param context 上下文参数，用于回调函数中
// * @param delivered 用于判断数据实发真是发送成功，如果只同步发送可以为NULL
// * @param msg_arrvd 用于数据异步接收，处理异步接收数据，不能为NULL,特别需要注意该函数返回值1表示消息成功接收并处理，0重新接收再处理
// * @param conn_lost 用于连接断开处理
// * @return dd_mqtt_cli_st*
// */
//static dd_paho_mqtt_cli_st *dd_mqtt_cli_async_init(char *srv, char *id, void *context,
//                                                   dd_mqtt_cli_delivered_fun delivered,
//                                                   dd_mqtt_cli_msg_arrvd_fun msg_arrvd,
//                                                   dd_mqtt_cli_conn_lost_fun conn_lost)
//{
//    int rc;
//    dd_paho_mqtt_cli_st *mqtt_cli = NULL;
//    static pthread_mutex_t mqtt_lck = PTHREAD_MUTEX_INITIALIZER;
//    MQTTClient_connectOptions conn = MQTTClient_connectOptions_initializer;
//    MQTTClient_SSLOptions ssl = MQTTClient_SSLOptions_initializer;
//    dd_mqtt_cli_st *cli = (dd_mqtt_cli_st *)(context);
//
//    if (srv == NULL || id == NULL)
//        return NULL;
//
//    mqtt_cli = calloc(1, sizeof(dd_paho_mqtt_cli_st));
//    if (mqtt_cli == NULL)
//        return NULL;
//
//    if (MQTTClient_create(&mqtt_cli->cli, srv, id, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTCLIENT_SUCCESS)
//        goto exit_malloc;
//
//    pthread_mutex_lock(&mqtt_lck);
//    rc = MQTTClient_setCallbacks(mqtt_cli->cli, context, conn_lost, msg_arrvd, delivered);
//    pthread_mutex_unlock(&mqtt_lck);
//    if(rc != MQTTCLIENT_SUCCESS)
//        goto exit_create;
//
//    conn.keepAliveInterval = 20;
//    conn.cleansession = 1;
//    if(cli->username != NULL)
//        conn.username = cli->username;
//    if(cli->passwd != NULL)
//        conn.password = cli->passwd;
//
//    if(cli->slt_mode.mode == 1)
//    {
//        ssl.trustStore = cli->slt_mode.ca_crt;
//        conn.ssl = &ssl;
//    }
//    else if (cli->slt_mode.mode == 2)
//    {
//        ssl.trustStore = cli->slt_mode.ca_crt;
//        ssl.privateKey = cli->slt_mode.cli_key;
//        ssl.keyStore = cli->slt_mode.cli_crt;
//        conn.ssl = &ssl;
//    }
//
//    if((rc = MQTTClient_connect(mqtt_cli->cli, &conn)) != MQTTCLIENT_SUCCESS) {
//        printf("MQTTClient_connect failure. ret[%d]", rc);
//        goto exit_create;
//    }
//
//    return mqtt_cli;
//
//exit_create:
//    MQTTClient_destroy(&mqtt_cli->cli);
//exit_malloc:
//    free(mqtt_cli);
//    return NULL;
//}
//
///**
// * @brief mqtt初始化逆操作
// *
// * @param mqtt_cli
// */
//static void dd_mqtt_cli_deinit(dd_paho_mqtt_cli_st* mqtt_cli)
//{
//    MQTTClient_disconnect(mqtt_cli->cli, 10);
//    MQTTClient_destroy(&mqtt_cli->cli);
//    free(mqtt_cli);
//    mqtt_cli = NULL;
//}
//
///**
// * @brief 网络断开后重新连接
// *
// * @param mqtt_cli
// * @return int 0-success
// */
//static int dd_mqtt_cli_reconnect(dd_paho_mqtt_cli_st* mqtt_cli)
//{
//    MQTTClient_connectOptions conn = MQTTClient_connectOptions_initializer;
//    conn.keepAliveInterval = 20;
//    conn.cleansession = 1;
//    return MQTTClient_connect(mqtt_cli->cli, &conn);
//}
//
///**
// * @brief mqtt同步发送，发送成功或者失败才返回，需要在同步或者异步初始化后使用
// *
// * @param mqtt_cli 操作句柄
// * @param topic 主题
// * @param buf
// * @param len
// * @param qos 服务质量，0-最多一次,1-至少一次,2-有且只有一次
// * @return int
// */
//int dd_mqtt_cli_sync_pub_msg(dd_paho_mqtt_cli_st* mqtt_cli, const char *topic, const char *buf, size_t len, int qos)
//{
//    int rc;
//    MQTTClient_message msg = MQTTClient_message_initializer;
//
//    msg.payload = (void *)buf;
//    msg.payloadlen = len;
//    msg.qos = qos;
//    msg.retained = 0;
//
//    if ((rc = MQTTClient_publishMessage(mqtt_cli->cli, topic, &msg, &mqtt_cli->delivered_token)) != MQTTCLIENT_SUCCESS)
//         return rc;
//
//    return  MQTTClient_waitForCompletion(mqtt_cli->cli, mqtt_cli->delivered_token, 300);
//}
//
///**
// * @brief 用于mqtt异步发送数据，发送后不代表发送成功，需要dd_mqtt_cli_async_init中delivered函数指针判断
// *
// * @param mqtt_cli 操作句柄
// * @param topic 主题
// * @param buf
// * @param len
// * @param qos 服务质量，0-最多一次,1-至少一次,2-有且只有一次
// * @return int
// */
//int dd_mqtt_cli_async_pub_msg(dd_paho_mqtt_cli_st* mqtt_cli, const char *topic, const char *buf, size_t len, int qos)
//{
//    MQTTClient_message msg = MQTTClient_message_initializer;
//
//    msg.payload = (void *)buf;
//    msg.payloadlen = len;
//    msg.qos = qos;
//    msg.retained = 0;
//
//    return MQTTClient_publishMessage(mqtt_cli->cli, topic, &msg, &mqtt_cli->delivered_token);
//}
//
///**
// * @brief 订阅消息
// *
// * @param cli
// * @param topic
// * @param qos
// * @return int 0-success
// */
//int dd_mqtt_cli_async_sub_msg(dd_paho_mqtt_cli_st* cli, const char *topic, int qos)
//{
//    return MQTTClient_subscribe(cli->cli, topic, qos);
//}
//
//int dd_mqtt_cli_async_unsub_msg(dd_paho_mqtt_cli_st* cli, const char *topic)
//{
//    return MQTTClient_unsubscribe(cli->cli, topic);
//}
//
///**
// * @brief 释放mqtt消息，用于dd_mqtt_cli_async_init中的msg_arrvd函数中，处理完消息调用此接口
// *
// * @param topic 消息的主题
// * @param message 消息内容
// */
//static void dd_mqtt_cli_async_sub_msg_free(char *topic, MQTTClient_message *message)
//{
//    MQTTClient_freeMessage(&message);
//    MQTTClient_free(topic);
//}
//
///**
// * @brief mqtt链路断开操作
// *
// * @param context dd_mqtt_cli句柄
// * @param cause
// */
//static void dd_mqtt_cli_connlost(void *context, char *cause)
//{
//    int i;
//    dd_mqtt_cli_opt_st *opt;
//    dd_mqtt_cli_st *cli = (dd_mqtt_cli_st *)(context);
//
//    printf("mqtt_cloud srv[%s] id[%s] conn lost, cause[%s]\n", cli->srv, cli->id, cause);
//    g_atomic_int_set(&cli->link_flag, 0);
//
//    while (1)
//    {
//        if (dd_mqtt_cli_reconnect(cli->cli) == 0)
//        {
//            g_atomic_int_set(&cli->link_flag, 1);
//            printf("mqtt_cloud srv[%s] id[%s] reconnect success", cli->srv, cli->id);
//            g_mutex_lock(&cli->mtx);
//            for(i = 0; i < cli->opts.length; i++)
//            {
//                opt = g_queue_peek_nth(&cli->opts, i);
//                opt->register_opt(NULL);
//            }
//            g_mutex_unlock(&cli->mtx);
//            break;
//        }
//        else
//        {
//            printf("mqtt_cloud srv[%s] id[%s] failure, sleep(2) reconnect", cli->srv, cli->id);
//            sleep(2);
//            continue;
//        }
//    }
//}
//
///**
// * @brief mqtt数据接收处理接口
// *
// * @param context dd_mqtt_cli句柄
// * @param topicName 数据topic名称
// * @param topicLen
// * @param message
// * @return int
// */
//static int dd_mqtt_cli_msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
//{
//    dd_mqtt_cli_st *cli = (dd_mqtt_cli_st *)(context);
//
//    push_queue_msgs(&cli->msgs, (uint8_t *)message->payload, message->payloadlen, topicName);
//    printf("mqtt_cloud cli recv: topic[%s], msgLen[%d], topic[%s]", topicName, message->payloadlen, topicName);
//    dd_mqtt_cli_async_sub_msg_free(topicName, message);
//
//    // printf("mqtt_cloud cli recv start: topic[%s], msgLen[%d]", topicName, message->payloadlen);
//    // g_mutex_lock(&cli->mtx);
//    // for(i = 0; i < cli->opts.length; i++)
//    // {
//    //     opt = g_queue_peek_nth(&cli->opts, i);
//    //     if((ret = opt->data_type_opt(topicName)) > 0)
//    //     {
//    //         g_mutex_unlock(&cli->mtx);
//    //         printf("mqtt_cloud cli recv: topic[%s], msgLen[%d], type[%d]", topicName, message->payloadlen, ret);
//    //         push_queue_msgs(&cli->msgs, (uint8_t *)message->payload, message->payloadlen, ret);
//    //         dd_mqtt_cli_async_sub_msg_free(topicName, message);
//    //         return 1;
//    //     }
//    // }
//    // g_mutex_unlock(&cli->mtx);
//    // dd_mqtt_cli_async_sub_msg_free(topicName, message);
//    // printf("mqtt_cloud cli recv: topic[%s], msgLen[%d], type[%d]", topicName, message->payloadlen, ret);
//
//    return 1;
//}
//
///**
// * @brief 回收cli的资源
// *
// * @param cli
// * @return int
// */
//static int dd_mqtt_cli_free(dd_mqtt_cli_st *cli)
//{
//    int i;
//    char *t;
//    dd_mqtt_cli_opt_st *op;
//
//    free(cli->srv);
//    free(cli->id);
//    if(cli->username != NULL)
//        free(cli->username);
//    if(cli->passwd != NULL)
//        free(cli->passwd);
//
//    for(i = 0; i < cli->topics.length; i++)
//    {
//        t = g_queue_peek_nth(&cli->topics, i);
//        free(t);
//    }
//    g_queue_clear(&cli->topics);
//    for(i = 0; i < cli->opts.length; i++)
//    {
//        op = g_queue_peek_nth(&cli->opts, i);
//        free(op);
//    }
//    g_queue_clear(&cli->opts);
//
//    queue_msg_exit(&cli->msgs);
//    free(cli);
//
//    return 0;
//}
//
///**
// * @brief 删除mqtt_clis队列中的cli
// *
// * @param cli
// * @return int
// */
//static int dd_mqtt_clis_del_cli(dd_mqtt_cli_st *cli)
//{
//    int i;
//    dd_mqtt_cli_st *c;
//
//    for(i = 0; i < mqtt_clis.queue.length; i++)
//    {
//        c = g_queue_peek_nth(&mqtt_clis.queue, i);
//        if(strcmp(c->srv, cli->srv) == 0)
//        {
//            g_queue_pop_nth(&mqtt_clis.queue, i);
//            return 0;
//        }
//    }
//
//    return -1;
//}
//
///**
// * @brief 非对外开放接口，当mqtt的cli创建是，会创建一个线程，启动此函数功能，主要有两个功能。
// *          1、处理cli的数据；
// *          2、clide topic都取消后，回收资源，推出函数
// *
// * @param arg
// * @return void*
// */
//static void *dd_mqtt_cli_task(void *arg)
//{
//    int cli_topic_length;
//    int ret, i;
//    char arr[10240];
//    uint16_t len;
//    char type[128];
//    dd_mqtt_cli_opt_st *opt = NULL;
//    dd_mqtt_cli_st *cli = (dd_mqtt_cli_st *)(arg);
//
//    while (1)
//    {
//        cli->cli = dd_mqtt_cli_async_init(cli->srv, cli->id,
//                                                  cli, NULL, dd_mqtt_cli_msgarrvd, dd_mqtt_cli_connlost);
//        if (cli->cli == NULL)
//        {
//            printf("mqtt_cloud srv[%s], id[%s] connect failure, sleep(2) after recnnect !!!", cli->srv, cli->id);
//            sleep(2);
//            continue;
//        }
//        printf("mqtt_cloud srv[%s], id[%s] connect cloud success!!!\n", cli->srv, cli->id);
//        g_mutex_lock(&cli->mtx);
//        g_atomic_int_set(&cli->link_flag, 1);
//        for(i = 0; i < cli->opts.length; i++)
//        {
//            opt = g_queue_peek_nth(&cli->opts, i);
//            if(opt->register_opt != NULL)
//                opt->register_opt();
//        }
//        g_mutex_unlock(&cli->mtx);
//
//        while (1)
//        {
//            memset(arr, 0, 10240);
//            memset(type, 0, 128);
//            ret = pop_queue_msg_until(&cli->msgs, (uint8_t *)arr, &len, type, 3000);
//            //超时
//            if(ret < 0)
//            {
//                if(cli->count == 0)
//                {
//                    printf("mqtt cli[%p] count == 0, cli task exit", cli);
//                    dd_mqtt_cli_free(cli);
//                    // free(cli);
//                    return NULL;
//                }
//                continue;
//            }
//            g_mutex_lock(&cli->mtx);
//            cli_topic_length = cli->opts.length;
//            for(i = 0; i< cli->opts.length; i++)
//            {
//                opt = g_queue_peek_nth(&cli->opts, i);
//                if(opt->data_type_opt(type) >= 0)
//                    break;
//            }
//            g_mutex_unlock(&cli->mtx);
//            if(i != cli_topic_length)
//                opt->handle_opt(arr, len, type);
//        }
//        break;
//    }
//}
//
///**
// * @brief 对外操作接口，mqtt cli管理链表初始化
// *
// * @return int8_t
// */
//int8_t dd_mqtt_clis_init()
//{
//    return queue_msg_init(&mqtt_clis);
//}
//
///**
// * @brief 对外开放操作接口，动态添加mqtt的topic，如果当前cli不存在，会自动创建cli
// *
// * @param outcli 客户端操作符
// * @param srv cli的srv地址
// * @param id cli的id
// * @param topic 添加的topic
// * @param username 服务器用户名
// * @param passwd 服务器地址
// * @param stlMode slt操作结构体
// * @param opt 模块操作接口
// * @return dd_mqtt_cli_st* 返回统一接口
// */
//dd_mqtt_cli_st* dd_mqtt_topic_add(dd_mqtt_cli_st** outcli, char *srv, char *id, char *topic, char *username, char* passwd, dd_mqtt_slt_mode_t stl, dd_mqtt_cli_opt_st *opt)
//{
//    int i, j;
//    char *t = NULL, *t1 = NULL;
//    dd_mqtt_cli_opt_st *op = NULL;
//    dd_mqtt_cli_st *cli = NULL;
//    osal_task_t task_fd;
//
//    if(srv == NULL || id == NULL || topic == NULL || opt == NULL)
//        return NULL;
//
//    g_mutex_lock(&mqtt_clis.mutex);
//    for(i = 0; i < mqtt_clis.queue.length; i++)
//    {
//        cli = g_queue_peek_nth(&mqtt_clis.queue, i);
//        //存在cli
//        if(cli->srv != NULL && strcmp(srv, cli->srv) == 0)
//        {
//            g_mutex_lock(&cli->mtx);
//            for(j = 0; j < cli->topics.length; j++)
//            {
//                t1 = g_queue_peek_nth(&cli->topics, j);
//                //topic已经存在
//                if(strcmp(t1, topic) == 0)
//                {
//                    printf("dd_mqtt_topic_add. srv[%s],topic[%s] = queue_topic[%s] has exist.", srv, topic, t1);
//                    g_mutex_unlock(&cli->mtx);
//                    g_mutex_unlock(&mqtt_clis.mutex);
//                    return cli;
//                }
//            }
//            //topic不存在
//            t = calloc(1, strlen(topic)+1);
//            op = calloc(1, sizeof(dd_mqtt_cli_opt_st));
//            if(t == NULL || op == NULL)
//            {
//                free(t);
//                free(op);
//                goto unlock2_exit;
//            }
//            strcpy(t, topic);
//            g_queue_push_tail(&cli->topics, t);
//            op->init_opt = opt->init_opt;
//            op->register_opt = opt->register_opt;
//            op->unregister_opt = op->unregister_opt;
//            op->handle_opt = opt->handle_opt;
//            op->data_type_opt = opt->data_type_opt;
//            g_queue_push_tail(&cli->opts, op);
//
//            if(opt->init_opt != NULL)
//                opt->init_opt();
//            //如果链接已建立，注册服务,
//            if(cli->link_flag == 1 && opt->register_opt != NULL)
//            {
//                opt->register_opt();
//            }
//            cli->count++;
//            printf("dd_mqtt_topic_add. cli[%p], srv[%s] exist, add topic[%s] success", cli, srv, t);
//            g_mutex_unlock(&cli->mtx);
//            g_mutex_unlock(&mqtt_clis.mutex);
//            if(outcli != NULL)
//                *outcli = cli;
//            return cli;
//        }
//    }
//    //没有cli
//    if(i == 0 || i == mqtt_clis.queue.length)
//    {
//        cli = calloc(1, sizeof(dd_mqtt_cli_st));
//        if(cli == NULL)
//            goto unlock1_exit;
//        cli->srv = calloc(1, strlen(srv)+1);
//        cli->id = calloc(1, strlen(id)+1);
//        t = calloc(1, strlen(topic)+1);
//        op = calloc(1, sizeof(dd_mqtt_cli_opt_st));
//        if(cli->srv == NULL || cli->id == NULL || t== NULL || op == NULL)
//        {
//            free(cli->srv);
//            free(cli->id);
//            free(t);
//            free(op);
//            free(cli);
//            goto unlock1_exit;
//        }
//        if(username != NULL && passwd != NULL) {
//            cli->username = calloc(1, strlen(username)+1);
//            cli->passwd = calloc(1, strlen(passwd)+1);
//            if(cli->username == NULL || cli->passwd == NULL) {
//                free(cli->username);
//                free(cli->passwd);
//                goto unlock1_exit;
//            }
//        }
//
//        //初始化队列，锁等
//        g_queue_init(&cli->topics);
//        g_queue_init(&cli->opts);
//        queue_msg_init(&cli->msgs);
//        g_mutex_init(&cli->mtx);
//
//        //操作cli结构体
//        strcpy(cli->srv, srv);
//        strcpy(cli->id, id);
//        strcpy(t, topic);
//        if(username != NULL)
//            strcpy(cli->username, username);
//        if(passwd != NULL)
//            strcpy(cli->passwd, passwd);
//        memmove(&cli->slt_mode, &stl, sizeof(dd_mqtt_slt_mode_t));
//        g_queue_push_tail(&cli->topics, t);
//        op->init_opt = opt->init_opt;
//        op->register_opt = opt->register_opt;
//        op->unregister_opt = op->unregister_opt;
//        op->handle_opt = opt->handle_opt;
//        op->data_type_opt = opt->data_type_opt;
//        g_queue_push_tail(&cli->opts, op);
//        cli->count++;
//        if(outcli != NULL)
//            *outcli = cli;
//
//        //初始化cli，并启动链接服务
//        if(opt->init_opt != NULL)
//            opt->init_opt();
//        printf("mqtt srv no exit, create cli[%p], srv[%s], id[%s], topic[%s] success", cli, srv, id, topic);
//        osal_task_create(&task_fd, cli->srv, dd_mqtt_cli_task, cli, (1024 * 8192), 30);
//
//        //添加到clis管理队列中
//        g_queue_push_tail(&mqtt_clis.queue, cli);
//        g_mutex_unlock(&mqtt_clis.mutex);
//
//        return cli;
//    }
//
//unlock1_exit:
//    g_mutex_unlock(&mqtt_clis.mutex);
//    return NULL;
//unlock2_exit:
//    g_mutex_unlock(&cli->mtx);
//    g_mutex_unlock(&mqtt_clis.mutex);
//    return NULL;
//}
//
///**
// * @brief 对外开放接口，动态取消topic操作，当所有topic都取消后，cli会在task中删除，此函数不处理
// *
// * @param srv cli的srv地址
// * @param id cli的id
// * @param topic 取消的topic
// * @return int 取消成功，返回0，失败（不存在此topic）返回-1
// */
//int dd_mqtt_topic_del(char *srv, char *id, char *topic)
//{
//    int ret = -1;
//    int i, j;
//    char *t;
//    dd_mqtt_cli_opt_st *op;
//    dd_mqtt_cli_st *cli = NULL;
//
//    if(srv == NULL || topic == NULL)
//        return -1;
//
////    g_mutex_lock(&mqtt_clis.mutex);
////    for(i = 0; i < mqtt_clis.queue.length; i++)
////    {
////        cli = g_queue_peek_nth(&mqtt_clis.queue, i);
////        if(strcmp(cli->srv, srv) == 0)
////        {
////            g_mutex_lock(&cli->mtx);
////            for(j = 0; j < cli->topics.length; j++)
////            {
////                t = g_queue_peek_nth(&cli->topics, j);
////                if(strcmp(t, topic) == 0)
////                {
////                    printf("dd_mqtt_topic_del success. src[%s], id[%s], topic[%s]", srv, id, topic);
////                    op = g_queue_peek_nth(&cli->opts, j);
////                    if(op->unregister_opt != NULL)
////                        op->unregister_opt();
////                    t = g_queue_pop_nth(&cli->topics, j);
////                    free(t);
////                    g_queue_pop_nth(&cli->opts, j);
////                    free(op);
////                    cli->count--;
////                    //此函数调用可能在task任务handle_opt接口中，此处只把cli从mqtt_clis中队列删除,断开链接，本身清理放在task中。
////                    if(cli->count == 0)
////                    {
////                        printf("mqtt cli[%p] count == 0, cli exit", cli);
////                        dd_mqtt_cli_deinit(cli->cli);
////                        dd_mqtt_clis_del_cli(cli);
////                    }
////                    ret = 0;
////                    break;
////                }
////            }
////            g_mutex_unlock(&cli->mtx);
////            break;
////        }
////    }
////    g_mutex_unlock(&mqtt_clis.mutex);
//
//    return ret;
//}
//
