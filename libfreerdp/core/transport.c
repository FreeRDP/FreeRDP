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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/winsock.h>
#include <winpr/crypto.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/utils/ringbuffer.h>

#include <openssl/bio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif /* _WIN32 */

#ifdef FREERDP_HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "tpkt.h"
#include "fastpath.h"
#include "transport.h"
#include "rdp.h"
#include "proxy.h"
#include "utils.h"
#include "state.h"

#define TAG FREERDP_TAG("core.transport")

#define BUFFER_SIZE 16384

struct rdp_transport
{
	TRANSPORT_LAYER layer;
	BIO* frontBio;
	rdpRdg* rdg;
	rdpTsg* tsg;
	rdpTls* tls;
	rdpContext* context;
	rdpNla* nla;
	void* ReceiveExtra;
	wStream* ReceiveBuffer;
	TransportRecv ReceiveCallback;
	wStreamPool* ReceivePool;
	HANDLE connectedEvent;
	BOOL NlaMode;
	BOOL blocking;
	BOOL GatewayEnabled;
	CRITICAL_SECTION ReadLock;
	CRITICAL_SECTION WriteLock;
	ULONG written;
	HANDLE rereadEvent;
	BOOL haveMoreBytesToRead;
	wLog* log;
	rdpTransportIo io;
	HANDLE ioEvent;
	BOOL useIoEvent;
};

static void transport_ssl_cb(SSL* ssl, int where, int ret)
{
	if (where & SSL_CB_ALERT)
	{
		rdpTransport* transport = (rdpTransport*)SSL_get_app_data(ssl);
		WINPR_ASSERT(transport);

		switch (ret)
		{
			case (SSL3_AL_FATAL << 8) | SSL_AD_ACCESS_DENIED:
			{
				if (!freerdp_get_last_error(transport_get_context(transport)))
				{
					WLog_Print(transport->log, WLOG_ERROR, "%s: ACCESS DENIED", __FUNCTION__);
					freerdp_set_last_error_log(transport_get_context(transport),
					                           FREERDP_ERROR_AUTHENTICATION_FAILED);
				}
			}
			break;

			case (SSL3_AL_FATAL << 8) | SSL_AD_INTERNAL_ERROR:
			{
				if (transport->NlaMode)
				{
					if (!freerdp_get_last_error(transport_get_context(transport)))
					{
						UINT32 kret = nla_get_error(transport->nla);
						if (kret == 0)
							kret = FREERDP_ERROR_CONNECT_PASSWORD_CERTAINLY_EXPIRED;
						freerdp_set_last_error_log(transport_get_context(transport), kret);
					}
				}

				break;

				case (SSL3_AL_WARNING << 8) | SSL3_AD_CLOSE_NOTIFY:
					break;

				default:
					WLog_Print(transport->log, WLOG_WARN,
					           "Unhandled SSL error (where=%d, ret=%d [%s, %s])", where, ret,
					           SSL_alert_type_string_long(ret), SSL_alert_desc_string_long(ret));
					break;
			}
		}
	}
}

wStream* transport_send_stream_init(rdpTransport* transport, size_t size)
{
	wStream* s;
	WINPR_ASSERT(transport);

	if (!(s = StreamPool_Take(transport->ReceivePool, size)))
		return NULL;

	if (!Stream_EnsureCapacity(s, size))
	{
		Stream_Release(s);
		return NULL;
	}

	Stream_SetPosition(s, 0);
	return s;
}

BOOL transport_attach(rdpTransport* transport, int sockfd)
{
	if (!transport)
		return FALSE;
	return IFCALLRESULT(FALSE, transport->io.TransportAttach, transport, sockfd);
}

static BOOL transport_default_attach(rdpTransport* transport, int sockfd)
{
	BIO* socketBio = NULL;
	BIO* bufferedBio;
	const rdpSettings* settings;
	rdpContext* context = transport_get_context(transport);

	if (sockfd < 0)
	{
		WLog_WARN(TAG, "Running peer without socket (sockfd=%d)", sockfd);
		return TRUE;
	}

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (sockfd >= 0)
	{
		if (!freerdp_tcp_set_keep_alive_mode(settings, sockfd))
			goto fail;

		socketBio = BIO_new(BIO_s_simple_socket());

		if (!socketBio)
			goto fail;

		BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	}

	bufferedBio = BIO_new(BIO_s_buffered_socket());
	if (!bufferedBio)
		goto fail;

	if (socketBio)
		bufferedBio = BIO_push(bufferedBio, socketBio);
	WINPR_ASSERT(bufferedBio);
	transport->frontBio = bufferedBio;
	return TRUE;
fail:

	if (socketBio)
		BIO_free_all(socketBio);
	else
		closesocket(sockfd);

	return FALSE;
}

