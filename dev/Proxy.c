
#include <stdio.h>
#include <string.h>
// for timeout process in Proxy_connect()
#include <time.h>

#include <unistd.h>

#include "Log.h"
#include "MQTTProtocolOut.h"
#include "Socket.h"

/**
 * Notify the IP address and port of the endpoint to proxy, and wait connection to endpoint.
 *
 * @param[in]  net               network connection to proxy.
 * @param[in]  ssl               enable ssl.
 * @param[in]  hostname          hostname of endpoint.
 *
 * @retval SOCKET_ERROR          failed to network connection
 * @retval 0                     connection to endpoint
 *
 */
int Proxy_connect(networkHandles *net, int ssl, const char *hostname) {
    int port, i, rc = 0, buf_len = 0;
    char *buf = NULL;
    size_t hostname_len, actual_len = 0;
    time_t current, timeout;
    PacketBuffers nulbufs = {0, NULL, NULL, NULL, {0, 0, 0, 0}};

    FUNC_ENTRY;
    hostname_len = MQTTProtocol_addressPort(hostname, &port, NULL, PROXY_DEFAULT_PORT);
    for (i = 0; i < 2; ++i) {
        if (net->http_proxy_auth) {
            buf_len = snprintf(buf, (size_t) buf_len, "CONNECT %.*s:%d HTTP/1.1\r\n"
                                                      "Host: %.*s\r\n"
                                                      "Proxy-authorization: Basic %s\r\n"
                                                      "\r\n",
                               (int) hostname_len, hostname, port,
                               (int) hostname_len, hostname, net->http_proxy_auth);
        } else {
            buf_len = snprintf(buf, (size_t) buf_len, "CONNECT %.*s:%d HTTP/1.1\r\n"
                                                      "Host: %.*s\r\n"
                                                      "\r\n",
                               (int) hostname_len, hostname, port,
                               (int) hostname_len, hostname);
        }
        if (i == 0 && buf_len > 0) {
            ++buf_len;
            if ((buf = malloc(buf_len)) == NULL) {
                rc = PAHO_MEMORY_ERROR;
                goto exit;
            }

        }
    }
    Log(TRACE_PROTOCOL, -1, "Proxy_connect: \"%s\"", buf);

    Socket_putdatas(net->socket, buf, buf_len, nulbufs);
    free(buf);
    buf = NULL;

    time(&timeout);
    timeout += (time_t) 10;

    while (1) {
        buf = Socket_getdata(net->socket, (size_t) 12, &actual_len, &rc);
        if (actual_len) {
            if ((strncmp(buf, "HTTP/1.0 200", 12) != 0) && (strncmp(buf, "HTTP/1.1 200", 12) != 0))
                rc = SOCKET_ERROR;
            break;
        } else {
            time(&current);
            if (current > timeout) {
                rc = SOCKET_ERROR;
                break;
            }

            usleep(250000);
        }
    }
    /* flush the SocketBuffer */
    actual_len = 1;
    while (actual_len) {
        int rc1;

        buf = Socket_getdata(net->socket, (size_t) 1, &actual_len, &rc1);
    }
    exit:
    return rc;
}
