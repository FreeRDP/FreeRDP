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

boolean transport_connect(rdpTransport* transport, const uint8* hostname, uint16 port)
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

int transport_delay(rdpTransport* transport, STREAM* s)
{
	transport_read(transport, s);
	nanosleep(&transport->ts, NULL);
	return 0;
}

int transport_read(rdpTransport* transport, STREAM* s)
{
	int status = -1;

	if (transport->layer == TRANSPORT_LAYER_TLS)
		status = tls_read(transport->tls, s->data, s->size);
	else if (transport->layer == TRANSPORT_LAYER_TCP)
		status = tcp_read(transport->tcp, s->data, s->size);

	return status;
}

int transport_write(rdpTransport* transport, STREAM* s)
{
	int status = -1;

	if (transport->layer == TRANSPORT_LAYER_TLS)
		status = tls_write(transport->tls, s->data, stream_get_length(s));
	else if (transport->layer == TRANSPORT_LAYER_TCP)
		status = tcp_write(transport->tcp, s->data, stream_get_length(s));

	return status;
}

int transport_check_fds(rdpTransport* transport)
{
	int pos;
	int bytes;
	uint16 length;
	STREAM* received;

	bytes = transport_read(transport, transport->recv_buffer);

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
			stream_check_size(transport->recv_buffer, pos - length);
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

void transport_init(rdpTransport* transport)
{
	transport->layer = TRANSPORT_LAYER_TCP;
	transport->state = TRANSPORT_STATE_NEGO;
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
