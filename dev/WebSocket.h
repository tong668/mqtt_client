
#if !defined(WEBSOCKET_H)
#define WEBSOCKET_H

#include "MQTTPacket.h"

/**
 * WebSocket op codes
 * @{
 */
#define WebSocket_OP_CONTINUE 0x0 /* 0000 - continue frame */
#define WebSocket_OP_TEXT     0x1 /* 0001 - text frame */
#define WebSocket_OP_BINARY   0x2 /* 0010 - binary frame */
#define WebSocket_OP_CLOSE    0x8 /* 1000 - close frame */
#define WebSocket_OP_PING     0x9 /* 1001 - ping frame */
#define WebSocket_OP_PONG     0xA /* 1010 - pong frame */
/** @} */

/**
 * Various close status codes
 * @{
 */
#define WebSocket_CLOSE_NORMAL          1000
#define WebSocket_CLOSE_GOING_AWAY      1001
#define WebSocket_CLOSE_TLS_FAIL        1015 /* reserved: not be used */
/** @} */

/* closes a websocket connection */
void WebSocket_close(networkHandles *net, int status_code, const char *reason);

/* sends upgrade request */
int WebSocket_connect(networkHandles *net, int ssl, const char *uri);

/* obtain data from network socket */
int WebSocket_getch(networkHandles *net, char* c);
char *WebSocket_getdata(networkHandles *net, size_t bytes, size_t* actual_len);
size_t WebSocket_framePos();
void WebSocket_framePosSeekTo(size_t);

/* send data out, in websocket format only if required */
int WebSocket_putdatas(networkHandles* net, char** buf0, size_t* buf0len, PacketBuffers* bufs);

/* releases any resources used by the websocket system */
void WebSocket_terminate(void);

/* handles websocket upgrade request */
int WebSocket_upgrade(networkHandles *net);

#endif /* WEBSOCKET_H */
