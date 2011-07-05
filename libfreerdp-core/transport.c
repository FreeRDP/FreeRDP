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

boolean
transport_connect(rdpTransport * transport, const char * server, int port)
{
	return transport->tcp->connect(transport->tcp, server, port);
}

boolean
transport_disconnect(rdpTransport * transport)
{
	return transport->tcp->disconnect(transport->tcp);
}

boolean
transport_connect_rdp(rdpTransport * transport)
{
	transport->state = TRANSPORT_STATE_RDP;

	/* RDP encryption */

	return True;
}

boolean
transport_connect_tls(rdpTransport * transport)
{
	if (transport->tls == NULL)
		transport->tls = tls_new();

	transport->state = TRANSPORT_STATE_TLS;

	transport->tls->sockfd = transport->tcp->sockfd;

	if (tls_connect(transport->tls) != True)
		return False;

	return True;
}

boolean
transport_connect_nla(rdpTransport * transport)
{
	if (transport->tls == NULL)
		transport->tls = tls_new();

	transport->state = TRANSPORT_STATE_NLA;

	transport->tls->sockfd = transport->tcp->sockfd;

	if (tls_connect(transport->tls) != True)
		return False;

	/* Network Level Authentication */

	return True;
}

static int
transport_recv(rdpTransport * transport);

static int
transport_delay(rdpTransport * transport)
{
	transport_recv(transport);
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
	int bytes;
	uint8 * head;
	uint8 * tail;

	head = stream_get_head(stream);
	tail = stream_get_tail(stream);

	while (head < tail)
	{
		bytes = send(transport->tcp->sockfd, head, tail - head, MSG_NOSIGNAL);

		if (bytes < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				if (transport_delay(transport) != 0)
					return -1;

				continue;
			}

			perror("send");
			return -1;
		}
		head += bytes;
	}

	return 0;
}

int
transport_send(rdpTransport * transport, STREAM * stream)
{
	int r;

	if (transport->state == TRANSPORT_STATE_TLS)
		r = transport_send_tls(transport, stream);
	else
		r = transport_send_tcp(transport, stream);

	if (r == 0)
		r = transport_check_fds(transport);

	return r;
}

static int
transport_recv_tls(rdpTransport * transport)
{
	return 0;
}

static int
transport_recv_tcp(rdpTransport * transport)
{
	int bytes;

	stream_check_capacity(transport->recv_buffer, BUFFER_SIZE);

	bytes = recv(transport->tcp->sockfd, transport->recv_buffer->ptr, BUFFER_SIZE, 0);

	if (bytes == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;

		perror("recv");
		return -1;
	}

	stream_seek(transport->recv_buffer, bytes);
	
	return bytes;
}

static int
transport_recv(rdpTransport * transport)
{
	if (transport->state == TRANSPORT_STATE_TLS)
		return transport_recv_tls(transport);
	else
		return transport_recv_tcp(transport);
}

int
transport_check_fds(rdpTransport * transport)
{
	int pos;
	int bytes;
	uint16 length;
	STREAM * received;

	bytes = transport_recv(transport);

	if (bytes <= 0)
		return bytes;

	while ((pos = stream_get_pos(transport->recv_buffer)) > 0)
	{
		/* Ensure the TPKT header is available. */
		if (pos <= 4)
			return 0;

		stream_set_pos(transport->recv_buffer, 0);
		length = tpkt_read_header(transport->recv_buffer);

		if (length == 0)
		{
			printf("transport_check_fds: protocol error, not a TPKT header.\n");
			return -1;
		}

		if (pos < length)
		{
			stream_set_pos(transport->recv_buffer, pos);
			return 0; /* Packet is not yet completely received. */
		}

		/*
		 * A complete packet has been received. In case there are trailing data
		 * for the next packet, we copy it to the new receive buffer.
		 */
		received = transport->recv_buffer;
		transport->recv_buffer = stream_new(BUFFER_SIZE);

		if (pos > length)
		{
			stream_set_pos(received, length);
			stream_check_capacity(transport->recv_buffer, pos - length);
			stream_copy(transport->recv_buffer, received, pos - length);
		}

		stream_set_pos(received, 0);
		bytes = transport->recv_callback(transport, received, transport->recv_extra);
		stream_free(received);

		if (bytes < 0)
			return bytes;
	}

	return 0;
}

void
transport_init(rdpTransport * transport)
{
	transport->state = TRANSPORT_STATE_NEGO;
}

rdpTransport *
transport_new(void)
{
	rdpTransport * transport;

	transport = (rdpTransport *) xzalloc(sizeof(rdpTransport));

	if (transport != NULL)
	{
		transport->tcp = tcp_new();

		/* a small 0.1ms delay when transport is blocking. */
		transport->ts.tv_sec = 0;
		transport->ts.tv_nsec = 100000;

		/* receive buffer for non-blocking read. */
		transport->recv_buffer = stream_new(BUFFER_SIZE);
	}

	return transport;
}

void
transport_free(rdpTransport * transport)
{
	if (transport != NULL)
	{
		stream_free(transport->recv_buffer);
		tcp_free(transport->tcp);
		xfree(transport);
	}
}