BOOL transport_connect_rdp(rdpTransport* transport)
{
	if (!transport)
		return FALSE;

	switch (utils_authenticate(transport_get_context(transport)->instance, AUTH_RDP, FALSE))
	{
		case AUTH_SKIP:
		case AUTH_SUCCESS:
		case AUTH_NO_CREDENTIALS:
			return TRUE;
		default:
			return FALSE;
	}
}

BOOL transport_connect_tls(rdpTransport* transport)
{
	const rdpSettings* settings;
	rdpContext* context = transport_get_context(transport);

	settings = context->settings;
	WINPR_ASSERT(settings);

	/* Only prompt for password if we use TLS (NLA also calls this function) */
	if (settings->SelectedProtocol == PROTOCOL_SSL)
	{
		switch (utils_authenticate(transport_get_context(transport)->instance, AUTH_TLS, FALSE))
		{
			case AUTH_SKIP:
			case AUTH_SUCCESS:
			case AUTH_NO_CREDENTIALS:
				break;
			default:
				return FALSE;
		}
	}

	return IFCALLRESULT(FALSE, transport->io.TLSConnect, transport);
}

static BOOL transport_default_connect_tls(rdpTransport* transport)
{
	int tlsStatus;
	rdpTls* tls = NULL;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(transport);

	context = transport_get_context(transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!(tls = tls_new(settings)))
		return FALSE;

	transport->tls = tls;

	if (transport->GatewayEnabled)
		transport->layer = TRANSPORT_LAYER_TSG_TLS;
	else
		transport->layer = TRANSPORT_LAYER_TLS;

	tls->hostname = settings->ServerHostname;
	tls->port = settings->ServerPort;

	if (tls->port == 0)
		tls->port = 3389;

	tls->isGatewayTransport = FALSE;
	tlsStatus = tls_connect(tls, transport->frontBio);

	if (tlsStatus < 1)
	{
		if (tlsStatus < 0)
		{
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		}
		else
		{
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_CANCELLED);
		}

		return FALSE;
	}

	transport->frontBio = tls->bio;
	BIO_callback_ctrl(tls->bio, BIO_CTRL_SET_CALLBACK, (bio_info_cb*)(void*)transport_ssl_cb);
	SSL_set_app_data(tls->ssl, transport);

	if (!transport->frontBio)
	{
		WLog_Print(transport->log, WLOG_ERROR, "unable to prepend a filtering TLS bio");
		return FALSE;
	}

	return TRUE;
}

BOOL transport_connect_nla(rdpTransport* transport)
{
	rdpContext* context = NULL;
	rdpSettings* settings = NULL;
	rdpRdp* rdp = NULL;
	if (!transport)
		return FALSE;

	context = transport_get_context(transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	rdp = context->rdp;
	WINPR_ASSERT(rdp);

	if (!transport_connect_tls(transport))
		return FALSE;

	if (!settings->Authentication)
		return TRUE;

	nla_free(rdp->nla);
	rdp->nla = nla_new(context, transport);

	if (!rdp->nla)
		return FALSE;

	transport_set_nla_mode(transport, TRUE);

	if (settings->AuthenticationServiceClass)
	{
		if (!nla_set_service_principal(rdp->nla, settings->AuthenticationServiceClass,
		                               freerdp_settings_get_server_name(settings)))
			return FALSE;
	}

	if (nla_client_begin(rdp->nla) < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR, "NLA begin failed");

		freerdp_set_last_error_if_not(context, FREERDP_ERROR_AUTHENTICATION_FAILED);

		transport_set_nla_mode(transport, FALSE);
		return FALSE;
	}

	return rdp_client_transition_to_state(rdp, CONNECTION_STATE_NLA);
}

