/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>

#include <freerdp/error.h>
#include <freerdp/utils/tcp.h>
#include <freerdp/utils/stream.h>

#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif

#include "tpkt.h"
#include "fastpath.h"
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

void transport_attach(rdpTransport* transport, int sockfd)
{
	transport->TcpIn->sockfd = sockfd;

	transport->SplitInputOutput = FALSE;
	transport->TcpOut = transport->TcpIn;
}

BOOL transport_disconnect(rdpTransport* transport)
{
	BOOL status = TRUE;

	if (transport->layer == TRANSPORT_LAYER_TLS)
		status &= tls_disconnect(transport->TlsIn);

	if (transport->layer == TRANSPORT_LAYER_TSG)
	{
		tsg_disconnect(transport->tsg);
	}
	else
	{
		status &= tcp_disconnect(transport->TcpIn);
	}

	return status;
}

BOOL transport_connect_rdp(rdpTransport* transport)
{
	/* RDP encryption */

	return TRUE;
}

BOOL transport_connect_tls(rdpTransport* transport)
{
	if (transport->layer == TRANSPORT_LAYER_TSG)
		return TRUE;

	if (transport->TlsIn == NULL)
		transport->TlsIn = tls_new(transport->settings);

	if (transport->TlsOut == NULL)
		transport->TlsOut = transport->TlsIn;

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->TlsIn->sockfd = transport->TcpIn->sockfd;

	if (tls_connect(transport->TlsIn) != TRUE)
	{
		if (!connectErrorCode)
			connectErrorCode = TLSCONNECTERROR;

		tls_free(transport->TlsIn);
		if (transport->TlsIn == transport->TlsOut)
			transport->TlsIn = transport->TlsOut = NULL;
		else
			transport->TlsIn = NULL;

		return FALSE;
	}

	return TRUE;
}

BOOL transport_connect_nla(rdpTransport* transport)
{
	freerdp* instance;
	rdpSettings* settings;

	if (transport->layer == TRANSPORT_LAYER_TSG)
		return TRUE;

	if (!transport_connect_tls(transport))
		return FALSE;

	/* Network Level Authentication */

	if (transport->settings->Authentication != TRUE)
		return TRUE;

	settings = transport->settings;
	instance = (freerdp*) settings->instance;

	if (transport->credssp == NULL)
		transport->credssp = credssp_new(instance, transport, settings);

	if (credssp_authenticate(transport->credssp) < 0)
	{
		if (!connectErrorCode)                    
			connectErrorCode = AUTHENTICATIONERROR;                      

		printf("Authentication failure, check credentials.\n"
			"If credentials are valid, the NTLMSSP implementation may be to blame.\n");

		credssp_free(transport->credssp);
		return FALSE;
	}

	credssp_free(transport->credssp);

	return TRUE;
}

BOOL transport_tsg_connect(rdpTransport* transport, const char* hostname, UINT16 port)
{
	rdpTsg* tsg = tsg_new(transport);

	tsg->transport = transport;
	transport->tsg = tsg;
	transport->SplitInputOutput = TRUE;

	if (transport->TlsIn == NULL)
		transport->TlsIn = tls_new(transport->settings);

	transport->TlsIn->sockfd = transport->TcpIn->sockfd;

	if (transport->TlsOut == NULL)
		transport->TlsOut = tls_new(transport->settings);

	transport->TlsOut->sockfd = transport->TcpOut->sockfd;

	if (tls_connect(transport->TlsIn) != TRUE)
		return FALSE;

	if (tls_connect(transport->TlsOut) != TRUE)
		return FALSE;

	if (!tsg_connect(tsg, hostname, port))
		return FALSE;

	return TRUE;
}

BOOL transport_connect(rdpTransport* transport, const char* hostname, UINT16 port)
{
	BOOL status = FALSE;
	rdpSettings* settings = transport->settings;

	if (transport->settings->GatewayUsageMethod)
	{
		transport->layer = TRANSPORT_LAYER_TSG;
		transport->TcpOut = tcp_new(settings);

		status = tcp_connect(transport->TcpIn, settings->GatewayHostname, 443);

		if (status)
			status = tcp_connect(transport->TcpOut, settings->GatewayHostname, 443);

		if (status)
			status = transport_tsg_connect(transport, hostname, port);
	}
	else
	{
		status = tcp_connect(transport->TcpIn, hostname, port);

		transport->SplitInputOutput = FALSE;
		transport->TcpOut = transport->TcpIn;
	}

	return status;
}

