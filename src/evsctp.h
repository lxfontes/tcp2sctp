#ifndef __evsctp_H__
#define __evsctp_H__

#include <ev.h>
#include <netinet/in.h>

struct evsctp;
typedef void (*evsctpopcb)(struct evsctp *);

struct evsctpop {
	struct ev_loop *loop;
	evsctpopcb read;
	evsctpopcb write;
	evsctpopcb error;
	evsctpopcb connect;
	evsctpopcb close;
};

typedef struct evsctp {
	void *data;
	int socket;
	ev_io readw;
	ev_io writew;
	struct sockaddr_in sin;
	struct evsctpop sops;
} evsctp_t;

#define EVSCTP_SIN(x) x->sin
#define EVSCTP_SOCKET(x) x->socket

int evsctp_create(evsctp_t *evsctp, struct evsctpop *ops);
int evsctp_connect(evsctp_t *evsctp, const char *dest, short port);
void evsctp_wantread(evsctp_t *evsctp);
void evsctp_wantwrite(evsctp_t *evsctp);
void evsctp_stopread(evsctp_t *evsctp);
void evsctp_stopwrite(evsctp_t *evsctp);
void evsctp_close(evsctp_t *evsctp);
void evsctp_nonblock(evsctp_t *evsctp);

#endif