BOOL transport_connect(rdpTransport* transport, const char* hostname, UINT16 port, DWORD timeout)
{
	int sockfd;
	BOOL status = FALSE;
	rdpSettings* settings;
	rdpContext* context = transport_get_context(transport);
	BOOL rpcFallback;

	WINPR_ASSERT(context);
	WINPR_ASSERT(hostname);

	settings = context->settings;
	WINPR_ASSERT(settings);

	rpcFallback = !settings->GatewayHttpTransport;

	if (transport->GatewayEnabled)
	{
		if (!status && settings->GatewayHttpTransport)
		{
			transport->rdg = rdg_new(context);

			if (!transport->rdg)
				return FALSE;

			status = rdg_connect(transport->rdg, timeout, &rpcFallback);

			if (status)
			{
				transport->frontBio = rdg_get_front_bio_and_take_ownership(transport->rdg);
				WINPR_ASSERT(transport->frontBio);
				BIO_set_nonblock(transport->frontBio, 0);
				transport->layer = TRANSPORT_LAYER_TSG;
				status = TRUE;
			}
			else
			{
				rdg_free(transport->rdg);
				transport->rdg = NULL;
			}
		}

		if (!status && settings->GatewayRpcTransport && rpcFallback)
		{
			transport->tsg = tsg_new(transport);

			if (!transport->tsg)
				return FALSE;

			/* Reset error condition from RDG */
			freerdp_set_last_error_log(context, FREERDP_ERROR_SUCCESS);
			status = tsg_connect(transport->tsg, hostname, port, timeout);

			if (status)
			{
				transport->frontBio = tsg_get_bio(transport->tsg);
				transport->layer = TRANSPORT_LAYER_TSG;
				status = TRUE;
			}
			else
			{
				tsg_free(transport->tsg);
				transport->tsg = NULL;
			}
		}
	}
	else
	{
		UINT16 peerPort;
		const char *proxyHostname, *proxyUsername, *proxyPassword;
		BOOL isProxyConnection =
		    proxy_prepare(settings, &proxyHostname, &peerPort, &proxyUsername, &proxyPassword);

		if (isProxyConnection)
			sockfd = transport_tcp_connect(transport, proxyHostname, peerPort, timeout);
		else
			sockfd = transport_tcp_connect(transport, hostname, port, timeout);

		if (sockfd < 0)
			return FALSE;

		if (!transport_attach(transport, sockfd))
			return FALSE;

		if (isProxyConnection)
		{
			if (!proxy_connect(settings, transport->frontBio, proxyUsername, proxyPassword,
			                   hostname, port))
				return FALSE;
		}

		status = TRUE;
	}

	return status;
}

BOOL transport_accept_rdp(rdpTransport* transport)
{
	if (!transport)
		return FALSE;
	/* RDP encryption */
	return TRUE;
}

BOOL transport_accept_tls(rdpTransport* transport)
{
	if (!transport)
		return FALSE;
	return IFCALLRESULT(FALSE, transport->io.TLSAccept, transport);
}

static BOOL transport_default_accept_tls(rdpTransport* transport)
{
	rdpContext* context = transport_get_context(transport);
	rdpSettings* settings;

	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!transport->tls)
		transport->tls = tls_new(settings);

	transport->layer = TRANSPORT_LAYER_TLS;

	if (!tls_accept(transport->tls, transport->frontBio, settings))
		return FALSE;

	transport->frontBio = transport->tls->bio;
	return TRUE;
}

BOOL transport_accept_nla(rdpTransport* transport)
{
	rdpContext* context = transport_get_context(transport);
	rdpSettings* settings;

	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!IFCALLRESULT(FALSE, transport->io.TLSAccept, transport))
		return FALSE;

	/* Network Level Authentication */

	if (!settings->Authentication)
		return TRUE;

	if (!transport->nla)
	{
		transport->nla = nla_new(context, transport);
		transport_set_nla_mode(transport, TRUE);
	}

	if (nla_authenticate(transport->nla) < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR, "client authentication failure");
		transport_set_nla_mode(transport, FALSE);
		nla_free(transport->nla);
		transport->nla = NULL;
		tls_set_alert_code(transport->tls, TLS_ALERT_LEVEL_FATAL,
		                   TLS_ALERT_DESCRIPTION_ACCESS_DENIED);
		tls_send_alert(transport->tls);
		return FALSE;
	}

	/* don't free nla module yet, we need to copy the credentials from it first */
	transport_set_nla_mode(transport, FALSE);
	return TRUE;
}

#define WLog_ERR_BIO(transport, biofunc, bio) \
	transport_bio_error_log(transport, biofunc, bio, __FILE__, __FUNCTION__, __LINE__)

static void transport_bio_error_log(rdpTransport* transport, LPCSTR biofunc, BIO* bio, LPCSTR file,
                                    LPCSTR func, DWORD line)
{
	unsigned long sslerr;
	int saveerrno;
	DWORD level;

	WINPR_ASSERT(transport);

	saveerrno = errno;
	level = WLOG_ERROR;

	if (level < WLog_GetLogLevel(transport->log))
		return;

	if (ERR_peek_error() == 0)
	{
		const char* fmt = "%s returned a system error %d: %s";
		WLog_PrintMessage(transport->log, WLOG_MESSAGE_TEXT, level, line, file, func, fmt, biofunc,
		                  saveerrno, strerror(saveerrno));
		return;
	}

	while ((sslerr = ERR_get_error()))
	{
		char buf[120] = { 0 };
		const char* fmt = "%s returned an error: %s";

		ERR_error_string_n(sslerr, buf, 120);
		WLog_PrintMessage(transport->log, WLOG_MESSAGE_TEXT, level, line, file, func, fmt, biofunc,
		                  buf);
	}
}

