

#if !defined(PROXY_H)
#define PROXY_H

#include "Clients.h"
/* Notify the IP address and port of the endpoint to proxy, and wait connection to endpoint */
int Proxy_connect(networkHandles *net, int ssl, const char *hostname );

#endif /* PROXY_H */
