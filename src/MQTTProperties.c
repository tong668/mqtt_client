//
// Created by Administrator on 2022/11/3.
//

#include "MQTTProperties.h"
#include "MQTTPacket.h"
#include <memory.h>
#include "Log.h"

static struct nameToType
{
    enum MQTTPropertyCodes name;
    enum MQTTPropertyTypes type;
} namesToTypes[] =
        {
                {MQTTPROPERTY_CODE_PAYLOAD_FORMAT_INDICATOR, MQTTPROPERTY_TYPE_BYTE},
                {MQTTPROPERTY_CODE_MESSAGE_EXPIRY_INTERVAL, MQTTPROPERTY_TYPE_FOUR_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_CONTENT_TYPE, MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING},
                {MQTTPROPERTY_CODE_RESPONSE_TOPIC, MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING},
                {MQTTPROPERTY_CODE_CORRELATION_DATA, MQTTPROPERTY_TYPE_BINARY_DATA},
                {MQTTPROPERTY_CODE_SUBSCRIPTION_IDENTIFIER, MQTTPROPERTY_TYPE_VARIABLE_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_SESSION_EXPIRY_INTERVAL, MQTTPROPERTY_TYPE_FOUR_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_ASSIGNED_CLIENT_IDENTIFER, MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING},
                {MQTTPROPERTY_CODE_SERVER_KEEP_ALIVE, MQTTPROPERTY_TYPE_TWO_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_AUTHENTICATION_METHOD, MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING},
                {MQTTPROPERTY_CODE_AUTHENTICATION_DATA, MQTTPROPERTY_TYPE_BINARY_DATA},
                {MQTTPROPERTY_CODE_REQUEST_PROBLEM_INFORMATION, MQTTPROPERTY_TYPE_BYTE},
                {MQTTPROPERTY_CODE_WILL_DELAY_INTERVAL, MQTTPROPERTY_TYPE_FOUR_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_REQUEST_RESPONSE_INFORMATION, MQTTPROPERTY_TYPE_BYTE},
                {MQTTPROPERTY_CODE_RESPONSE_INFORMATION, MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING},
                {MQTTPROPERTY_CODE_SERVER_REFERENCE, MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING},
                {MQTTPROPERTY_CODE_REASON_STRING, MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING},
                {MQTTPROPERTY_CODE_RECEIVE_MAXIMUM, MQTTPROPERTY_TYPE_TWO_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_TOPIC_ALIAS_MAXIMUM, MQTTPROPERTY_TYPE_TWO_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_TOPIC_ALIAS, MQTTPROPERTY_TYPE_TWO_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_MAXIMUM_QOS, MQTTPROPERTY_TYPE_BYTE},
                {MQTTPROPERTY_CODE_RETAIN_AVAILABLE, MQTTPROPERTY_TYPE_BYTE},
                {MQTTPROPERTY_CODE_USER_PROPERTY, MQTTPROPERTY_TYPE_UTF_8_STRING_PAIR},
                {MQTTPROPERTY_CODE_MAXIMUM_PACKET_SIZE, MQTTPROPERTY_TYPE_FOUR_BYTE_INTEGER},
                {MQTTPROPERTY_CODE_WILDCARD_SUBSCRIPTION_AVAILABLE, MQTTPROPERTY_TYPE_BYTE},
                {MQTTPROPERTY_CODE_SUBSCRIPTION_IDENTIFIERS_AVAILABLE, MQTTPROPERTY_TYPE_BYTE},
                {MQTTPROPERTY_CODE_SHARED_SUBSCRIPTION_AVAILABLE, MQTTPROPERTY_TYPE_BYTE}
        };


static char* datadup(const MQTTLenString* str)
{
    char* temp = malloc(str->len);
    if (temp)
        memcpy(temp, str->data, str->len);
    return temp;
}


int MQTTProperty_getType(enum MQTTPropertyCodes value)
{
    int i, rc = -1;

    for (i = 0; i < ARRAY_SIZE(namesToTypes); ++i)
    {
        if (namesToTypes[i].name == value)
        {
            rc = namesToTypes[i].type;
            break;
        }
    }
    return rc;
}


int MQTTProperties_len(MQTTProperties* props)
{
    /* properties length is an mbi */
    return (props == NULL) ? 1 : props->length + MQTTPacket_VBIlen(props->length);
}