static SSIZE_T transport_read_layer(rdpTransport* transport, BYTE* data, size_t bytes)
{
	SSIZE_T read = 0;
	rdpRdp* rdp;
	rdpContext* context;

	WINPR_ASSERT(transport);

	context = transport_get_context(transport);
	WINPR_ASSERT(context);

	rdp = context->rdp;
	WINPR_ASSERT(rdp);

	if (!transport->frontBio || (bytes > SSIZE_MAX))
	{
		transport->layer = TRANSPORT_LAYER_CLOSED;
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
		return -1;
	}

	while (read < (SSIZE_T)bytes)
	{
		const SSIZE_T tr = (SSIZE_T)bytes - read;
		int r = (int)((tr > INT_MAX) ? INT_MAX : tr);
		ERR_clear_error();
		int status = BIO_read(transport->frontBio, data + read, r);

		if (freerdp_shall_disconnect_context(context))
			return -1;

		if (status <= 0)
		{
			if (!transport->frontBio || !BIO_should_retry(transport->frontBio))
			{
				/* something unexpected happened, let's close */
				if (!transport->frontBio)
				{
					WLog_Print(transport->log, WLOG_ERROR, "BIO_read: transport->frontBio null");
					return -1;
				}

				WLog_ERR_BIO(transport, "BIO_read", transport->frontBio);
				transport->layer = TRANSPORT_LAYER_CLOSED;
				freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
				return -1;
			}

			/* non blocking will survive a partial read */
			if (!transport->blocking)
				return read;

			/* blocking means that we can't continue until we have read the number of requested
			 * bytes */
			if (BIO_wait_read(transport->frontBio, 100) < 0)
			{
				WLog_ERR_BIO(transport, "BIO_wait_read", transport->frontBio);
				return -1;
			}

			continue;
		}

#ifdef FREERDP_HAVE_VALGRIND_MEMCHECK_H
		VALGRIND_MAKE_MEM_DEFINED(data + read, bytes - read);
#endif
		read += status;
		rdp->inBytes += status;
	}

	return read;
}

/**
 * @brief Tries to read toRead bytes from the specified transport
 *
 * Try to read toRead bytes from the transport to the stream.
 * In case it was not possible to read toRead bytes 0 is returned. The stream is always advanced by
 * the number of bytes read.
 *
 * The function assumes that the stream has enough capacity to hold the data.
 *
 * @param[in] transport rdpTransport
 * @param[in] s wStream
 * @param[in] toRead number of bytes to read
 * @return < 0 on error; 0 if not enough data is available (non blocking mode); 1 toRead bytes read
 */
static SSIZE_T transport_read_layer_bytes(rdpTransport* transport, wStream* s, size_t toRead)
{
	SSIZE_T status;
	if (!transport)
		return -1;

	if (toRead > SSIZE_MAX)
		return 0;

	status = IFCALLRESULT(-1, transport->io.ReadBytes, transport, Stream_Pointer(s), toRead);

	if (status <= 0)
		return status;

	Stream_Seek(s, (size_t)status);
	return status == (SSIZE_T)toRead ? 1 : 0;
}

/**
 * @brief Try to read a complete PDU (NLA, fast-path or tpkt) from the underlying transport.
 *
 * If possible a complete PDU is read, in case of non blocking transport this might not succeed.
 * Except in case of an error the passed stream will point to the last byte read (correct
 * position). When the pdu read is completed the stream is sealed and the pointer set to 0
 *
 * @param[in] transport rdpTransport
 * @param[in] s wStream
 * @return < 0 on error; 0 if not enough data is available (non blocking mode); > 0 number of
 * bytes of the *complete* pdu read
 */
int transport_read_pdu(rdpTransport* transport, wStream* s)
{
	if (!transport)
		return -1;
	return IFCALLRESULT(-1, transport->io.ReadPdu, transport, s);
}