BOOL transport_accept_rdp(rdpTransport* transport)
{
	/* RDP encryption */

	return TRUE;
}

BOOL transport_accept_tls(rdpTransport* transport)
{
	if (transport->TlsIn == NULL)
		transport->TlsIn = tls_new(transport->settings);

	if (transport->TlsOut == NULL)
		transport->TlsOut = transport->TlsIn;

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->TlsIn->sockfd = transport->TcpIn->sockfd;

	if (tls_accept(transport->TlsIn, transport->settings->CertificateFile, transport->settings->PrivateKeyFile) != TRUE)
		return FALSE;

	return TRUE;
}

BOOL transport_accept_nla(rdpTransport* transport)
{
	freerdp* instance;
	rdpSettings* settings;

	if (transport->TlsIn == NULL)
		transport->TlsIn = tls_new(transport->settings);

	transport->layer = TRANSPORT_LAYER_TLS;
	transport->TlsIn->sockfd = transport->TcpIn->sockfd;

	if (tls_accept(transport->TlsIn, transport->settings->CertificateFile, transport->settings->PrivateKeyFile) != TRUE)
		return FALSE;

	/* Network Level Authentication */

	if (transport->settings->Authentication != TRUE)
		return TRUE;

	settings = transport->settings;
	instance = (freerdp*) settings->instance;

	if (transport->credssp == NULL)
		transport->credssp = credssp_new(instance, transport, settings);

	if (credssp_authenticate(transport->credssp) < 0)
	{
		printf("client authentication failure\n");
		credssp_free(transport->credssp);
		return FALSE;
	}

	/* don't free credssp module yet, we need to copy the credentials from it first */

	return TRUE;
}

int transport_read_layer(rdpTransport* transport, UINT8* data, int bytes)
{
	int status = -1;

#if 0
	int read = 0;

	/**
	 * FIXME: this breaks NLA, since the NLA packet length is improperly detected
	 */

	while (read < bytes)
	{
		if (transport->layer == TRANSPORT_LAYER_TLS)
			status = tls_read(transport->TlsIn, data + read, bytes - read);
		else if (transport->layer == TRANSPORT_LAYER_TCP)
			status = tcp_read(transport->TcpIn, data + read, bytes - read);
		else if (transport->layer == TRANSPORT_LAYER_TSG)
			status = tsg_read(transport->tsg, data + read, bytes - read);

		/* blocking means that we can't continue until this is read */

		if (!transport->blocking)
			return status;

		if (status < 0)
			return status;

		read += status;

		if (status == 0)
		{
			/* instead of sleeping, we should wait timeout on the socket
			   but this only happens on initial connection */
			USleep(transport->usleep_interval);
		}
	}

	return read;
#else

	if (transport->layer == TRANSPORT_LAYER_TLS)
		status = tls_read(transport->TlsIn, data, bytes);
	else if (transport->layer == TRANSPORT_LAYER_TCP)
		status = tcp_read(transport->TcpIn, data, bytes);
	else if (transport->layer == TRANSPORT_LAYER_TSG)
		status = tsg_read(transport->tsg, data, bytes);

	return status;

#endif
}

#if 0

/**
 * FIXME: this breaks NLA in certain cases only, why?
 */