int MQTTProperties_add(MQTTProperties* props, const MQTTProperty* prop)
{
    int rc = 0, type;

    if ((type = MQTTProperty_getType(prop->identifier)) < 0)
    {
        /*StackTrace_printStack(stdout);*/
        rc = MQTT_INVALID_PROPERTY_ID;
        goto exit;
    }
    else if (props->array == NULL)
    {
        props->max_count = 10;
        props->array = malloc(sizeof(MQTTProperty) * props->max_count);
    }
    else if (props->count == props->max_count)
    {
        props->max_count += 10;
        props->array = realloc(props->array, sizeof(MQTTProperty) * props->max_count);
    }

    if (props->array)
    {
        int len = 0;

        props->array[props->count++] = *prop;
        /* calculate length */
        switch (type)
        {
            case MQTTPROPERTY_TYPE_BYTE:
                len = 1;
                break;
            case MQTTPROPERTY_TYPE_TWO_BYTE_INTEGER:
                len = 2;
                break;
            case MQTTPROPERTY_TYPE_FOUR_BYTE_INTEGER:
                len = 4;
                break;
            case MQTTPROPERTY_TYPE_VARIABLE_BYTE_INTEGER:
                len = MQTTPacket_VBIlen(prop->value.integer4);
                break;
            case MQTTPROPERTY_TYPE_BINARY_DATA:
            case MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING:
            case MQTTPROPERTY_TYPE_UTF_8_STRING_PAIR:
                len = 2 + prop->value.data.len;
                props->array[props->count-1].value.data.data = datadup(&prop->value.data);
                if (type == MQTTPROPERTY_TYPE_UTF_8_STRING_PAIR)
                {
                    len += 2 + prop->value.value.len;
                    props->array[props->count-1].value.value.data = datadup(&prop->value.value);
                }
                break;
        }
        props->length += len + 1; /* add identifier byte */
    }
    else
        rc = PAHO_MEMORY_ERROR;

    exit:
    return rc;
}

int MQTTProperty_write(char** pptr, MQTTProperty* prop)
{
    int rc = -1,
            type = -1;

    type = MQTTProperty_getType(prop->identifier);
    if (type >= MQTTPROPERTY_TYPE_BYTE && type <= MQTTPROPERTY_TYPE_UTF_8_STRING_PAIR)
    {
        writeChar(pptr, prop->identifier);
        switch (type)
        {
            case MQTTPROPERTY_TYPE_BYTE:
                writeChar(pptr, prop->value.byte);
                rc = 1;
                break;
            case MQTTPROPERTY_TYPE_TWO_BYTE_INTEGER:
                writeInt(pptr, prop->value.integer2);
                rc = 2;
                break;
            case MQTTPROPERTY_TYPE_FOUR_BYTE_INTEGER:
                writeInt4(pptr, prop->value.integer4);
                rc = 4;
                break;
            case MQTTPROPERTY_TYPE_VARIABLE_BYTE_INTEGER:
                rc = MQTTPacket_encode(*pptr, prop->value.integer4);
                *pptr += rc;
                break;
            case MQTTPROPERTY_TYPE_BINARY_DATA:
            case MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING:
                writeMQTTLenString(pptr, prop->value.data);
                rc = prop->value.data.len + 2; /* include length field */
                break;
            case MQTTPROPERTY_TYPE_UTF_8_STRING_PAIR:
                writeMQTTLenString(pptr, prop->value.data);
                writeMQTTLenString(pptr, prop->value.value);
                rc = prop->value.data.len + prop->value.value.len + 4; /* include length fields */
                break;
        }
    }
    return rc + 1; /* include identifier byte */
}


int MQTTProperties_write(char** pptr, const MQTTProperties* properties)
{
    int rc = -1;
    int i = 0, len = 0;

    /* write the entire property list length first */
    if (properties == NULL)
    {
        *pptr += MQTTPacket_encode(*pptr, 0);
        rc = 1;
    }
    else
    {
        *pptr += MQTTPacket_encode(*pptr, properties->length);
        len = rc = 1;
        for (i = 0; i < properties->count; ++i)
        {
            rc = MQTTProperty_write(pptr, &properties->array[i]);
            if (rc < 0)
                break;
            else
                len += rc;
        }
        if (rc >= 0)
            rc = len;
    }
    return rc;
}

MQTTProperties MQTTProperties_copy(const MQTTProperties* props)
{
    int i = 0;
    MQTTProperties result = MQTTProperties_initializer;
    for (i = 0; i < props->count; ++i)
    {
        int rc = 0;

        if ((rc = MQTTProperties_add(&result, &props->array[i])) != 0)
            Log(LOG_ERROR, -1, "Error from MQTTProperties add %d", rc);
    }
    return result;
}


int MQTTProperties_socketCompare(void* a, void* b)
{
    Clients* client = (Clients*)a;
    return client->net.socket == *(SOCKET*)b;
}