SSIZE_T transport_parse_pdu(rdpTransport* transport, wStream* s, BOOL* incomplete)
{
	size_t position;
	size_t pduLength;
	BYTE* header;
	pduLength = 0;

	if (!transport)
		return -1;

	if (!s)
		return -1;

	header = Stream_Buffer(s);
	position = Stream_GetPosition(s);

	if (incomplete)
		*incomplete = TRUE;

	/* Make sure at least two bytes are read for further processing */
	if (position < 2)
	{
		/* No data available at the moment */
		return 0;
	}

	if (transport->NlaMode)
	{
		/*
		 * In case NlaMode is set TSRequest package(s) are expected
		 * 0x30 = DER encoded data with these bits set:
		 * bit 6 P/C constructed
		 * bit 5 tag number - sequence
		 */
		if (header[0] == 0x30)
		{
			/* TSRequest (NLA) */
			if (header[1] & 0x80)
			{
				if ((header[1] & ~(0x80)) == 1)
				{
					/* check for header bytes already was readed in previous calls */
					if (position < 3)
						return 0;

					pduLength = header[2];
					pduLength += 3;
				}
				else if ((header[1] & ~(0x80)) == 2)
				{
					/* check for header bytes already was readed in previous calls */
					if (position < 4)
						return 0;

					pduLength = (header[2] << 8) | header[3];
					pduLength += 4;
				}
				else
				{
					WLog_Print(transport->log, WLOG_ERROR, "Error reading TSRequest!");
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
			/* check for header bytes already was readed in previous calls */
			if (position < 4)
				return 0;

			pduLength = (header[2] << 8) | header[3];

			/* min and max values according to ITU-T Rec. T.123 (01/2007) section 8 */
			if ((pduLength < 7) || (pduLength > 0xFFFF))
			{
				WLog_Print(transport->log, WLOG_ERROR, "tpkt - invalid pduLength: %" PRIdz,
				           pduLength);
				return -1;
			}
		}
		else
		{
			/* Fast-Path Header */
			if (header[1] & 0x80)
			{
				/* check for header bytes already was readed in previous calls */
				if (position < 3)
					return 0;

				pduLength = ((header[1] & 0x7F) << 8) | header[2];
			}
			else
				pduLength = header[1];

			/*
			 * fast-path has 7 bits for length so the maximum size, including headers is 0x8000
			 * The theoretical minimum fast-path PDU consists only of two header bytes plus one
			 * byte for data (e.g. fast-path input synchronize pdu)
			 */
			if (pduLength < 3 || pduLength > 0x8000)
			{
				WLog_Print(transport->log, WLOG_ERROR, "fast path - invalid pduLength: %" PRIdz,
				           pduLength);
				return -1;
			}
		}
	}

	if (position > pduLength)
		return -1;

	if (incomplete)
		*incomplete = position < pduLength;

	return pduLength;
}

static int transport_default_read_pdu(rdpTransport* transport, wStream* s)
{
	BOOL incomplete;
	SSIZE_T status;
	size_t pduLength;
	size_t position;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(s);

	/* Read in pdu length */
	status = transport_parse_pdu(transport, s, &incomplete);
	while ((status == 0) && incomplete)
	{
		int rc;
		if (!Stream_EnsureRemainingCapacity(s, 1))
			return -1;
		rc = transport_read_layer_bytes(transport, s, 1);
		if (rc != 1)
			return rc;
		status = transport_parse_pdu(transport, s, &incomplete);
	}

	if (status < 0)
		return -1;

	pduLength = (size_t)status;

	/* Read in rest of the PDU */
	if (!Stream_EnsureCapacity(s, pduLength))
		return -1;

	position = Stream_GetPosition(s);
	if (position > pduLength)
		return -1;

	status = transport_read_layer_bytes(transport, s, pduLength - Stream_GetPosition(s));

	if (status != 1)
		return status;

	if (Stream_GetPosition(s) >= pduLength)
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), pduLength, WLOG_PACKET_INBOUND);

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);
	return Stream_Length(s);
}

int transport_write(rdpTransport* transport, wStream* s)
{
	if (!transport)
		return -1;

	return IFCALLRESULT(-1, transport->io.WritePdu, transport, s);
}

static int transport_default_write(rdpTransport* transport, wStream* s)
{
	size_t length;
	int status = -1;
	int writtenlength = 0;
	rdpRdp* rdp;
	rdpContext* context = transport_get_context(transport);

	WINPR_ASSERT(transport);
	WINPR_ASSERT(context);

	if (!s)
		return -1;

	Stream_AddRef(s);

	rdp = context->rdp;
	if (!rdp)
		goto fail;

	EnterCriticalSection(&(transport->WriteLock));
	if (!transport->frontBio)
		goto out_cleanup;

	length = Stream_GetPosition(s);
	writtenlength = length;
	Stream_SetPosition(s, 0);

	if (length > 0)
	{
		rdp->outBytes += length;
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), length, WLOG_PACKET_OUTBOUND);
	}

	while (length > 0)
	{
		ERR_clear_error();
		status = BIO_write(transport->frontBio, Stream_Pointer(s), length);

		if (status <= 0)
		{
			/* the buffered BIO that is at the end of the chain always says OK for writing,
			 * so a retry means that for any reason we need to read. The most probable
			 * is a SSL or TSG BIO in the chain.
			 */
			if (!BIO_should_retry(transport->frontBio))
			{
				WLog_ERR_BIO(transport, "BIO_should_retry", transport->frontBio);
				goto out_cleanup;
			}

			/* non-blocking can live with blocked IOs */
			if (!transport->blocking)
			{
				WLog_ERR_BIO(transport, "BIO_write", transport->frontBio);
				goto out_cleanup;
			}

			if (BIO_wait_write(transport->frontBio, 100) < 0)
			{
				WLog_ERR_BIO(transport, "BIO_wait_write", transport->frontBio);
				status = -1;
				goto out_cleanup;
			}

			continue;
		}

		WINPR_ASSERT(context->settings);
		if (transport->blocking || context->settings->WaitForOutputBufferFlush)
		{
			while (BIO_write_blocked(transport->frontBio))
			{
				if (BIO_wait_write(transport->frontBio, 100) < 0)
				{
					WLog_Print(transport->log, WLOG_ERROR, "error when selecting for write");
					status = -1;
					goto out_cleanup;
				}

				if (BIO_flush(transport->frontBio) < 1)
				{
					WLog_Print(transport->log, WLOG_ERROR, "error when flushing outputBuffer");
					status = -1;
					goto out_cleanup;
				}
			}
		}

		length -= status;
		Stream_Seek(s, status);
	}

	transport->written += writtenlength;
