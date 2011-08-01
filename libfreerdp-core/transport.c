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
#include "credssp.h"
#include "transport.h"

#define BUFFER_SIZE 16384

STREAM* transport_recv_stream_init(rdpTransport* transport, int size)
{
	STREAM* s = transport->recv_stream;
	stream_check_size(s, size);
	stream_set_pos(s, 0);
	return s;
}

STREAM* transport_send_stream_init(rdpTransport* transport, int size)
{
	STREAM* s = transport->send_stream;
	stream_check_size(s, size);
	stream_set_pos(s, 0);
	return s;
}

boolean transport_connect(rdpTransport* transport, const char* hostname, uint16 port)
{
	return transport->tcp->connect(transport->tcp, hostname, port);
}

boolean transport_disconnect(rdpTransport* transport)
{
	return transport->tcp->disconnect(transport->tcp);
}

boolean transport_connect_rdp(rdpTransport* transport)
{
	transport->state = TRANSPORT_STATE_RDP;

	/* RDP encryption */

	return True;
}

boolean transport_connect_tls(rdpTransport* transport)
{
	if (transport->tls == NULL)
		transport->tls = tls_new();

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->state = TRANSPORT_STATE_TLS;
	transport->tls->sockfd = transport->tcp->sockfd;

	if (tls_connect(transport->tls) != True)
		return False;

	return True;
}

boolean transport_connect_nla(rdpTransport* transport)
{
	if (transport->tls == NULL)
		transport->tls = tls_new();

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->state = TRANSPORT_STATE_NLA;
	transport->tls->sockfd = transport->tcp->sockfd;

	if (tls_connect(transport->tls) != True)
		return False;

	/* Network Level Authentication */

	if (transport->credssp == NULL)
		transport->credssp = credssp_new(transport);

	if (credssp_authenticate(transport->credssp) < 0)
	{
		printf("Authentication failure, check credentials.\n"
			"If credentials are valid, the NTLMSSP implementation may be to blame.\n");

		credssp_free(transport->credssp);
		return False;
	}

	credssp_free(transport->credssp);

	return True;
}

int transport_read(rdpTransport* transport, STREAM* s)
{
	int status = -1;

	while (True)
	{
		if (transport->layer == TRANSPORT_LAYER_TLS)
			status = tls_read(transport->tls, s->data, s->size);
		else if (transport->layer == TRANSPORT_LAYER_TCP)
			status = tcp_read(transport->tcp, s->data, s->size);

		if (status == 0 && transport->blocking)
		{
			nanosleep(&transport->ts, NULL);
			continue;
		}

		break;
	}

	return status;
}

static int transport_read_nonblocking(rdpTransport* transport)
{
	int status;

	stream_check_size(transport->recv_buffer, 4096);
	status = transport_read(transport, transport->recv_buffer);

	if (status <= 0)
		return status;

	stream_seek(transport->recv_buffer, status);

	return status;
}

int transport_write(rdpTransport* transport, STREAM* s)
{
	int status = -1;
	int length;
	int sent = 0;

	length = stream_get_length(s);
	stream_set_pos(s, 0);
	while (sent < length)
	{
		if (transport->layer == TRANSPORT_LAYER_TLS)
			status = tls_write(transport->tls, stream_get_tail(s), length);
		else if (transport->layer == TRANSPORT_LAYER_TCP)
			status = tcp_write(transport->tcp, stream_get_tail(s), length);

		if (status < 0)
			break; /* error occurred */

		if (status == 0)
		{
			/* blocking while sending */
			nanosleep(&transport->ts, NULL);

			/* when sending is blocked in nonblocking mode, the receiving buffer should be checked */
			if (!transport->blocking)
				transport_read_nonblocking(transport);
		}

		sent += status;
		stream_seek(s, status);
	}

	if (!transport->blocking)
		transport_check_fds(transport);

	return status;
}

int transport_check_fds(rdpTransport* transport)
{
	int pos;
	int status;
	uint8 header;
	uint16 length;
	STREAM* received;

	status = transport_read_nonblocking(transport);
	if (status <= 0)
		return status;

	while ((pos = stream_get_pos(transport->recv_buffer)) > 0)
	{
		/* Ensure the TPKT or Fast Path header is available. */
		if (pos <= 4)
			return 0;

		stream_set_pos(transport->recv_buffer, 0);
		stream_peek_uint8(transport->recv_buffer, header);
		if (header == 0x03) /* TPKT */
			length = tpkt_read_header(transport->recv_buffer);
		else /* TODO: Fast Path */
			length = 0;

		if (length == 0)
		{
			printf("transport_check_fds: protocol error, not a TPKT header (%d).\n", header);
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
			stream_check_size(transport->recv_buffer, pos - length);
			stream_copy(transport->recv_buffer, received, pos - length);
		}

		stream_set_pos(received, 0);
		status = transport->recv_callback(transport, received, transport->recv_extra);
		stream_free(received);

		if (status < 0)
			return status;
	}

	return 0;
}

void transport_init(rdpTransport* transport)
{
	transport->layer = TRANSPORT_LAYER_TCP;
	transport->state = TRANSPORT_STATE_NEGO;
}

boolean transport_set_blocking_mode(rdpTransport* transport, boolean blocking)
{
	transport->blocking = blocking;
	return transport->tcp->set_blocking_mode(transport->tcp, blocking);
}

rdpTransport* transport_new(rdpSettings* settings)
{
	rdpTransport* transport;

	transport = (rdpTransport*) xzalloc(sizeof(rdpTransport));

	if (transport != NULL)
	{
		transport->tcp = tcp_new(settings);
		transport->settings = settings;

		/* a small 0.1ms delay when transport is blocking. */
		transport->ts.tv_sec = 0;
		transport->ts.tv_nsec = 100000;

		/* receive buffer for non-blocking read. */
		transport->recv_buffer = stream_new(BUFFER_SIZE);

		/* buffers for blocking read/write */
		transport->recv_stream = stream_new(BUFFER_SIZE);
		transport->send_stream = stream_new(BUFFER_SIZE);

		transport->blocking = True;
	}

	return transport;
}

void transport_free(rdpTransport* transport)
{
	if (transport != NULL)
	{
		stream_free(transport->recv_buffer);
		stream_free(transport->recv_stream);
		stream_free(transport->send_stream);
		tcp_free(transport->tcp);
		xfree(transport);
	}
}
