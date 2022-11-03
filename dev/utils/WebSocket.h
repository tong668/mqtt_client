
#if !defined(WEBSOCKET_H)
#define WEBSOCKET_H

//#include "MQTTPacket.h"
#include "Socket.h"

/* closes a websocket connection */
void WebSocket_close(networkHandles *net, int status_code, const char *reason);

/* sends upgrade request */
int WebSocket_connect(networkHandles *net, int ssl, const char *uri);

/* obtain data from network socket */
int WebSocket_getch(networkHandles *net, char *c);

char *WebSocket_getdata(networkHandles *net, size_t bytes, size_t *actual_len);

size_t WebSocket_framePos();

void WebSocket_framePosSeekTo(size_t);

/* send data out, in websocket format only if required */
int WebSocket_putdatas(networkHandles *net, char **buf0, size_t *buf0len, PacketBuffers *bufs);

/* releases any resources used by the websocket system */
void WebSocket_terminate(void);

/* handles websocket upgrade request */
int WebSocket_upgrade(networkHandles *net);

#endif /* WEBSOCKET_H */