out_cleanup:

	if (status < 0)
	{
		/* A write error indicates that the peer has dropped the connection */
		transport->layer = TRANSPORT_LAYER_CLOSED;
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
	}

	LeaveCriticalSection(&(transport->WriteLock));
fail:
	Stream_Release(s);
	return status;
}

DWORD transport_get_event_handles(rdpTransport* transport, HANDLE* events, DWORD count)
{
	DWORD nCount = 0; /* always the reread Event */
	DWORD tmp;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(events);
	WINPR_ASSERT(count > 0);

	if (events)
	{
		if (count < 1)
		{
			WLog_Print(transport->log, WLOG_ERROR, "%s: provided handles array is too small",
			           __FUNCTION__);
			return 0;
		}

		events[nCount++] = transport->rereadEvent;

		if (transport->useIoEvent)
		{
			if (count < 2)
				return 0;
			events[nCount++] = transport->ioEvent;
		}
	}

	if (!transport->GatewayEnabled)
	{
		if (events)
		{
			if (nCount >= count)
			{
				WLog_Print(transport->log, WLOG_ERROR,
				           "%s: provided handles array is too small (count=%" PRIu32
				           " nCount=%" PRIu32 ")",
				           __FUNCTION__, count, nCount);
				return 0;
			}

			if (transport->frontBio)
			{
				if (BIO_get_event(transport->frontBio, &events[nCount]) != 1)
				{
					WLog_Print(transport->log, WLOG_ERROR, "%s: error getting the frontBio handle",
					           __FUNCTION__);
					return 0;
				}
				nCount++;
			}
		}
	}
	else
	{
		if (transport->rdg)
		{
			tmp = rdg_get_event_handles(transport->rdg, &events[nCount], count - nCount);

			if (tmp == 0)
				return 0;

			nCount += tmp;
		}
		else if (transport->tsg)
		{
			tmp = tsg_get_event_handles(transport->tsg, &events[nCount], count - nCount);

			if (tmp == 0)
				return 0;

			nCount += tmp;
		}
	}

	return nCount;
}

#if defined(WITH_FREERDP_DEPRECATED)
void transport_get_fds(rdpTransport* transport, void** rfds, int* rcount)
{
	DWORD index;
	DWORD nCount;
	HANDLE events[MAXIMUM_WAIT_OBJECTS] = { 0 };

	WINPR_ASSERT(transport);
	WINPR_ASSERT(rfds);
	WINPR_ASSERT(rcount);

	nCount = transport_get_event_handles(transport, events, ARRAYSIZE(events));
	*rcount = nCount + 1;

	for (index = 0; index < nCount; index++)
	{
		rfds[index] = GetEventWaitObject(events[index]);
	}

	rfds[nCount] = GetEventWaitObject(transport->rereadEvent);
}
#endif

BOOL transport_is_write_blocked(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	WINPR_ASSERT(transport->frontBio);
	return BIO_write_blocked(transport->frontBio);
}

int transport_drain_output_buffer(rdpTransport* transport)
{
	BOOL status = FALSE;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(transport->frontBio);
	if (BIO_write_blocked(transport->frontBio))
	{
		if (BIO_flush(transport->frontBio) < 1)
			return -1;

		status |= BIO_write_blocked(transport->frontBio);
	}

	return status;
}

