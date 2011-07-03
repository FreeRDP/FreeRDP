/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Network Transport Layer
 *
 * Copyright 2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#include "tpkt.h"
#include "transport.h"

#define BUFFER_SIZE 16384

rdpTransport *
transport_new(void)
{
	rdpTransport * transport;

	transport = (rdpTransport *) xmalloc(sizeof(rdpTransport));
	memset(transport, 0, sizeof(rdpTransport));

	transport->sockfd = -1;

	/* a small 0.1ms delay when transport is blocking. */
	transport->ts.tv_sec = 0;
	transport->ts.tv_nsec = 100000;

	/* receive buffer for non-blocking read. */
	transport->recv_buffer = stream_new(BUFFER_SIZE);

	return transport;
}

void
transport_free(rdpTransport * transport)
{
	stream_free(transport->recv_buffer);
	xfree(transport);
}

static int
transport_connect_sockfd(rdpTransport * transport, const char * server, int port)
{
	struct addrinfo hints = { 0 };
	struct addrinfo * res, * ai;
	int r;
	char servname[10];
	int sockfd = -1;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(servname, sizeof(servname), "%d", port);
	r = getaddrinfo(server, servname, &hints, &res);
	if (r != 0)
	{
		printf("transport_connect: getaddrinfo (%s)\n", gai_strerror(r));
		return -1;
	}

	for (ai = res; ai; ai = ai->ai_next)
	{
		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sockfd < 0)
			continue;

		r = connect(sockfd, ai->ai_addr, ai->ai_addrlen);
		if (r == 0)
		{
			printf("connected to %s:%s\n", server, servname);
			break;
		}

		sockfd = -1;
	}
	freeaddrinfo(res);

	if (sockfd == -1)
	{
		printf("unable to connect to %s:%s\n", server, servname);
		return -1;
	}

	transport->sockfd = sockfd;

	return 0;
}

static int
transport_configure_sockfd(rdpTransport * transport)
{
	int flags;

	flags = fcntl(transport->sockfd, F_GETFL);
	if (flags == -1)
	{
		printf("transport_configure_sockfd: fcntl failed.\n");
		return -1;
	}
	fcntl(transport->sockfd, F_SETFL, flags | O_NONBLOCK);

	return 0;
}

int
transport_connect(rdpTransport * transport, const char * server, int port)
{
	int r;

	r = transport_connect_sockfd(transport, server, port);
	if (r != 0)
		return r;

	r = transport_configure_sockfd(transport);
	if (r != 0)
		return r;

	return 0;
}

int
transport_disconnect(rdpTransport * transport)
{
	if (transport->sockfd != -1)
	{
		close(transport->sockfd);
		transport->sockfd = -1;
	}
	return 0;
}

int
transport_start_tls(rdpTransport * transport)
{
	return 0;
}

static int
transport_delay(rdpTransport * transport)
{
	nanosleep(&transport->ts, NULL);
	return 0;
}

static int
transport_send_tls(rdpTransport * transport, STREAM * stream)
{
	return 0;
}

static int
transport_send_tcp(rdpTransport * transport, STREAM * stream)
{
	uint8 * head;
	uint8 * tail;
	int r;

	head = stream_get_head(stream);
	tail = stream_get_tail(stream);
	while (head < tail)
	{
		r = send(transport->sockfd, head, tail - head, MSG_NOSIGNAL);
		if (r < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				if (transport_delay(transport) != 0)
					return -1;
				continue;
			}
			printf("transport_send_tcp: send (%d)\n", errno);
			return -1;
		}
		head += r;
	}
	return 0;
}

int
transport_send(rdpTransport * transport, STREAM * stream)
{
	if (transport->tls)
		return transport_send_tls(transport, stream);
	else
		return transport_send_tcp(transport, stream);
}

static int
transport_recv_tls(rdpTransport * transport)
{
	return 0;
}

static int
transport_recv_tcp(rdpTransport * transport)
{
	int r;

	stream_check_capacity(transport->recv_buffer, BUFFER_SIZE);

	r = recv(transport->sockfd, transport->recv_buffer->ptr, BUFFER_SIZE, 0);
	if (r == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
		printf("transport_recv_tcp: recv failed (%d).\n", errno);
		return -1;
	}
	stream_seek(transport->recv_buffer, r);
	
	return r;
}

int
transport_check_fds(rdpTransport * transport)
{
	int r;
	int pos;
	uint16 len;
	STREAM * received;

	if (transport->tls)
		r = transport_recv_tls(transport);
	else
		r = transport_recv_tcp(transport);

	if (r <= 0)
		return r;

	pos = stream_get_pos(transport->recv_buffer);
	/* Ensure the TPKT header is available. */
	if (pos <= 4)
		return 0;

	stream_set_pos(transport->recv_buffer, 0);
	len = tpkt_read_header(transport->recv_buffer);
	if (len == 0)
	{
		printf("transport_check_fds: protocol error, not a TPKT header.\n");
		return -1;
	}
	if (pos < len)
		return 0; /* Packet is not yet completely received. */

	/* A complete packet has been received. In case there are trailing data
	 * for the next packet, we copy it to the new receive buffer.
	 */
	received = transport->recv_buffer;
	transport->recv_buffer = stream_new(BUFFER_SIZE);
	if (pos > len)
	{
		stream_set_pos(received, len);
		stream_check_capacity(transport->recv_buffer, pos - len);
		stream_copy(transport->recv_buffer, received, pos - len);
	}

	stream_set_pos(received, 0);
	r = transport->recv_callback(received, transport->recv_callback_data);
	stream_free(received);

	return r;
}
