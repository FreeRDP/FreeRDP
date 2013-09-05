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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>

#include <freerdp/error.h>
#include <freerdp/utils/tcp.h>
#include <winpr/stream.h>

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

static void* transport_client_thread(void* arg);

wStream* transport_send_stream_init(rdpTransport* transport, int size)
{
	wStream* s;

	s = StreamPool_Take(transport->ReceivePool, size);

	Stream_EnsureCapacity(s, size);
	Stream_SetPosition(s, 0);

	return s;
}

void transport_attach(rdpTransport* transport, int sockfd)
{
	tcp_attach(transport->TcpIn, sockfd);
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

	if (transport->async)
	{
		SetEvent(transport->stopEvent);
		WaitForSingleObject(transport->thread, INFINITE);

		CloseHandle(transport->thread);
		CloseHandle(transport->stopEvent);
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

		fprintf(stderr, "Authentication failure, check credentials.\n"
			"If credentials are valid, the NTLMSSP implementation may be to blame.\n");

		credssp_free(transport->credssp);
		transport->credssp = NULL;
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

	transport->async = transport->settings->AsyncTransport;

	if (transport->async)
	{
		transport->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		transport->thread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) transport_client_thread, transport, 0, NULL);
	}

	if (transport->settings->GatewayEnabled)
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

	if (transport->TlsOut == NULL)
		transport->TlsOut = transport->TlsIn;

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
		fprintf(stderr, "client authentication failure\n");
		credssp_free(transport->credssp);
		transport->credssp = NULL;
		return FALSE;
	}

	/* don't free credssp module yet, we need to copy the credentials from it first */

	return TRUE;
}

BOOL nla_verify_header(wStream* s)
{
	if ((s->pointer[0] == 0x30) && (s->pointer[1] & 0x80))
		return TRUE;

	return FALSE;
}

UINT32 nla_read_header(wStream* s)
{
	UINT32 length = 0;

	if (s->pointer[1] & 0x80)
	{
		if ((s->pointer[1] & ~(0x80)) == 1)
		{
			length = s->pointer[2];
			length += 3;
			Stream_Seek(s, 3);
		}
		else if ((s->pointer[1] & ~(0x80)) == 2)
		{
			length = (s->pointer[2] << 8) | s->pointer[3];
			length += 4;
			Stream_Seek(s, 4);
		}
		else
		{
			fprintf(stderr, "Error reading TSRequest!\n");
		}
	}
	else
	{
		length = s->pointer[1];
		length += 2;
		Stream_Seek(s, 2);
	}

	return length;
}

UINT32 nla_header_length(wStream* s)
{
	UINT32 length = 0;

	if (s->pointer[1] & 0x80)
	{
		if ((s->pointer[1] & ~(0x80)) == 1)
			length = 3;
		else if ((s->pointer[1] & ~(0x80)) == 2)
			length = 4;
		else
			fprintf(stderr, "Error reading TSRequest!\n");
	}
	else
	{
		length = 2;
	}

	return length;
}

int transport_read_layer(rdpTransport* transport, UINT8* data, int bytes)
{
	int read = 0;
	int status = -1;

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
			/*
			 * instead of sleeping, we should wait timeout on the
			 * socket but this only happens on initial connection
			 */
			USleep(transport->SleepInterval);
		}
	}

	return read;
}

int transport_read(rdpTransport* transport, wStream* s)
{
	int status;
	int pduLength;
	int streamPosition;
	int transport_status;

	pduLength = 0;
	transport_status = 0;

	/* first check if we have header */
	streamPosition = Stream_GetPosition(s);

	if (streamPosition < 4)
	{
		status = transport_read_layer(transport, Stream_Buffer(s) + streamPosition, 4 - streamPosition);

		if (status < 0)
			return status;

		transport_status += status;

		if ((status + streamPosition) < 4)
			return transport_status;

		streamPosition += status;
	}

	/* if header is present, read in exactly one PDU */
	if (s->buffer[0] == 0x03)
	{
		/* TPKT header */

		pduLength = (s->buffer[2] << 8) | s->buffer[3];
	}
	else if (s->buffer[0] == 0x30)
	{
		/* TSRequest (NLA) */

		if (s->buffer[1] & 0x80)
		{
			if ((s->buffer[1] & ~(0x80)) == 1)
			{
				pduLength = s->buffer[2];
				pduLength += 3;
			}
			else if ((s->buffer[1] & ~(0x80)) == 2)
			{
				pduLength = (s->buffer[2] << 8) | s->buffer[3];
				pduLength += 4;
			}
			else
			{
				fprintf(stderr, "Error reading TSRequest!\n");
			}
		}
		else
		{
			pduLength = s->buffer[1];
			pduLength += 2;
		}
	}
	else
	{
		/* Fast-Path Header */

		if (s->buffer[1] & 0x80)
			pduLength = ((s->buffer[1] & 0x7F) << 8) | s->buffer[2];
		else
			pduLength = s->buffer[1];
	}

	status = transport_read_layer(transport, Stream_Buffer(s) + streamPosition, pduLength - streamPosition);

	if (status < 0)
		return status;

	transport_status += status;

#ifdef WITH_DEBUG_TRANSPORT
	/* dump when whole PDU is read */
	if (streamPosition + status >= pduLength)
	{
		fprintf(stderr, "Local < Remote\n");
		winpr_HexDump(Stream_Buffer(s), pduLength);
	}
#endif

	return transport_status;
}

