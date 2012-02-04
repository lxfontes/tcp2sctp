#include <ev.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/sctp.h>
#include <sys/uio.h>
#include <unistd.h>
#include "evtcp.h"
#include "evsctp.h"
#include "diam_wire.h"


#define CONNECTION_DOWN 0
#define CONNECTION_UP 1
#define CONNECTION_DESTROYED 2

#define BUFFER_SIZE 8192
typedef struct buffer {
	char b[BUFFER_SIZE];
	size_t pos;
} buffer_t;

typedef struct proxypair {
	/* TCP fields */
	evtcp_t evtcp;
	int tcpup;
	buffer_t tcpin;
	buffer_t tcpout;

	/* SCTP fields */
	evsctp_t evsctp;
	int sctpup;

} proxypair_t;


void tcp_read(evtcp_t *evtcp);
void tcp_write(evtcp_t *evtcp);
void tcp_accept(evtcp_t *evtcp);
void tcp_error(evtcp_t *evtcp);
void tcp_close(evtcp_t *evtcp);

void sctp_read(evsctp_t *evsctp);
void sctp_write(evsctp_t *evsctp);
void sctp_connect(evsctp_t *evsctp);
void sctp_error(evsctp_t *evsctp);
void sctp_close(evsctp_t *evsctp);


void buffer_create(buffer_t *b)
{
	b->pos = 0;
}

size_t buffer_remaining(buffer_t *b)
{
	return BUFFER_SIZE - b->pos;
}

int buffer_add(buffer_t *b, char *data, size_t len)
{
	if (buffer_remaining(b) < len)
		return -1;

	memcpy(b->b + b->pos, data, len);
	b->pos += len;
	return 0;
}

size_t buffer_position(buffer_t *b)
{
	return b->pos;
}

char *buffer_current(buffer_t *b)
{
	return b->b + b->pos;
}

char *buffer_begin(buffer_t *b) {
	return b->b;
}

void buffer_increment(buffer_t *b, size_t len)
{
	b->pos += len;
}

/* return new buffer position */
int buffer_consume(buffer_t *b, size_t len)
{

	printf("Decrementing %ld from %ld \n",len, b->pos);
	if ( len == b->pos ) {
		b->pos = 0;
		return b->pos;
	}

	memmove(b->b, b->b + len, b->pos - len);
	b->pos -= len;

	printf("to %ld\n",b->pos);
	return b->pos;
}


int proxypair_create(proxypair_t *ret, struct ev_loop *loop)
{

	struct evtcpop tcpops;
	struct evsctpop sctpops;

	tcpops.connect = tcp_accept;
	tcpops.read = tcp_read;
	tcpops.write = tcp_write;
	tcpops.error = tcp_error;
	tcpops.close = tcp_close;
	tcpops.loop = loop;

	sctpops.connect = sctp_connect;
	sctpops.read = sctp_read;
	sctpops.write = sctp_write;
	sctpops.error = sctp_error;
	sctpops.close = sctp_close;
	sctpops.loop = loop;

	evtcp_create(&ret->evtcp, &tcpops);
	evsctp_create(&ret->evsctp, &sctpops);

	buffer_create(&ret->tcpin);
	buffer_create(&ret->tcpout);

	ret->tcpup = CONNECTION_DOWN;
	ret->sctpup = CONNECTION_DOWN;

	ret->evtcp.data = ret;
	ret->evsctp.data = ret;

	return 0;
}

void tcp_close(evtcp_t *evtcp)
{
	proxypair_t *pair = (proxypair_t *) evtcp->data;

	if (pair->sctpup == CONNECTION_DESTROYED) {
		free(pair);
		printf("Free @ TCP\n");
	} else {
		pair->tcpup = CONNECTION_DESTROYED;
	}

	printf("tcp closed\n");
}

void tcp_read(evtcp_t *evtcp)
{
	proxypair_t *pair = (proxypair_t *) evtcp->data;
	size_t mayread = buffer_remaining(&pair->tcpin);
	size_t readn;

	if (mayread == 0) {
		printf("buffer is full\n");
		evtcp_stopread(&pair->evtcp);
		return;
	}

	printf("Reading %ld\n", mayread);
	readn = recv(EVTCP_SOCKET(evtcp), buffer_current(&pair->tcpin), mayread, 0);

	if (readn == 0) {
		if (errno != EAGAIN) {
			evtcp_close(evtcp);
			evsctp_close(&pair->evsctp);
		}
		return;
	}

	buffer_increment(&pair->tcpin, readn);
	printf("Read %ld\n", readn);

	evsctp_wantwrite(&pair->evsctp);
}

