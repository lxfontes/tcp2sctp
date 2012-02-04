#include "evsctp.h"
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

static void set_nonblock (int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	assert(0 <= r && "Setting socket non-block failed!");
}

void evsctp_nonblock(evsctp_t *evsctp)
{
	set_nonblock(EVSCTP_SOCKET(evsctp));
}


void evsctp_close(evsctp_t *evsctp)
{
	if (evsctp->socket <= 0) {
		return;
	}
	evsctp_stopread(evsctp);
	evsctp_stopwrite(evsctp);
	close(EVSCTP_SOCKET(evsctp));
	evsctp->socket = 0;
	if (evsctp->sops.close != NULL)
		evsctp->sops.close(evsctp);
}

int evsctp_create(evsctp_t *evsctp, struct evsctpop *sops)
{
	if (evsctp == NULL) return -1;
	memcpy(&evsctp->sops, sops, sizeof(*sops));
	evsctp->socket = 0;
	evsctp->readw.data = evsctp;
	evsctp->writew.data = evsctp;
	return 0;
}

void evsctp_connected(struct ev_loop *loop, ev_io *w, int revents)
{
	evsctp_t *evsctp = (evsctp_t *)w->data;

	if (revents & EV_ERROR) {
		if (evsctp->sops.error != NULL)
			evsctp->sops.error(evsctp);
	} else {
		if (evsctp->sops.connect != NULL)
			evsctp->sops.connect(evsctp);
	}
}

void evsctp_read(struct ev_loop *loop, ev_io *w, int revents)
{
	evsctp_t *evsctp = (evsctp_t *)w->data;


	if (revents & EV_ERROR) {
		if (evsctp->sops.error != NULL)
			evsctp->sops.error(evsctp);
	} else {
		if (evsctp->sops.read != NULL)
			evsctp->sops.read(evsctp);
	}
}

void evsctp_write(struct ev_loop *loop, ev_io *w, int revents)
{
	evsctp_t *evsctp = (evsctp_t *)w->data;


	if (revents & EV_ERROR) {
		if (evsctp->sops.error != NULL)
			evsctp->sops.error(evsctp);
	} else {
		if (evsctp->sops.write != NULL)
			evsctp->sops.write(evsctp);
	}
}



void evsctp_wantread(evsctp_t *evsctp)
{
	if (! ev_is_active(&evsctp->readw)) {
		ev_io_init(&evsctp->readw, evsctp_read, evsctp->socket, EV_READ);
		ev_io_start(evsctp->sops.loop, &evsctp->readw);
	}
}

void evsctp_stopread(evsctp_t *evsctp)
{

	if (ev_is_active(&evsctp->readw)) {
		ev_io_stop(evsctp->sops.loop, &evsctp->readw);
	}
}

void evsctp_wantwrite(evsctp_t *evsctp)
{
	if (! ev_is_active(&evsctp->writew)) {
		ev_io_init(&evsctp->writew, evsctp_write, evsctp->socket, EV_WRITE);
		ev_io_start(evsctp->sops.loop, &evsctp->writew);
	}
}

void evsctp_stopwrite(evsctp_t *evsctp)
{
	if (ev_is_active(&evsctp->writew)) {
		ev_io_stop(evsctp->sops.loop, &evsctp->writew);
	}
}


int evsctp_connect(evsctp_t *evsctp, const char *dest, short port)
{
	int r;

	if (evsctp->socket > 0) {
		return -1;
	}

	evsctp->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP);

	if (evsctp->socket < 0) return -1;

	evsctp->sin.sin_family = AF_INET;
	evsctp->sin.sin_port = htons(port);
	evsctp->sin.sin_addr.s_addr = inet_addr(dest);


	struct sctp_initmsg initmsg = {0};

	// 1 to 1 mapping between tcp and sctp connections
	initmsg.sinit_num_ostreams = 1;

	r = setsockopt( evsctp->socket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
				sizeof(initmsg));

	socklen_t addr_len = sizeof(evsctp->sin);

	if (r == 0) {
		r = connect(evsctp->socket, (struct sockaddr *) &evsctp->sin, addr_len);
	}

	if (r) {
		evsctp->sops.error(evsctp);
	} else {
		evsctp->sops.connect(evsctp);
	}

	return r;
}
