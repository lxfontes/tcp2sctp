#ifndef __EVTCP_H__
#define __EVTCP_H__

#include <ev.h>
#include <netinet/in.h>

struct evtcp;
typedef void (*evtcpopcb)(struct evtcp *);

struct evtcpop {
	struct ev_loop *loop;
	evtcpopcb read;
	evtcpopcb write;
	evtcpopcb error;
	evtcpopcb connect;
	evtcpopcb close;
};

typedef struct evtcp {
	void *data;
	int socket;
	ev_io readw;
	ev_io writew;
	struct sockaddr_in sin;
	struct evtcpop sops;
} evtcp_t;

#define EVTCP_SIN(x) x->sin
#define EVTCP_SOCKET(x) x->socket

int evtcp_create(evtcp_t *evtcp, struct evtcpop *ops);
int evtcp_listen(evtcp_t *evtcp, const char *dest, short port);
void evtcp_wantread(evtcp_t *evtcp);
void evtcp_wantwrite(evtcp_t *evtcp);
void evtcp_stopread(evtcp_t *evtcp);
void evtcp_stopwrite(evtcp_t *evtcp);
void evtcp_close(evtcp_t *evtcp);
void evtcp_nonblock(evtcp_t *evtcp);

#endif