void tcp_write(evtcp_t *evtcp)
{
	proxypair_t *pair = (proxypair_t *) evtcp->data;
	size_t nwrite;
	size_t bpos = buffer_position(&pair->tcpout);
	char *bwrite = buffer_begin(&pair->tcpout);

	printf("tcp writing %ld\n", bpos);
	nwrite = write(EVSCTP_SOCKET((&pair->evtcp)), bwrite, bpos);

	if ( nwrite > 0) {
		buffer_consume(&pair->tcpout, nwrite);
		if (nwrite == bpos) {
			evtcp_stopwrite(&pair->evtcp);
		}

		evsctp_wantread(&pair->evsctp);
	}
}

void tcp_error(evtcp_t *evtcp)
{

	proxypair_t *pair = (proxypair_t *) evtcp->data;
	evtcp_close(evtcp);
	evsctp_close(&pair->evsctp);
}

void tcp_accept(evtcp_t *server)
{
	proxypair_t *pair = malloc(sizeof(*pair));

	assert(pair != NULL);

	proxypair_create(pair, server->sops.loop);

	socklen_t addr_len = sizeof(&EVTCP_SIN((&pair->evtcp)));

	EVTCP_SOCKET((&pair->evtcp)) = accept(EVTCP_SOCKET(server), (struct sockaddr *) &EVTCP_SIN((&pair->evtcp)), &addr_len);

	if (EVTCP_SOCKET((&pair->evtcp)) < 0) {
		free(pair);
		printf("error accepting\n");
		return;
	}

	evtcp_nonblock(&pair->evtcp);

	printf("TCP accepted\n");
	pair->tcpup = CONNECTION_UP;

	/* connect sctp side */
	if (evsctp_connect(&pair->evsctp, "127.0.0.1", 3869)) {
		printf("Error connecting SCTP\n");
		return;
	}
}


void sctp_connect(evsctp_t *evsctp)
{
	proxypair_t *pair = (proxypair_t *) evsctp->data;
	evtcp_wantread(&pair->evtcp);
	evsctp_wantread(&pair->evsctp);
	printf("SCTP connected\n");
	pair->sctpup = CONNECTION_UP;
}

void sctp_read(evsctp_t *evsctp)
{
	proxypair_t *pair = (proxypair_t *) evsctp->data;
	size_t mayread = buffer_remaining(&pair->tcpout);
	size_t readn;

	if (mayread == 0) {
		printf("buffer is full\n");
		evsctp_stopread(&pair->evsctp);
		return;
	}

	printf("Reading %ld\n", mayread);
	readn = recv(EVTCP_SOCKET(evsctp), buffer_current(&pair->tcpout), mayread, 0);

	if (readn == 0) {
		if (errno != EAGAIN) {
			evtcp_close(&pair->evtcp);
			evsctp_close(&pair->evsctp);
		}
		return;
	}

	buffer_increment(&pair->tcpout, readn);
	printf("Read %ld\n", readn);

	evtcp_wantwrite(&pair->evtcp);

}

void sctp_write(evsctp_t *evsctp)
{
	proxypair_t *pair = (proxypair_t *) evsctp->data;
	size_t nwrite;
	size_t bpos = buffer_position(&pair->tcpin);
	char *bwrite = buffer_begin(&pair->tcpin);

	//offset a diameter message

	printf("sctp writing %ld\n", bpos);
	nwrite = write(EVSCTP_SOCKET((&pair->evsctp)), bwrite, bpos);

	if ( nwrite > 0) {
		buffer_consume(&pair->tcpin, nwrite);
		if (nwrite == bpos) {
			evsctp_stopwrite(&pair->evsctp);
		}

		evtcp_wantread(&pair->evtcp);
	}
}

void sctp_error(evsctp_t *evsctp)
{
	proxypair_t *pair = (proxypair_t *) evsctp->data;
	evsctp_close(&pair->evsctp);
	evtcp_close(&pair->evtcp);
}



void sctp_close(evsctp_t *evsctp)
{
	proxypair_t *pair = (proxypair_t *) evsctp->data;

	if (pair->tcpup == CONNECTION_DESTROYED) {
		free(pair);
		printf("Free @ SCTP\n");
	} else {
		pair->sctpup = CONNECTION_DESTROYED;
	}
	printf("sctp closed\n");
}



int main(int argc, char **argv)
{
	proxypair_t pair;
	struct ev_loop *loop = ev_default_loop(0);

	if (proxypair_create(&pair, loop))
		return 1;

	if (evtcp_listen(&pair.evtcp, "0.0.0.0", 3868))
		return 2;

	ev_loop(loop, 0);

	return 0;
}