int transport_read(rdpTransport* transport, STREAM* s)
{
	int status;
	int pdu_bytes;
	int stream_bytes;
	int transport_status;

	transport_status = 0;

	/* first check if we have header */
	stream_bytes = stream_get_length(s);

	if (stream_bytes < 4)
	{
		status = transport_read_layer(transport, s->data + stream_bytes, 4 - stream_bytes);

		if (status < 0)
			return status;

		transport_status += status;

		if ((status + stream_bytes) < 4)
			return transport_status;

		stream_bytes += status;
	}

	/* if header is present, read in exactly one PDU */
	if (s->data[0] == 0x03)
	{
		pdu_bytes = (s->data[2] << 8) | s->data[3];
	}
	else
	{
		if (s->data[1] & 0x80)
			pdu_bytes = ((s->data[1] & 0x7f) << 8) | s->data[2];
		else
			pdu_bytes = s->data[1];
	}

	status = transport_read_layer(transport, s->data + stream_bytes, pdu_bytes - stream_bytes);

	if (status < 0)
		return status;

	transport_status += status;

#ifdef WITH_DEBUG_TRANSPORT
	/* dump when whole PDU is read */
	if (stream_bytes + status >= pdu_bytes)
	{
		printf("Local < Remote\n");
		winpr_HexDump(s->data, pdu_bytes);
	}
#endif

	return transport_status;
}

#else

int transport_read(rdpTransport* transport, STREAM* s)
{
	int status = -1;

	while (TRUE)
	{
		if (transport->layer == TRANSPORT_LAYER_TLS)
			status = tls_read(transport->TlsIn, stream_get_tail(s), stream_get_left(s));
		else if (transport->layer == TRANSPORT_LAYER_TCP)
			status = tcp_read(transport->TcpIn, stream_get_tail(s), stream_get_left(s));
		else if (transport->layer == TRANSPORT_LAYER_TSG)
			status = tsg_read(transport->tsg, stream_get_tail(s), stream_get_left(s));

		if ((status == 0) && (transport->blocking))
		{
			USleep(transport->usleep_interval);
			continue;
		}

		break;
	}

#ifdef WITH_DEBUG_TRANSPORT
	if (status > 0)
	{
		printf("Local < Remote\n");
		winpr_HexDump(s->data, status);
	}
#endif

	return status;
}

#endif

static int transport_read_nonblocking(rdpTransport* transport)
{
	int status;

	stream_check_size(transport->recv_buffer, 32 * 1024);
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

	length = stream_get_length(s);
	stream_set_pos(s, 0);

#ifdef WITH_DEBUG_TRANSPORT
	if (length > 0)
	{
		printf("Local > Remote\n");
		winpr_HexDump(s->data, length);
	}
#endif

	while (length > 0)
	{
		if (transport->layer == TRANSPORT_LAYER_TLS)
			status = tls_write(transport->TlsOut, stream_get_tail(s), length);
		else if (transport->layer == TRANSPORT_LAYER_TCP)
			status = tcp_write(transport->TcpOut, stream_get_tail(s), length);
		else if (transport->layer == TRANSPORT_LAYER_TSG)
			status = tsg_write(transport->tsg, stream_get_tail(s), length);

		if (status < 0)
			break; /* error occurred */

		if (status == 0)
		{
			/* blocking while sending */
			USleep(transport->usleep_interval);

			/* when sending is blocked in nonblocking mode, the receiving buffer should be checked */
			if (!transport->blocking)
			{
				/* and in case we do have buffered some data, we set the event so next loop will get it */
				if (transport_read_nonblocking(transport) > 0)
					SetEvent(transport->recv_event);
			}
		}

		length -= status;
		stream_seek(s, status);
	}

	if (status < 0)
	{
		/* A write error indicates that the peer has dropped the connection */
		transport->layer = TRANSPORT_LAYER_CLOSED;
	}

	return status;
}

void transport_get_fds(rdpTransport* transport, void** rfds, int* rcount)
{
	void* pfd;

#ifdef _WIN32
	rfds[*rcount] = transport->TcpIn->wsa_event;
	(*rcount)++;

	if (transport->SplitInputOutput)
	{
		rfds[*rcount] = transport->TcpOut->wsa_event;
		(*rcount)++;
	}
#else
	rfds[*rcount] = (void*)(long)(transport->TcpIn->sockfd);
	(*rcount)++;

	if (transport->SplitInputOutput)
	{
		rfds[*rcount] = (void*)(long)(transport->TcpOut->sockfd);
		(*rcount)++;
	}
#endif

	pfd = GetEventWaitObject(transport->recv_event);

	if (pfd)
	{
		rfds[*rcount] = pfd;
		(*rcount)++;
	}
}