int transport_check_fds(rdpTransport* transport)
{
	int status;
	state_run_t recv_status;
	wStream* received;
	rdpContext* context = transport_get_context(transport);

	WINPR_ASSERT(context);

	if (transport->layer == TRANSPORT_LAYER_CLOSED)
	{
		WLog_Print(transport->log, WLOG_DEBUG, "transport_check_fds: transport layer closed");
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
		return -1;
	}

	/**
	 * Note: transport_read_pdu tries to read one PDU from
	 * the transport layer.
	 * The ReceiveBuffer might have a position > 0 in case of a non blocking
	 * transport. If transport_read_pdu returns 0 the pdu couldn't be read at
	 * this point.
	 * Note that transport->ReceiveBuffer is replaced after each iteration
	 * of this loop with a fresh stream instance from a pool.
	 */
	if ((status = transport_read_pdu(transport, transport->ReceiveBuffer)) <= 0)
	{
		if (status < 0)
			WLog_Print(transport->log, WLOG_DEBUG, "transport_check_fds: transport_read_pdu() - %i",
			           status);
		if (transport->haveMoreBytesToRead)
		{
			transport->haveMoreBytesToRead = FALSE;
			ResetEvent(transport->rereadEvent);
		}
		return status;
	}

	received = transport->ReceiveBuffer;

	if (!(transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0)))
		return -1;

	/**
	 * status:
	 * 	-1: error
	 * 	 0: success
	 * 	 1: redirection
	 */
	WINPR_ASSERT(transport->ReceiveCallback);
	recv_status = transport->ReceiveCallback(transport, received, transport->ReceiveExtra);
	Stream_Release(received);

	if (state_run_failed(recv_status))
	{
		char buffer[64] = { 0 };
		WLog_Print(transport->log, WLOG_ERROR,
		           "transport_check_fds: transport->ReceiveCallback() - %s",
		           state_run_result_string(recv_status, buffer, ARRAYSIZE(buffer)));
		return -1;
	}

	/* Run this again to be sure we consumed all input data.
	 * This will be repeated until a (not fully) received packet is in buffer
	 */
	if (!transport->haveMoreBytesToRead)
	{
		transport->haveMoreBytesToRead = TRUE;
		SetEvent(transport->rereadEvent);
	}
	return recv_status;
}

BOOL transport_set_blocking_mode(rdpTransport* transport, BOOL blocking)
{
	WINPR_ASSERT(transport);

	transport->blocking = blocking;

	if (transport->frontBio)
	{
		if (!BIO_set_nonblock(transport->frontBio, blocking ? FALSE : TRUE))
			return FALSE;
	}

	return TRUE;
}

void transport_set_gateway_enabled(rdpTransport* transport, BOOL GatewayEnabled)
{
	WINPR_ASSERT(transport);
	transport->GatewayEnabled = GatewayEnabled;
}

void transport_set_nla_mode(rdpTransport* transport, BOOL NlaMode)
{
	WINPR_ASSERT(transport);
	transport->NlaMode = NlaMode;
}

BOOL transport_disconnect(rdpTransport* transport)
{
	if (!transport)
		return FALSE;
	return IFCALLRESULT(FALSE, transport->io.TransportDisconnect, transport);
}

static BOOL transport_default_disconnect(rdpTransport* transport)
{
	BOOL status = TRUE;

	if (!transport)
		return FALSE;

	if (transport->tls)
	{
		tls_free(transport->tls);
		transport->tls = NULL;
	}
	else
	{
		if (transport->frontBio)
			BIO_free_all(transport->frontBio);
	}

	if (transport->tsg)
	{
		tsg_free(transport->tsg);
		transport->tsg = NULL;
	}

	if (transport->rdg)
	{
		rdg_free(transport->rdg);
		transport->rdg = NULL;
	}

	transport->frontBio = NULL;
	transport->layer = TRANSPORT_LAYER_TCP;
	return status;
}

rdpTransport* transport_new(rdpContext* context)
{
	rdpTransport* transport = (rdpTransport*)calloc(1, sizeof(rdpTransport));

	WINPR_ASSERT(context);
	if (!transport)
		return NULL;

	transport->log = WLog_Get(TAG);

	if (!transport->log)
		goto fail;

	// transport->io.DataHandler = transport_data_handler;
	transport->io.TCPConnect = freerdp_tcp_default_connect;
	transport->io.TLSConnect = transport_default_connect_tls;
	transport->io.TLSAccept = transport_default_accept_tls;
	transport->io.TransportAttach = transport_default_attach;
	transport->io.TransportDisconnect = transport_default_disconnect;
	transport->io.ReadPdu = transport_default_read_pdu;
	transport->io.WritePdu = transport_default_write;
	transport->io.ReadBytes = transport_read_layer;

	transport->context = context;
	transport->ReceivePool = StreamPool_New(TRUE, BUFFER_SIZE);

	if (!transport->ReceivePool)
		goto fail;

	/* receive buffer for non-blocking read. */
	transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0);

	if (!transport->ReceiveBuffer)
		goto fail;

	transport->connectedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->connectedEvent || transport->connectedEvent == INVALID_HANDLE_VALUE)
		goto fail;

	transport->rereadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->rereadEvent || transport->rereadEvent == INVALID_HANDLE_VALUE)
		goto fail;

	transport->ioEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->ioEvent || transport->ioEvent == INVALID_HANDLE_VALUE)
		goto fail;

	transport->haveMoreBytesToRead = FALSE;
	transport->blocking = TRUE;
	transport->GatewayEnabled = FALSE;
	transport->layer = TRANSPORT_LAYER_TCP;

	if (!InitializeCriticalSectionAndSpinCount(&(transport->ReadLock), 4000))
		goto fail;

	if (!InitializeCriticalSectionAndSpinCount(&(transport->WriteLock), 4000))
		goto fail;

	return transport;
