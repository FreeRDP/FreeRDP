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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/error.h>
#include <freerdp/utils/tcp.h>
#include <freerdp/utils/ringbuffer.h>

#include <openssl/bio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif /* _WIN32 */

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "tpkt.h"
#include "fastpath.h"
#include "transport.h"
#include "rdp.h"


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
	transport->frontBio = transport->TcpIn->bufferedBio;
}

void transport_stop(rdpTransport* transport)
{
	if (transport->async)
	{
		if (transport->stopEvent)
		{
			SetEvent(transport->stopEvent);
			WaitForSingleObject(transport->thread, INFINITE);

			CloseHandle(transport->thread);
			CloseHandle(transport->stopEvent);

			transport->thread = NULL;
			transport->stopEvent = NULL;
		}
	}
}

BOOL transport_disconnect(rdpTransport* transport)
{
	BOOL status = TRUE;

	if (!transport)
		return FALSE;

	transport_stop(transport);

	BIO_free_all(transport->frontBio);

	transport->frontBio = 0;
	return status;
}

BOOL transport_connect_rdp(rdpTransport* transport)
{
	/* RDP encryption */

	return TRUE;
}

long transport_bio_tsg_callback(BIO* bio, int mode, const char* argp, int argi, long argl, long ret)
{
	return 1;
}

static int transport_bio_tsg_write(BIO* bio, const char* buf, int num)
{
	int status;
	rdpTsg* tsg;

	tsg = (rdpTsg*) bio->ptr;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE);
	status = tsg_write(tsg, (BYTE*) buf, num);

	if (status < 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	}
	else
	{
		BIO_set_flags(bio, BIO_FLAGS_WRITE);
	}

	return status >= 0 ? status : -1;
}

static int transport_bio_tsg_read(BIO* bio, char* buf, int size)
{
	int status;
	rdpTsg* tsg;

	tsg = (rdpTsg*) bio->ptr;

	BIO_clear_flags(bio, BIO_FLAGS_READ);

	status = tsg_read(bio->ptr, (BYTE*) buf, size);

	if (status < 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	}
	else
	{
		BIO_set_flags(bio, BIO_FLAGS_READ);
	}

	return status >= 0 ? status : -1;
}

static int transport_bio_tsg_puts(BIO* bio, const char* str)
{
	return 1;
}

static int transport_bio_tsg_gets(BIO* bio, char* str, int size)
{
	return 1;
}

static long transport_bio_tsg_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	if (cmd == BIO_CTRL_FLUSH)
	{
		return 1;
	}

	return 0;
}

static int transport_bio_tsg_new(BIO* bio)
{
	bio->init = 1;
	bio->num = 0;
	bio->ptr = NULL;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;

	return 1;
}

static int transport_bio_tsg_free(BIO* bio)
{
	return 1;
}

#define BIO_TYPE_TSG	65

static BIO_METHOD transport_bio_tsg_methods =
{
	BIO_TYPE_TSG,
	"TSGateway",
	transport_bio_tsg_write,
	transport_bio_tsg_read,
	transport_bio_tsg_puts,
	transport_bio_tsg_gets,
	transport_bio_tsg_ctrl,
	transport_bio_tsg_new,
	transport_bio_tsg_free,
	NULL,
};

BIO_METHOD* BIO_s_tsg(void)
{
	return &transport_bio_tsg_methods;
}