static int transport_read_nonblocking(rdpTransport* transport)
{
	int status;

	status = transport_read(transport, transport->ReceiveBuffer);

	if (status <= 0)
		return status;

	Stream_Seek(transport->ReceiveBuffer, status);

	return status;
}

int transport_write(rdpTransport* transport, wStream* s)
{
	int length;
	int status = -1;

	WaitForSingleObject(transport->WriteMutex, INFINITE);

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

#ifdef WITH_DEBUG_TRANSPORT
	if (length > 0)
	{
		fprintf(stderr, "Local > Remote\n");
		winpr_HexDump(Stream_Buffer(s), length);
	}
#endif

	while (length > 0)
	{
		if (transport->layer == TRANSPORT_LAYER_TLS)
			status = tls_write(transport->TlsOut, Stream_Pointer(s), length);
		else if (transport->layer == TRANSPORT_LAYER_TCP)
			status = tcp_write(transport->TcpOut, Stream_Pointer(s), length);
		else if (transport->layer == TRANSPORT_LAYER_TSG)
			status = tsg_write(transport->tsg, Stream_Pointer(s), length);

		if (status < 0)
			break; /* error occurred */

		if (status == 0)
		{
			/* when sending is blocked in nonblocking mode, the receiving buffer should be checked */
			if (!transport->blocking)
			{
				/* and in case we do have buffered some data, we set the event so next loop will get it */
				if (transport_read_nonblocking(transport) > 0)
					SetEvent(transport->ReceiveEvent);
			}

			if (transport->layer == TRANSPORT_LAYER_TLS)
				tls_wait_write(transport->TlsOut);
			else if (transport->layer == TRANSPORT_LAYER_TCP)
				tcp_wait_write(transport->TcpOut);
			else
				USleep(transport->SleepInterval);
		}

		length -= status;
		Stream_Seek(s, status);
	}

	if (status < 0)
	{
		/* A write error indicates that the peer has dropped the connection */
		transport->layer = TRANSPORT_LAYER_CLOSED;
	}

	if (s->pool)
		Stream_Release(s);

	ReleaseMutex(transport->WriteMutex);

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

	pfd = GetEventWaitObject(transport->ReceiveEvent);

	if (pfd)
	{
		rfds[*rcount] = pfd;
		(*rcount)++;
	}

	if (transport->GatewayEvent)
	{
		pfd = GetEventWaitObject(transport->GatewayEvent);

		if (pfd)
		{
			rfds[*rcount] = pfd;
			(*rcount)++;
		}
	}
}

void transport_get_read_handles(rdpTransport* transport, HANDLE* events, DWORD* count)
{
	events[*count] = tcp_get_event_handle(transport->TcpIn);
	(*count)++;

	if (transport->SplitInputOutput)
	{
		events[*count] = tcp_get_event_handle(transport->TcpOut);
		(*count)++;
	}

	if (transport->ReceiveEvent)
	{
		events[*count] = transport->ReceiveEvent;
		(*count)++;
	}

	if (transport->GatewayEvent)
	{
		events[*count] = transport->GatewayEvent;
		(*count)++;
	}
}