fail:
	transport_free(transport);
	return NULL;
}

void transport_free(rdpTransport* transport)
{
	if (!transport)
		return;

	transport_disconnect(transport);

	if (transport->ReceiveBuffer)
		Stream_Release(transport->ReceiveBuffer);

	nla_free(transport->nla);
	StreamPool_Free(transport->ReceivePool);
	CloseHandle(transport->connectedEvent);
	CloseHandle(transport->rereadEvent);
	CloseHandle(transport->ioEvent);
	DeleteCriticalSection(&(transport->ReadLock));
	DeleteCriticalSection(&(transport->WriteLock));
	free(transport);
}

BOOL transport_set_io_callbacks(rdpTransport* transport, const rdpTransportIo* io_callbacks)
{
	if (!transport || !io_callbacks)
		return FALSE;

	transport->io = *io_callbacks;
	return TRUE;
}

const rdpTransportIo* transport_get_io_callbacks(rdpTransport* transport)
{
	if (!transport)
		return NULL;
	return &transport->io;
}

rdpContext* transport_get_context(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return transport->context;
}

rdpTransport* freerdp_get_transport(rdpContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);
	return context->rdp->transport;
}

BOOL transport_set_nla(rdpTransport* transport, rdpNla* nla)
{
	WINPR_ASSERT(transport);
	nla_free(transport->nla);
	transport->nla = nla;
	return TRUE;
}

rdpNla* transport_get_nla(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return transport->nla;
}

BOOL transport_set_tls(rdpTransport* transport, rdpTls* tls)
{
	WINPR_ASSERT(transport);
	tls_free(transport->tls);
	transport->tls = tls;
	return TRUE;
}

rdpTls* transport_get_tls(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return transport->tls;
}

BOOL transport_set_tsg(rdpTransport* transport, rdpTsg* tsg)
{
	WINPR_ASSERT(transport);
	tsg_free(transport->tsg);
	transport->tsg = tsg;
	return TRUE;
}

rdpTsg* transport_get_tsg(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return transport->tsg;
}

wStream* transport_take_from_pool(rdpTransport* transport, size_t size)
{
	WINPR_ASSERT(transport);
	return StreamPool_Take(transport->ReceivePool, size);
}

ULONG transport_get_bytes_sent(rdpTransport* transport, BOOL resetCount)
{
	ULONG rc;
	WINPR_ASSERT(transport);
	rc = transport->written;
	if (resetCount)
		transport->written = 0;
	return rc;
}

TRANSPORT_LAYER transport_get_layer(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return transport->layer;
}

BOOL transport_set_layer(rdpTransport* transport, TRANSPORT_LAYER layer)
{
	WINPR_ASSERT(transport);
	transport->layer = layer;
	return TRUE;
}

BOOL transport_set_connected_event(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return SetEvent(transport->connectedEvent);
}

BOOL transport_set_recv_callbacks(rdpTransport* transport, TransportRecv recv, void* extra)
{
	WINPR_ASSERT(transport);
	transport->ReceiveCallback = recv;
	transport->ReceiveExtra = extra;
	return TRUE;
}

BOOL transport_get_blocking(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return transport->blocking;
}

BOOL transport_set_blocking(rdpTransport* transport, BOOL blocking)
{
	WINPR_ASSERT(transport);
	transport->blocking = blocking;
	return TRUE;
}

BOOL transport_have_more_bytes_to_read(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return transport->haveMoreBytesToRead;
}

int transport_tcp_connect(rdpTransport* transport, const char* hostname, int port, DWORD timeout)
{
	rdpContext* context = transport_get_context(transport);
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->settings);
	return IFCALLRESULT(-1, transport->io.TCPConnect, context, context->settings, hostname, port,
	                    timeout);
}

HANDLE transport_get_front_bio(rdpTransport* transport)
{
	HANDLE hEvent = NULL;
	WINPR_ASSERT(transport);
	WINPR_ASSERT(transport->frontBio);

	BIO_get_event(transport->frontBio, &hEvent);
	return hEvent;
}

BOOL transport_io_callback_set_event(rdpTransport* transport, BOOL set)
{
	WINPR_ASSERT(transport);
	transport->useIoEvent = TRUE;
	if (!set)
		return ResetEvent(transport->ioEvent);
	return SetEvent(transport->ioEvent);
}