BOOL transport_connect_tls(rdpTransport* transport)
{
	rdpSettings *settings = transport->settings;
	rdpTls *targetTls;
	BIO *targetBio;
	int tls_status;
	freerdp* instance;
	rdpContext* context;

	instance = (freerdp*) transport->settings->instance;
	context = instance->context;

	if (transport->layer == TRANSPORT_LAYER_TSG)
	{
		transport->TsgTls = tls_new(transport->settings);
		transport->layer = TRANSPORT_LAYER_TSG_TLS;

		targetTls = transport->TsgTls;
		targetBio = transport->frontBio;
	}
	else
	{
		if (!transport->TlsIn)
			transport->TlsIn = tls_new(settings);

		if (!transport->TlsOut)
			transport->TlsOut = transport->TlsIn;

		targetTls = transport->TlsIn;
		targetBio = transport->TcpIn->bufferedBio;

		transport->layer = TRANSPORT_LAYER_TLS;
	}


	targetTls->hostname = settings->ServerHostname;
	targetTls->port = settings->ServerPort;

	if (targetTls->port == 0)
		targetTls->port = 3389;

	targetTls->isGatewayTransport = FALSE;

	tls_status = tls_connect(targetTls, targetBio);

	if (tls_status < 1)
	{
		if (tls_status < 0)
		{
			if (!connectErrorCode)
				connectErrorCode = TLSCONNECTERROR;

			if (!freerdp_get_last_error(context))
				freerdp_set_last_error(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		}
		else
		{
			if (!freerdp_get_last_error(context))
				freerdp_set_last_error(context, FREERDP_ERROR_CONNECT_CANCELLED);
		}

		return FALSE;
	}

	transport->frontBio = targetTls->bio;
	if (!transport->frontBio)
	{
		fprintf(stderr, "%s: unable to prepend a filtering TLS bio", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

BOOL transport_connect_nla(rdpTransport* transport)
{
	freerdp* instance;
	rdpSettings* settings;
	rdpCredssp *credSsp;

	settings = transport->settings;
	instance = (freerdp*) settings->instance;

	if (!transport_connect_tls(transport))
		return FALSE;

	/* Network Level Authentication */

	if (!settings->Authentication)
		return TRUE;

	if (!transport->credssp)
	{
		transport->credssp = credssp_new(instance, transport, settings);
		if (!transport->credssp)
			return FALSE;

		transport_set_nla_mode(transport, TRUE);

		if (settings->AuthenticationServiceClass)
		{
			transport->credssp->ServicePrincipalName =
				credssp_make_spn(settings->AuthenticationServiceClass, settings->ServerHostname);
			if (!transport->credssp->ServicePrincipalName)
				return FALSE;
		}
	}

	credSsp = transport->credssp;
	if (credssp_authenticate(credSsp) < 0)
	{
		if (!connectErrorCode)
			connectErrorCode = AUTHENTICATIONERROR;

		if (!freerdp_get_last_error(instance->context))
		{
			freerdp_set_last_error(instance->context, FREERDP_ERROR_AUTHENTICATION_FAILED);
		}

		fprintf(stderr, "Authentication failure, check credentials.\n"
			"If credentials are valid, the NTLMSSP implementation may be to blame.\n");

		transport_set_nla_mode(transport, FALSE);
		credssp_free(credSsp);
		transport->credssp = NULL;

		return FALSE;
	}

	transport_set_nla_mode(transport, FALSE);
	credssp_free(credSsp);
	transport->credssp = NULL;

	return TRUE;
}

BOOL transport_tsg_connect(rdpTransport* transport, const char* hostname, UINT16 port)
{
	rdpTsg* tsg;
	int tls_status;
	freerdp* instance;
	rdpContext* context;
	rdpSettings *settings = transport->settings;

	instance = (freerdp*) transport->settings->instance;
	context = instance->context;

	tsg = tsg_new(transport);

	if (!tsg)
		return FALSE;

	tsg->transport = transport;
	transport->tsg = tsg;
	transport->SplitInputOutput = TRUE;

	if (!transport->TlsIn)
	{
		transport->TlsIn = tls_new(settings);
		if (!transport->TlsIn)
			return FALSE;
	}
	if (!transport->TlsOut)
	{
		transport->TlsOut = tls_new(settings);
		if (!transport->TlsOut)
			return FALSE;
	}

	/* put a decent default value for gateway port */
	if (!settings->GatewayPort)
		settings->GatewayPort = 443;

	transport->TlsIn->hostname = transport->TlsOut->hostname = settings->GatewayHostname;
	transport->TlsIn->port = transport->TlsOut->port = settings->GatewayPort;

	transport->TlsIn->isGatewayTransport = TRUE;

	tls_status = tls_connect(transport->TlsIn, transport->TcpIn->bufferedBio);
	if (tls_status < 1)
	{
		if (tls_status < 0)
		{
			if (!freerdp_get_last_error(context))
				freerdp_set_last_error(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		}
		else
		{
			if (!freerdp_get_last_error(context))
				freerdp_set_last_error(context, FREERDP_ERROR_CONNECT_CANCELLED);
		}

		return FALSE;
	}

	transport->TlsOut->isGatewayTransport = TRUE;

	tls_status = tls_connect(transport->TlsOut, transport->TcpOut->bufferedBio);
	if (tls_status < 1)
	{
		if (tls_status < 0)
		{
			if (!freerdp_get_last_error(context))
				freerdp_set_last_error(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		}
		else
		{
			if (!freerdp_get_last_error(context))
				freerdp_set_last_error(context, FREERDP_ERROR_CONNECT_CANCELLED);
		}

		return FALSE;
	}

	if (!tsg_connect(tsg, hostname, port))
		return FALSE;

	transport->frontBio = BIO_new(BIO_s_tsg());
	transport->frontBio->ptr = tsg;
	return TRUE;
}

BOOL transport_connect(rdpTransport* transport, const char* hostname, UINT16 port, int timeout)
{
	BOOL status = FALSE;
	rdpSettings* settings = transport->settings;

	transport->async = settings->AsyncTransport;

	if (transport->GatewayEnabled)
	{
		transport->layer = TRANSPORT_LAYER_TSG;
		transport->SplitInputOutput = TRUE;
		transport->TcpOut = tcp_new(settings);

		if (!tcp_connect(transport->TcpIn, settings->GatewayHostname, settings->GatewayPort, timeout) ||
				!tcp_set_blocking_mode(transport->TcpIn, FALSE))
			return FALSE;

		if (!tcp_connect(transport->TcpOut, settings->GatewayHostname, settings->GatewayPort, timeout) ||
				!tcp_set_blocking_mode(transport->TcpOut, FALSE))
			return FALSE;

		if (!transport_tsg_connect(transport, hostname, port))
			return FALSE;
		status = TRUE;
	}
	else
	{
		status = tcp_connect(transport->TcpIn, hostname, port, timeout);

		transport->SplitInputOutput = FALSE;
		transport->TcpOut = transport->TcpIn;
		transport->frontBio = transport->TcpIn->bufferedBio;
	}

	if (status)
	{
		if (transport->async)
		{
			transport->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

			transport->thread = CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE) transport_client_thread, transport, 0, NULL);
		}
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
	if (!transport->TlsIn)
		transport->TlsIn = tls_new(transport->settings);

	if (!transport->TlsOut)
		transport->TlsOut = transport->TlsIn;

	transport->layer = TRANSPORT_LAYER_TLS;

	if (!tls_accept(transport->TlsIn, transport->TcpIn->bufferedBio, transport->settings->CertificateFile, transport->settings->PrivateKeyFile))
		return FALSE;

	transport->frontBio = transport->TlsIn->bio;
	return TRUE;
}

BOOL transport_accept_nla(rdpTransport* transport)
{
	freerdp* instance;
	rdpSettings* settings;

	settings = transport->settings;
	instance = (freerdp*) settings->instance;

	if (!transport->TlsIn)
		transport->TlsIn = tls_new(transport->settings);

	if (!transport->TlsOut)
		transport->TlsOut = transport->TlsIn;

	transport->layer = TRANSPORT_LAYER_TLS;

	if (!tls_accept(transport->TlsIn, transport->TcpIn->bufferedBio, settings->CertificateFile, settings->PrivateKeyFile))
		return FALSE;
	transport->frontBio = transport->TlsIn->bio;

	/* Network Level Authentication */

	if (!settings->Authentication)
		return TRUE;

	if (!transport->credssp)
	{
		transport->credssp = credssp_new(instance, transport, settings);
		transport_set_nla_mode(transport, TRUE);
	}

	if (credssp_authenticate(transport->credssp) < 0)
	{
		fprintf(stderr, "client authentication failure\n");

		transport_set_nla_mode(transport, FALSE);
		credssp_free(transport->credssp);
		transport->credssp = NULL;

		tls_set_alert_code(transport->TlsIn, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_DESCRIPTION_ACCESS_DENIED);

		return FALSE;
	}

	/* don't free credssp module yet, we need to copy the credentials from it first */
	transport_set_nla_mode(transport, FALSE);

	return TRUE;
}

BOOL nla_verify_header(wStream* s)
{
	if ((Stream_Pointer(s)[0] == 0x30) && (Stream_Pointer(s)[1] & 0x80))
		return TRUE;

	return FALSE;
}

UINT32 nla_read_header(wStream* s)
{
	UINT32 length = 0;

	if (Stream_Pointer(s)[1] & 0x80)
	{
		if ((Stream_Pointer(s)[1] & ~(0x80)) == 1)
		{
			length = Stream_Pointer(s)[2];
			length += 3;
			Stream_Seek(s, 3);
		}
		else if ((Stream_Pointer(s)[1] & ~(0x80)) == 2)
		{
			length = (Stream_Pointer(s)[2] << 8) | Stream_Pointer(s)[3];
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
		length = Stream_Pointer(s)[1];
		length += 2;
		Stream_Seek(s, 2);
	}

	return length;
}

UINT32 nla_header_length(wStream* s)
{
	UINT32 length = 0;

	if (Stream_Pointer(s)[1] & 0x80)
	{
		if ((Stream_Pointer(s)[1] & ~(0x80)) == 1)
			length = 3;
		else if ((Stream_Pointer(s)[1] & ~(0x80)) == 2)
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

static int transport_wait_for_read(rdpTransport* transport)
{
	rdpTcp *tcpIn = transport->TcpIn;

	if (tcpIn->readBlocked)
	{
		return tcp_wait_read(tcpIn, 10);
	}
	else if (tcpIn->writeBlocked)
	{
		return tcp_wait_write(tcpIn, 10);
	}

	USleep(1000);
	return 0;
}


static int transport_wait_for_write(rdpTransport* transport)
{
	rdpTcp *tcpOut;

	tcpOut = transport->SplitInputOutput ? transport->TcpOut : transport->TcpIn;
	if (tcpOut->writeBlocked)
	{
		return tcp_wait_write(tcpOut, 10);
	}
	else if (tcpOut->readBlocked)
	{
		return tcp_wait_read(tcpOut, 10);
	}

	USleep(1000);
	return 0;
}


int transport_read_layer(rdpTransport* transport, BYTE* data, int bytes)
{
	int read = 0;
	int status = -1;

	if (!transport->frontBio)
	{
		transport->layer = TRANSPORT_LAYER_CLOSED;
		return -1;
	}

	while (read < bytes)
	{
		status = BIO_read(transport->frontBio, data + read, bytes - read);

		if (status <= 0)
		{
			if (!transport->frontBio || !BIO_should_retry(transport->frontBio))
			{
				/* something unexpected happened, let's close */
				transport->layer = TRANSPORT_LAYER_CLOSED;
				return -1;
			}

			/* non blocking will survive a partial read */
			if (!transport->blocking)
				return read;

			/* blocking means that we can't continue until we have read the number of
			 * requested bytes */
			if (transport_wait_for_read(transport) < 0)
			{
				fprintf(stderr, "%s: error when selecting for read\n", __FUNCTION__);
				return -1;
			}
			continue;
		}

#ifdef HAVE_VALGRIND_MEMCHECK_H
		VALGRIND_MAKE_MEM_DEFINED(data + read, bytes - read);
#endif

		read += status;
	}

	return read;
}



int transport_read(rdpTransport* transport, wStream* s)
{
	int status;
	int position;
	int pduLength;
	BYTE *header;
	int transport_status;

	position = 0;
	pduLength = 0;
	transport_status = 0;

	if (!transport)
		return -1;

	if (!s)
		return -1;

	/* first check if we have header */
	position = Stream_GetPosition(s);

	if (position < 4)
	{
		Stream_EnsureCapacity(s, 4);
		status = transport_read_layer(transport, Stream_Buffer(s) + position, 4 - position);

		if (status < 0)
			return status;

		transport_status += status;

		if ((status + position) < 4)
			return transport_status;

		position += status;
	}

	header = Stream_Buffer(s);

	/* if header is present, read exactly one PDU */

	if (transport->NlaMode)
	{
		if (header[0] == 0x30)
		{
			/* TSRequest (NLA) */

			if (header[1] & 0x80)
			{
				if ((header[1] & ~(0x80)) == 1)
				{
					pduLength = header[2];
					pduLength += 3;
				}
				else if ((header[1] & ~(0x80)) == 2)
				{
					pduLength = (header[2] << 8) | header[3];
					pduLength += 4;
				}
				else
				{
					fprintf(stderr, "Error reading TSRequest!\n");
					return -1;
				}
			}
			else
			{
				pduLength = header[1];
				pduLength += 2;
			}
		}
	}
	else
	{
		if (header[0] == 0x03)
		{
			/* TPKT header */

			pduLength = (header[2] << 8) | header[3];
		}
		else
		{
			/* Fast-Path Header */

			if (header[1] & 0x80)
				pduLength = ((header[1] & 0x7F) << 8) | header[2];
			else
				pduLength = header[1];
		}
	}

	if (pduLength < 0 || pduLength > 0xFFFF)
	{
		fprintf(stderr, "%s: invalid pduLength: %d\n", __FUNCTION__, pduLength);
		return -1;
	}

	Stream_EnsureCapacity(s, pduLength);
	status = transport_read_layer(transport, Stream_Buffer(s) + position, pduLength - position);

	if (status < 0)
		return status;

	transport_status += status;

#ifdef WITH_DEBUG_TRANSPORT
	/* dump when whole PDU is read */
	if (position + status >= pduLength)
	{
		fprintf(stderr, "Local < Remote\n");
		winpr_HexDump(Stream_Buffer(s), pduLength);
	}
#endif

	if (position + status >= pduLength)
	{
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), pduLength, WLOG_PACKET_INBOUND);
	}

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

BOOL transport_bio_buffered_drain(BIO *bio);

int transport_write(rdpTransport* transport, wStream* s)
{
	int length;
	int status = -1;

	EnterCriticalSection(&(transport->WriteLock));

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

#ifdef WITH_DEBUG_TRANSPORT
	if (length > 0)
	{
		fprintf(stderr, "Local > Remote\n");
		winpr_HexDump(Stream_Buffer(s), length);
	}
#endif

	if (length > 0)
	{
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), length, WLOG_PACKET_OUTBOUND);
	}

	while (length > 0)
	{
		status = BIO_write(transport->frontBio, Stream_Pointer(s), length);

		if (status <= 0)
		{
			/* the buffered BIO that is at the end of the chain always says OK for writing,
			 * so a retry means that for any reason we need to read. The most probable
			 * is a SSL or TSG BIO in the chain.
			 */
			if (!BIO_should_retry(transport->frontBio))
				return status;

			/* non-blocking can live with blocked IOs */
			if (!transport->blocking)
				return status;

			if (transport_wait_for_write(transport) < 0)
			{
				fprintf(stderr, "%s: error when selecting for write\n", __FUNCTION__);
				return -1;
			}
			continue;
		}

		if (transport->blocking || transport->settings->WaitForOutputBufferFlush)
		{
			/* blocking transport, we must ensure the write buffer is really empty */
			rdpTcp *out = transport->TcpOut;

			while (out->writeBlocked)
			{
				if (transport_wait_for_write(transport) < 0)
				{
					fprintf(stderr, "%s: error when selecting for write\n", __FUNCTION__);
					return -1;
				}

				if (!transport_bio_buffered_drain(out->bufferedBio))
				{
					fprintf(stderr, "%s: error when draining outputBuffer\n", __FUNCTION__);
					return -1;
				}
			}
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

	LeaveCriticalSection(&(transport->WriteLock));

	return status;
}

HANDLE *transport_get_event_handles(rdpTransport *transport, HANDLE *handles, DWORD *count)
{
	HANDLE *hdl;
	DWORD cnt = 7;

	if (!count)
	{
		DEBUG_WARN("count=%p", count);
		return NULL;
	}

	/* Allocate the maximum number of event handles that can be added.
	 * Bit of a waste, but works ;) */
	cnt += *count;
	hdl = realloc(handles, cnt * sizeof(HANDLE));

	if (!hdl)
	{
		DEBUG_WARN("could not realloc to size %08X: %s (%d)", cnt,
				   strerror(errno), errno);
		return NULL;
	}

	if (transport->async)
		hdl[(*count)++] = transport->thread;

#ifdef _WIN32
	hdl[(*count)++] = transport->TcpIn->wsa_event;

	if (transport->SplitInputOutput)
		hdl[(*count)++] = transport->TcpOut->wsa_event;
#else
	hdl[(*count)++] = tcp_get_event_handle(transport->TcpIn);

	if (transport->SplitInputOutput)
	{
		hdl[(*count)++] = tcp_get_event_handle(transport->TcpOut);
	}
#endif

	if (transport->ReceiveEvent)
		hdl[(*count++)] = transport->ReceiveEvent;

	if (transport->GatewayEvent)
		hdl[(*count++)] = transport->GatewayEvent;

	return hdl;
}

static void transport_get_read_handles(rdpTransport *transport, HANDLE *events, DWORD *count)
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

BOOL tranport_is_write_blocked(rdpTransport* transport)
{
	if (transport->TcpIn->writeBlocked)
		return TRUE;

	return transport->SplitInputOutput &&
			transport->TcpOut &&
			transport->TcpOut->writeBlocked;
}

int tranport_drain_output_buffer(rdpTransport* transport)
{
	BOOL ret = FALSE;

	/* First try to send some accumulated bytes in the send buffer */
	if (transport->TcpIn->writeBlocked)
	{
		if (!transport_bio_buffered_drain(transport->TcpIn->bufferedBio))
			return -1;
		ret |= transport->TcpIn->writeBlocked;
	}

	if (transport->SplitInputOutput && transport->TcpOut && transport->TcpOut->writeBlocked)
	{
		if (!transport_bio_buffered_drain(transport->TcpOut->bufferedBio))
			return -1;
		ret |= transport->TcpOut->writeBlocked;
	}

	return ret;
}

int transport_check_fds(rdpTransport* transport)
{
	int pos;
	int status;
	int length;
	int recv_status;
	wStream* received;

	if (!transport)
		return -1;

#ifdef _WIN32
	WSAResetEvent(transport->TcpIn->wsa_event);
#endif
	ResetEvent(transport->ReceiveEvent);

	/**
	 * Loop through and read all available PDUs.  Since multiple
	 * PDUs can exist, it's important to deliver them all before
	 * returning.  Otherwise we run the risk of having a thread
	 * wait for a socket to get signaled that data is available
	 * (which may never happen).
	 */
	for (;;)
	{
		/**
		 * Note: transport_read_nonblocking() reads max 1 additional PDU from
		 * the layer. Also note that transport_read_nonblocking() is also called
		 * outside of this function in transport_write()! This means that when
		 * entering transport_check_fds it is possible that the stream position
		 * of transport->ReceiveBuffer position is > 0. We must process this data
		 * even if transport_read_nonblocking() returns 0.
		 * Note that transport->ReceiveBuffer is replaced after each iteration
		 * of this loop with a fresh stream instance from a pool.
		 */

		if ((status = transport_read_nonblocking(transport)) < 0)
			return status;

		if ((pos = Stream_GetPosition(transport->ReceiveBuffer)) < 2)
			return 0;

		Stream_SetPosition(transport->ReceiveBuffer, 0);
		length = 0;

		if (transport->NlaMode)
		{
			if (nla_verify_header(transport->ReceiveBuffer))
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
		}
		else
		{
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

		/**
		 * status:
		 * 	-1: error
		 * 	 0: success
		 * 	 1: redirection
		 */

		recv_status = transport->ReceiveCallback(transport, received, transport->ReceiveExtra);

		if (recv_status == 1)
		{
			return 1; /* session redirection */
		}
		Stream_Release(received);

		if (recv_status < 0)
			return -1;
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

	if (transport->layer == TRANSPORT_LAYER_TSG || transport->layer == TRANSPORT_LAYER_TSG_TLS)
	{
		tsg_set_blocking_mode(transport->tsg, blocking);
	}

	return status;
}

void transport_set_gateway_enabled(rdpTransport* transport, BOOL GatewayEnabled)
{
	transport->GatewayEnabled = GatewayEnabled;
}

void transport_set_nla_mode(rdpTransport* transport, BOOL NlaMode)
{
	transport->NlaMode = NlaMode;
}

static void* transport_client_thread(void* arg)
{
	DWORD status;
	DWORD nCount;
	HANDLE handles[8];
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
	
	WLog_Print(transport->log, WLOG_DEBUG, "Starting transport thread");

	nCount = 0;
	handles[nCount++] = transport->stopEvent;
	handles[nCount++] = transport->connectedEvent;
	
	status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);
	
	if (WaitForSingleObject(transport->stopEvent, 0) == WAIT_OBJECT_0)
	{
		WLog_Print(transport->log, WLOG_DEBUG, "Terminating transport thread");
		ExitThread(0);
		return NULL;
	}

	WLog_Print(transport->log, WLOG_DEBUG, "Asynchronous transport activated");

	while (1)
	{
		nCount = 0;
		handles[nCount++] = transport->stopEvent;

		transport_get_read_handles(transport, (HANDLE*) &handles, &nCount);

		status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);
		if (transport->layer == TRANSPORT_LAYER_CLOSED)
		{
			rdpRdp* rdp = (rdpRdp*) transport->rdp;
			rdp_set_error_info(rdp, ERRINFO_PEER_DISCONNECTED);
			break;
		}
		else if (status != WAIT_TIMEOUT)
		{
			if (WaitForSingleObject(transport->stopEvent, 0) == WAIT_OBJECT_0)
				break;

			if (!freerdp_check_fds(instance))
			{

			}
		}
	}

	WLog_Print(transport->log, WLOG_DEBUG, "Terminating transport thread");

	ExitThread(0);
	return NULL;
}

rdpTransport* transport_new(rdpSettings* settings)
{
	rdpTransport* transport;

	transport = (rdpTransport *)calloc(1, sizeof(rdpTransport));
	if (!transport)
		return NULL;

	WLog_Init();
	transport->log = WLog_Get("com.freerdp.core.transport");
	if (!transport->log)
		goto out_free;

	transport->TcpIn = tcp_new(settings);
	if (!transport->TcpIn)
		goto out_free;

	transport->settings = settings;

	/* a small 0.1ms delay when transport is blocking. */
	transport->SleepInterval = 100;

	transport->ReceivePool = StreamPool_New(TRUE, BUFFER_SIZE);
	if (!transport->ReceivePool)
		goto out_free_tcpin;

	/* receive buffer for non-blocking read. */
	transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0);
	if (!transport->ReceiveBuffer)
		goto out_free_receivepool;

	transport->ReceiveEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!transport->ReceiveEvent || transport->ReceiveEvent == INVALID_HANDLE_VALUE)
		goto out_free_receivebuffer;

	transport->connectedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!transport->connectedEvent || transport->connectedEvent == INVALID_HANDLE_VALUE)
		goto out_free_receiveEvent;

	transport->blocking = TRUE;
	transport->GatewayEnabled = FALSE;
	transport->layer = TRANSPORT_LAYER_TCP;

	if (!InitializeCriticalSectionAndSpinCount(&(transport->ReadLock), 4000))
		goto out_free_connectedEvent;
	if (!InitializeCriticalSectionAndSpinCount(&(transport->WriteLock), 4000))
		goto out_free_readlock;

	return transport;

out_free_readlock:
	DeleteCriticalSection(&(transport->ReadLock));
out_free_connectedEvent:
	CloseHandle(transport->connectedEvent);
out_free_receiveEvent:
	CloseHandle(transport->ReceiveEvent);
out_free_receivebuffer:
	StreamPool_Return(transport->ReceivePool, transport->ReceiveBuffer);
out_free_receivepool:
	StreamPool_Free(transport->ReceivePool);
out_free_tcpin:
	tcp_free(transport->TcpIn);
out_free:
	free(transport);
	return NULL;
}

void transport_free(rdpTransport* transport)
{
	if (!transport)
		return;

	transport_stop(transport);

	if (transport->ReceiveBuffer)
		Stream_Release(transport->ReceiveBuffer);

	StreamPool_Free(transport->ReceivePool);

	CloseHandle(transport->ReceiveEvent);
	CloseHandle(transport->connectedEvent);

	if (transport->TlsIn)
		tls_free(transport->TlsIn);

	if (transport->TlsOut != transport->TlsIn)
		tls_free(transport->TlsOut);

	transport->TlsIn = NULL;
	transport->TlsOut = NULL;

	if (transport->TcpIn)
		tcp_free(transport->TcpIn);

	if (transport->TcpOut != transport->TcpIn)
		tcp_free(transport->TcpOut);

	transport->TcpIn = NULL;
	transport->TcpOut = NULL;

	tsg_free(transport->tsg);
	transport->tsg = NULL;

	DeleteCriticalSection(&(transport->ReadLock));
	DeleteCriticalSection(&(transport->WriteLock));

	free(transport);
}
