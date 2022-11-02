//
// Created by Administrator on 2022/11/2.
//

#ifndef MQTT_CLIENT_MQTTPACKET_H
#define MQTT_CLIENT_MQTTPACKET_H

#include <stdbool.h>

typedef union
{
    /*unsigned*/ char byte;	/**< the whole byte */
    struct
    {
        bool retain : 1;		/**< retained flag bit */
        unsigned int qos : 2;	/**< QoS value, 0, 1 or 2 */
        bool dup : 1;			/**< DUP flag bit */
        unsigned int type : 4;	/**< message type nibble */
    } bits;

} Header;

typedef struct
{
    Header header;	/**< MQTT header byte */
} MQTTPacket;

#endif //MQTT_CLIENT_MQTTPACKET_H