int transport_check_fds(rdpTransport** ptransport)
{
	int pos;
	int status;
	UINT16 length;
	int recv_status;
	wStream* received;
	rdpTransport* transport = *ptransport;

#ifdef _WIN32
	WSAResetEvent(transport->TcpIn->wsa_event);
#endif
	ResetEvent(transport->ReceiveEvent);

	status = transport_read_nonblocking(transport);

	if (status < 0)
		return status;

	while ((pos = Stream_GetPosition(transport->ReceiveBuffer)) > 0)
	{
		Stream_SetPosition(transport->ReceiveBuffer, 0);

		if (tpkt_verify_header(transport->ReceiveBuffer)) /* TPKT */
		{
			/* Ensure the TPKT header is available. */
			if (pos <= 4)
			{
				Stream_SetPosition(transport->ReceiveBuffer, pos);
				return 0;
			}

			length = tpkt_read_header(transport->ReceiveBuffer);
		}
		else if (nla_verify_header(transport->ReceiveBuffer))
		{
			/* TSRequest */

			/* Ensure the TSRequest header is available. */
			if (pos <= 4)
			{
				Stream_SetPosition(transport->ReceiveBuffer, pos);
				return 0;
			}

			/* TSRequest header can be 2, 3 or 4 bytes long */
			length = nla_header_length(transport->ReceiveBuffer);

			if (pos < length)
			{
				Stream_SetPosition(transport->ReceiveBuffer, pos);
				return 0;
			}

			length = nla_read_header(transport->ReceiveBuffer);
		}
		else /* Fast Path */
		{
			/* Ensure the Fast Path header is available. */
			if (pos <= 2)
			{
				Stream_SetPosition(transport->ReceiveBuffer, pos);
				return 0;
			}

			/* Fastpath header can be two or three bytes long. */
			length = fastpath_header_length(transport->ReceiveBuffer);

			if (pos < length)
			{
				Stream_SetPosition(transport->ReceiveBuffer, pos);
				return 0;
			}

			length = fastpath_read_header(NULL, transport->ReceiveBuffer);
		}

		if (length == 0)
		{
			fprintf(stderr, "transport_check_fds: protocol error, not a TPKT or Fast Path header.\n");
			winpr_HexDump(Stream_Buffer(transport->ReceiveBuffer), pos);
			return -1;
		}

		if (pos < length)
		{
			Stream_SetPosition(transport->ReceiveBuffer, pos);
			return 0; /* Packet is not yet completely received. */
		}

		received = transport->ReceiveBuffer;
		transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0);

		Stream_SetPosition(received, length);
		Stream_SealLength(received);
		Stream_SetPosition(received, 0);

		recv_status = transport->ReceiveCallback(transport, received, transport->ReceiveExtra);

		if (transport == *ptransport)
			/* transport might now have been freed by rdp_client_redirect and a new rdp->transport created */
			/* so only release if still valid */
			Stream_Release(received);
			

		if (recv_status < 0)
			status = -1;

		if (status < 0)
			return status;

		/* transport might now have been freed by rdp_client_redirect and a new rdp->transport created */
		transport = *ptransport;
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

static void* transport_client_thread(void* arg)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	freerdp* instance;
	rdpContext* context;
	rdpTransport* transport;

	transport = (rdpTransport*) arg;
	assert(NULL != transport);
	assert(NULL != transport->settings);
	
	instance = (freerdp*) transport->settings->instance;
	assert(NULL != instance);
	
	context = instance->context;
	assert(NULL != instance->context);

	while (1)
	{
		nCount = 0;
		events[nCount++] = transport->stopEvent;
		events[nCount] = transport->connectedEvent;

		status = WaitForMultipleObjects(nCount + 1, events, FALSE, INFINITE);
		if (status == WAIT_OBJECT_0)
			break;

		transport_get_read_handles(transport, (HANDLE*) &events, &nCount);
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);
		if (status == WAIT_OBJECT_0)
			break;

		if (!freerdp_check_fds(instance))
			break;
	}

	ExitThread(0);

	return NULL;
}

rdpTransport* transport_new(rdpSettings* settings)
{
	rdpTransport* transport;

	transport = (rdpTransport*) malloc(sizeof(rdpTransport));

	if (transport != NULL)
	{
		ZeroMemory(transport, sizeof(rdpTransport));

		transport->TcpIn = tcp_new(settings);

		transport->settings = settings;

		/* a small 0.1ms delay when transport is blocking. */
		transport->SleepInterval = 100;

		transport->ReceivePool = StreamPool_New(TRUE, BUFFER_SIZE);

		/* receive buffer for non-blocking read. */
		transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0);
		transport->ReceiveEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		transport->connectedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		transport->blocking = TRUE;

		transport->ReadMutex = CreateMutex(NULL, FALSE, NULL);
		transport->WriteMutex = CreateMutex(NULL, FALSE, NULL);

		transport->layer = TRANSPORT_LAYER_TCP;
	}

	return transport;
}

void transport_free(rdpTransport* transport)
{
	if (transport != NULL)
	{
        SetEvent(transport->stopEvent);
        
		if (transport->ReceiveBuffer)
			Stream_Release(transport->ReceiveBuffer);

		StreamPool_Free(transport->ReceivePool);

		CloseHandle(transport->ReceiveEvent);
		CloseHandle(transport->connectedEvent);

		if (transport->TlsIn)
			tls_free(transport->TlsIn);

		if (transport->TlsOut != transport->TlsIn)
			tls_free(transport->TlsOut);

		tcp_free(transport->TcpIn);

		if (transport->TcpOut != transport->TcpIn)
			tcp_free(transport->TcpOut);

		tsg_free(transport->tsg);

		CloseHandle(transport->ReadMutex);
		CloseHandle(transport->WriteMutex);

		free(transport);
	}
}