int transport_check_fds(rdpTransport** ptransport)
{
	int pos;
	int status;
	UINT16 length;
	STREAM* received;
	rdpTransport* transport = *ptransport;

#ifdef _WIN32
	WSAResetEvent(transport->TcpIn->wsa_event);
#endif
	ResetEvent(transport->recv_event);

	status = transport_read_nonblocking(transport);

	if (status < 0)
		return status;

	while ((pos = stream_get_pos(transport->recv_buffer)) > 0)
	{
		stream_set_pos(transport->recv_buffer, 0);

		if (tpkt_verify_header(transport->recv_buffer)) /* TPKT */
		{
			/* Ensure the TPKT header is available. */
			if (pos <= 4)
			{
				stream_set_pos(transport->recv_buffer, pos);
				return 0;
			}

			length = tpkt_read_header(transport->recv_buffer);
		}
		else /* Fast Path */
		{
			/* Ensure the Fast Path header is available. */
			if (pos <= 2)
			{
				stream_set_pos(transport->recv_buffer, pos);
				return 0;
			}

			/* Fastpath header can be two or three bytes long. */
			length = fastpath_header_length(transport->recv_buffer);

			if (pos < length)
			{
				stream_set_pos(transport->recv_buffer, pos);
				return 0;
			}

			length = fastpath_read_header(NULL, transport->recv_buffer);
		}

		if (length == 0)
		{
			printf("transport_check_fds: protocol error, not a TPKT or Fast Path header.\n");
			winpr_HexDump(stream_get_head(transport->recv_buffer), pos);
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

		stream_set_pos(received, length);
		stream_seal(received);
		stream_set_pos(received, 0);

		if (transport->recv_callback(transport, received, transport->recv_extra) == FALSE)
			status = -1;

		stream_free(received);

		if (status < 0)
			return status;

		/* transport might now have been freed by rdp_client_redirect and a new rdp->transport created */
		transport = *ptransport;

		if (transport->ProcessSinglePdu)
		{
			/* one at a time but set event if data buffered
			 * so the main loop will call freerdp_check_fds asap */
			if (stream_get_pos(transport->recv_buffer) > 0)
				SetEvent(transport->recv_event);
			break;
		}

	}

	return 0;
}

BOOL transport_set_blocking_mode(rdpTransport* transport, BOOL blocking)
{
	BOOL status;

	status = TRUE;
	transport->blocking = blocking;

	if (transport->SplitInputOutput)
	{
		status &= tcp_set_blocking_mode(transport->TcpIn, blocking);
		status &= tcp_set_blocking_mode(transport->TcpOut, blocking);
	}
	else
	{
		status &= tcp_set_blocking_mode(transport->TcpIn, blocking);
	}

	if (transport->layer == TRANSPORT_LAYER_TSG)
	{
		tsg_set_blocking_mode(transport->tsg, blocking);
	}

	return status;
}

rdpTransport* transport_new(rdpSettings* settings)
{
	rdpTransport* transport;

	transport = (rdpTransport*) malloc(sizeof(rdpTransport));
	ZeroMemory(transport, sizeof(rdpTransport));

	if (transport != NULL)
	{
		transport->TcpIn = tcp_new(settings);

		transport->settings = settings;

		/* a small 0.1ms delay when transport is blocking. */
		transport->usleep_interval = 100;

		/* receive buffer for non-blocking read. */
		transport->recv_buffer = stream_new(BUFFER_SIZE);
		transport->recv_event = CreateEvent(NULL, TRUE, FALSE, NULL);

		/* buffers for blocking read/write */
		transport->recv_stream = stream_new(BUFFER_SIZE);
		transport->send_stream = stream_new(BUFFER_SIZE);

		transport->blocking = TRUE;

		transport->layer = TRANSPORT_LAYER_TCP;
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
		CloseHandle(transport->recv_event);

		if (transport->TlsIn)
			tls_free(transport->TlsIn);

		if (transport->TlsOut != transport->TlsIn)
			tls_free(transport->TlsOut);

		tcp_free(transport->TcpIn);

		if (transport->TcpOut != transport->TcpIn)
			tcp_free(transport->TcpOut);

		tsg_free(transport->tsg);

		free(transport);
	}
}
