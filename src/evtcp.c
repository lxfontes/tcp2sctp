#include "evtcp.h"
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static void set_nonblock (int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);

	//already there
	if (flags & O_NONBLOCK) return;

	int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	assert(0 <= r && "Setting socket non-block failed!");
}

void evtcp_nonblock(evtcp_t *evtcp)
{
	set_nonblock(EVTCP_SOCKET(evtcp));
}


void evtcp_close(evtcp_t *evtcp)
{
	if (evtcp->socket <= 0) {
		return;
	}
	evtcp_stopread(evtcp);
	evtcp_stopwrite(evtcp);
	close(EVTCP_SOCKET(evtcp));
	evtcp->socket = 0;
	if (evtcp->sops.close != NULL)
		evtcp->sops.close(evtcp);
}

int evtcp_create(evtcp_t *evtcp, struct evtcpop *sops)
{
	if (evtcp == NULL) return -1;
	memcpy(&evtcp->sops, sops, sizeof(*sops));
	evtcp->socket = 0;
	evtcp->readw.data = evtcp;
	evtcp->writew.data = evtcp;
	return 0;
}

void evtcp_accept(struct ev_loop *loop, ev_io *w, int revents)
{
	evtcp_t *evtcp = (evtcp_t *)w->data;

	if (revents & EV_ERROR) {
		if (evtcp->sops.error != NULL)
			evtcp->sops.error(evtcp);
	} else {
		if (evtcp->sops.connect != NULL)
			evtcp->sops.connect(evtcp);
	}
}

void evtcp_read(struct ev_loop *loop, ev_io *w, int revents)
{
	evtcp_t *evtcp = (evtcp_t *)w->data;


	if (revents & EV_ERROR) {
		if (evtcp->sops.error != NULL)
			evtcp->sops.error(evtcp);
	} else {
		if (evtcp->sops.read != NULL)
			evtcp->sops.read(evtcp);
	}
}

void evtcp_write(struct ev_loop *loop, ev_io *w, int revents)
{
	evtcp_t *evtcp = (evtcp_t *)w->data;


	if (revents & EV_ERROR) {
		if (evtcp->sops.error != NULL)
			evtcp->sops.error(evtcp);
	} else {
		if (evtcp->sops.write != NULL)
			evtcp->sops.write(evtcp);
	}
}



void evtcp_wantread(evtcp_t *evtcp)
{
	if (! ev_is_active(&evtcp->readw)) {
		printf("want read on %d\n", evtcp->socket);
		ev_io_init(&evtcp->readw, evtcp_read, evtcp->socket, EV_READ);
		ev_io_start(evtcp->sops.loop, &evtcp->readw);
	}
}

void evtcp_stopread(evtcp_t *evtcp)
{

	if (ev_is_active(&evtcp->readw)) {
		printf("stop read on %d\n", evtcp->socket);
		ev_io_stop(evtcp->sops.loop, &evtcp->readw);
	}
}

void evtcp_wantwrite(evtcp_t *evtcp)
{
	if (! ev_is_active(&evtcp->writew)) {
		ev_io_init(&evtcp->writew, evtcp_write, evtcp->socket, EV_WRITE);
		ev_io_start(evtcp->sops.loop, &evtcp->writew);
	}
}

void evtcp_stopwrite(evtcp_t *evtcp)
{
	if (ev_is_active(&evtcp->writew)) {
		ev_io_stop(evtcp->sops.loop, &evtcp->writew);
	}
}


int evtcp_listen(evtcp_t *evtcp, const char *dest, short port)
{
	if (evtcp->socket > 0) {
		return -1;
	}

	evtcp->socket = socket(PF_INET, SOCK_STREAM, 0);

	if (evtcp->socket < 0) return -1;

	evtcp->sin.sin_family = AF_INET;
	evtcp->sin.sin_port = htons(port);
	evtcp->sin.sin_addr.s_addr = INADDR_ANY;

	int on = 1;

	setsockopt(evtcp->socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
	setsockopt(evtcp->socket, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on) );

	socklen_t addr_len = sizeof(evtcp->sin);
	if (bind(evtcp->socket, (struct sockaddr *) &evtcp->sin, addr_len) != 0) {
		close(evtcp->socket);
		return -2;
	}

	if (listen(evtcp->socket, 200) < 0) {
		close(evtcp->socket);
		return -3;
	}

	set_nonblock(evtcp->socket);
	//register with io loop
	ev_io_init(&evtcp->readw, evtcp_accept, evtcp->socket, EV_READ);
	ev_io_start(evtcp->sops.loop, &evtcp->readw);
	return 0;
}
