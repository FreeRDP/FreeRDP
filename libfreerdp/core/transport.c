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

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "tpkt.h"
#include "fastpath.h"
#include "transport.h"
#include "rdp.h"
#include "proxy.h"

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
};

#ifdef WITH_GSSAPI

#include <krb5.h>
#include <winpr/library.h>
static UINT32 transport_krb5_check_account(rdpTransport* transport, char* username, char* domain,
        char* passwd)
{
	krb5_error_code ret;
	krb5_context context = NULL;
	krb5_principal principal = NULL;
	char address[256];
	krb5_ccache ccache;
	krb5_init_creds_context ctx = NULL;
	_snprintf(address, sizeof(address), "%s@%s", username, domain);

	/* Create a krb5 library context */
	if ((ret = krb5_init_context(&context)) != 0)
		WLog_Print(transport->log, WLOG_ERROR, "krb5_init_context failed with error %d", (int)ret);
	else if ((ret = krb5_parse_name_flags(context, address, 0, &principal)) != 0)
		WLog_Print(transport->log, WLOG_ERROR, "krb5_parse_name_flags failed with error %d", (int)ret);
	/* Find a credential cache with a specified client principal */
	else if ((ret = krb5_cc_cache_match(context, principal, &ccache)) != 0)
	{
		if ((ret = krb5_cc_default(context, &ccache)) != 0)
			WLog_Print(transport->log, WLOG_ERROR, "krb5 failed to resolve credentials cache with error %d",
			           (int)ret);
	}

	if (ret != KRB5KDC_ERR_NONE)
		goto out;
	/* Create a context for acquiring initial credentials */
	else if ((ret = krb5_init_creds_init(context, principal, NULL, NULL, 0, NULL, &ctx)) != 0)
	{
		WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_init returned error %d", (int)ret);
		goto out;
	}
	/* Set a password for acquiring initial credentials */
	else if ((ret = krb5_init_creds_set_password(context, ctx, passwd)) != 0)
	{
		WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_set_password returned error %d", ret);
		goto out;
	}

	/* Acquire credentials using an initial credential context */
	ret = krb5_init_creds_get(context, ctx);
out:

	switch (ret)
	{
		case KRB5KDC_ERR_NONE:
			break;

		case KRB5_KDC_UNREACH:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get: KDC unreachable");
			ret = FREERDP_ERROR_CONNECT_KDC_UNREACHABLE;
			break;

		case KRB5KRB_AP_ERR_BAD_INTEGRITY:
		case KRB5KRB_AP_ERR_MODIFIED:
		case KRB5KDC_ERR_PREAUTH_FAILED:
		case KRB5_GET_IN_TKT_LOOP:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get: Password incorrect");
			ret = FREERDP_ERROR_AUTHENTICATION_FAILED;
			break;

		case KRB5KDC_ERR_KEY_EXP:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get: Password has expired");
			ret = FREERDP_ERROR_CONNECT_PASSWORD_EXPIRED;
			break;

		case KRB5KDC_ERR_CLIENT_REVOKED:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get: Password revoked");
			ret = FREERDP_ERROR_CONNECT_CLIENT_REVOKED;
			break;

		case KRB5KDC_ERR_POLICY:
			ret = FREERDP_ERROR_INSUFFICIENT_PRIVILEGES;
			break;

		default:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get");
			ret = FREERDP_ERROR_CONNECT_TRANSPORT_FAILED;
			break;
	}

	if (ctx)
		krb5_init_creds_free(context, ctx);

	krb5_free_context(context);
	return ret;
}
#endif /* WITH_GSSAPI */

static void transport_ssl_cb(SSL* ssl, int where, int ret)
{
	if (where & SSL_CB_ALERT)
	{
		rdpSettings* settings;
		rdpTransport* transport = (rdpTransport*) SSL_get_app_data(ssl);

		if (!transport || !transport->context || !transport->context->settings)
			return;

		settings = transport->context->settings;

		switch (ret)
		{
			case (SSL3_AL_FATAL << 8) | SSL_AD_ACCESS_DENIED:
				{
					if (!freerdp_get_last_error(transport->context))
					{
						WLog_Print(transport->log, WLOG_ERROR, "%s: ACCESS DENIED", __FUNCTION__);
						freerdp_set_last_error(transport->context, FREERDP_ERROR_AUTHENTICATION_FAILED);
					}
				}
				break;

			case (SSL3_AL_FATAL << 8) | SSL_AD_INTERNAL_ERROR:
				{
					if (transport->NlaMode)
					{
						UINT32 kret = 0;
#ifdef WITH_GSSAPI

						if ((strlen(settings->Domain) != 0) &&
						    (strncmp(settings->Domain, ".", 1) != 0))
						{
							kret = transport_krb5_check_account(transport, settings->Username,
							                                    settings->Domain,
							                                    settings->Password);
						}
						else
#endif /* WITH_GSSAPI */
							kret = FREERDP_ERROR_CONNECT_PASSWORD_CERTAINLY_EXPIRED;

						if (!freerdp_get_last_error(transport->context))
							freerdp_set_last_error(transport->context, kret);
					}

					break;

				case (SSL3_AL_WARNING << 8) | SSL3_AD_CLOSE_NOTIFY:
					break;

				default:
					WLog_Print(transport->log, WLOG_WARN, "Unhandled SSL error (where=%d, ret=%d [%s, %s])", where, ret,
					           SSL_alert_type_string_long(ret), SSL_alert_desc_string_long(ret));
					break;
				}
		}
	}
}

wStream* transport_send_stream_init(rdpTransport* transport, int size)
{
	wStream* s;

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
	BIO* socketBio;
	BIO* bufferedBio;
	socketBio = BIO_new(BIO_s_simple_socket());

	if (!socketBio)
		return FALSE;

	BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
		return FALSE;

	bufferedBio = BIO_push(bufferedBio, socketBio);
	transport->frontBio = bufferedBio;
	return TRUE;
}

BOOL transport_connect_rdp(rdpTransport* transport)
{
	/* RDP encryption */
	return TRUE;
}

BOOL transport_connect_tls(rdpTransport* transport)
{
	int tlsStatus;
	rdpTls* tls = NULL;
	rdpContext* context;
	rdpSettings* settings;

	if (!transport || !transport->context || !transport->context->settings)
		return FALSE;

	context = transport->context;
	settings = context->settings;

	if (!(tls = tls_new(settings)))
		return FALSE;

	transport->tls = tls;

	if (!transport_set_layer_state(transport,
	                               transport->GatewayEnabled ? TRANSPORT_LAYER_TSG_TLS : TRANSPORT_LAYER_TLS))
		return FALSE;

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

	transport->frontBio = tls->bio;
	BIO_callback_ctrl(tls->bio, BIO_CTRL_SET_CALLBACK,
	                  (bio_info_cb*) transport_ssl_cb);
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
	rdpContext* context;
	rdpSettings* settings;
	freerdp* instance ;
	rdpRdp* rdp;

	if (!transport || !transport->context)
		return FALSE;

	context = transport->context;
	settings = context->settings;
	instance = context->instance;
	rdp = context->rdp;

	if (!transport_connect_tls(transport))
		return FALSE;

	if (!settings->Authentication)
		return TRUE;

	rdp->nla = nla_new(instance, transport, settings);

	if (!rdp->nla)
		return FALSE;

	transport_set_nla_mode(transport, TRUE);

	if (settings->AuthenticationServiceClass)
	{
		if (!nla_set_service_principal_name(rdp->nla, settings->AuthenticationServiceClass,
		                                    settings->ServerHostname))
			return FALSE;
	}

	if (nla_client_begin(rdp->nla) < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR, "NLA begin failed");

		if (!freerdp_get_last_error(context))
			freerdp_set_last_error(context, FREERDP_ERROR_AUTHENTICATION_FAILED);

		transport_set_nla_mode(transport, FALSE);
		return FALSE;
	}

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_NLA);
	return TRUE;
}

BOOL transport_connect(rdpTransport* transport, const char* hostname,
                       UINT16 port, int timeout)
{
	int sockfd;
	BOOL status = FALSE;
	rdpSettings* settings;
	rdpContext* context;
	BOOL rpcFallback;

	if (!transport || !transport->context || !transport->context->settings)
		return FALSE;

	context = transport->context;
	settings = context->settings;
	rpcFallback = !settings->GatewayHttpTransport;

	if (transport->GatewayEnabled)
	{
		if (!status && settings->GatewayHttpTransport)
		{
			transport->rdg = rdg_new(transport);

			if (!transport->rdg)
				return FALSE;

			status = rdg_connect(transport->rdg, timeout, &rpcFallback);

			if (status)
			{
				transport->frontBio = transport->rdg->frontBio;
				BIO_set_nonblock(transport->frontBio, 0);
				status = transport_set_layer_state(transport, TRANSPORT_LAYER_TSG);
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

			status = tsg_connect(transport->tsg, hostname, port, timeout);

			if (status)
			{
				transport->frontBio = transport->tsg->bio;
				status = transport_set_layer_state(transport, TRANSPORT_LAYER_TSG);
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
		const char* proxyHostname, *proxyUsername, *proxyPassword;
		BOOL isProxyConnection = proxy_prepare(settings, &proxyHostname, &peerPort,
		                                       &proxyUsername,	&proxyPassword);

		if (isProxyConnection)
			sockfd = freerdp_tcp_connect(context, settings, proxyHostname, peerPort, timeout);
		else
			sockfd = freerdp_tcp_connect(context, settings, hostname, port, timeout);

		if (sockfd < 0)
			return FALSE;

		if (!transport_attach(transport, sockfd))
			return FALSE;

		if (isProxyConnection)
		{
			if (!proxy_connect(settings, transport->frontBio, proxyUsername, proxyPassword, hostname, port))
				return FALSE;
		}

		status = TRUE;
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
	rdpSettings* settings;

	if (!transport || !transport->context || !transport->context->settings)
		return FALSE;

	settings = transport->context->settings;

	if (!transport->tls)
		transport->tls = tls_new(transport->context->settings);

	if (!transport_set_layer_state(transport, TRANSPORT_LAYER_TLS))
		return FALSE;

	if (!tls_accept(transport->tls, transport->frontBio, settings))
		return FALSE;

	transport->frontBio = transport->tls->bio;
	return TRUE;
}

BOOL transport_accept_nla(rdpTransport* transport)
{
	freerdp* instance;
	rdpSettings* settings;

	if (!transport || !transport->context || !transport->context->settings)
		return FALSE;

	settings = transport->context->settings;
	instance = transport->context->instance;

	if (!transport->tls)
		transport->tls = tls_new(settings);

	if (!transport_set_layer_state(transport, TRANSPORT_LAYER_TLS))
		return FALSE;

	if (!tls_accept(transport->tls, transport->frontBio, settings))
		return FALSE;

	transport->frontBio = transport->tls->bio;

	/* Network Level Authentication */

	if (!settings->Authentication)
		return TRUE;

	if (!transport->nla)
	{
		transport->nla = nla_new(instance, transport, settings);
		transport_set_nla_mode(transport, TRUE);
	}

	if (nla_authenticate(transport->nla) < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR, "client authentication failure");
		transport_set_nla_mode(transport, FALSE);
		transport_free_nla(transport);
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

static void transport_bio_error_log(rdpTransport* transport, LPCSTR biofunc, BIO* bio,
                                    LPCSTR file, LPCSTR func, DWORD line)
{
	unsigned long sslerr;
	char* buf;
	int saveerrno;
	DWORD level;
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

	buf = malloc(120);

	if (buf)
	{
		const char* fmt = "%s returned an error: %s";

		while ((sslerr = ERR_get_error()))
		{
			ERR_error_string_n(sslerr, buf, 120);
			WLog_PrintMessage(transport->log, WLOG_MESSAGE_TEXT, level, line, file, func, fmt, biofunc,
			                  buf);
		}

		free(buf);
	}
}

static int transport_read_layer(rdpTransport* transport, BYTE* data, int bytes)
{
	int read = 0;
	int status = -1;

	if (!transport->frontBio)
	{
		transport_set_layer_state(transport, TRANSPORT_LAYER_CLOSED);
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
				if (!transport->frontBio)
				{
					WLog_Print(transport->log, WLOG_ERROR, "BIO_read: transport->frontBio null");
					return -1;
				}

				WLog_ERR_BIO(transport, "BIO_read", transport->frontBio);
				transport_set_layer_state(transport, TRANSPORT_LAYER_CLOSED);
				return -1;
			}

			/* non blocking will survive a partial read */
			if (!transport->blocking)
				return read;

			/* blocking means that we can't continue until we have read the number of requested bytes */
			if (BIO_wait_read(transport->frontBio, 100) < 0)
			{
				WLog_ERR_BIO(transport, "BIO_wait_read", transport->frontBio);
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


/**
 * @brief Tries to read toRead bytes from the specified transport
 *
 * Try to read toRead bytes from the transport to the stream.
 * In case it was not possible to read toRead bytes 0 is returned. The stream is always advanced by the
 * number of bytes read.
 *
 * The function assumes that the stream has enough capacity to hold the data.
 *
 * @param[in] transport rdpTransport
 * @param[in] s wStream
 * @param[in] toRead number of bytes to read
 * @return < 0 on error; 0 if not enough data is available (non blocking mode); 1 toRead bytes read
 */
static int transport_read_layer_bytes(rdpTransport* transport, wStream* s,
                                      unsigned int toRead)
{
	int status;
	status = transport_read_layer(transport, Stream_Pointer(s), toRead);

	if (status <= 0)
		return status;

	Stream_Seek(s, status);
	return status == toRead ? 1 : 0;
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
	int status;
	size_t position;
	int pduLength;
	BYTE* header;
	pduLength = 0;

	if (!transport)
		return -1;

	if (!s)
		return -1;

	position = Stream_GetPosition(s);

	/* Make sure there is enough space for the longest header within the stream */
	if (!Stream_EnsureCapacity(s, 4))
		return -1;

	/* Make sure at least two bytes are read for further processing */
	if (position < 2
	    && (status = transport_read_layer_bytes(transport, s, 2 - position)) != 1)
	{
		/* No data available at the moment */
		return status;
	}

	/* update position value for further checks */
	position = Stream_GetPosition(s);
	header = Stream_Buffer(s);

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
					if (position < 3
					    && (status = transport_read_layer_bytes(transport, s, 3 - position)) != 1)
						return status;

					pduLength = header[2];
					pduLength += 3;
				}
				else if ((header[1] & ~(0x80)) == 2)
				{
					/* check for header bytes already was readed in previous calls */
					if (position < 4
					    && (status = transport_read_layer_bytes(transport, s, 4 - position)) != 1)
						return status;

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
			if (position < 4
			    && (status = transport_read_layer_bytes(transport, s, 4 - position)) != 1)
				return status;

			pduLength = (header[2] << 8) | header[3];

			/* min and max values according to ITU-T Rec. T.123 (01/2007) section 8 */
			if (pduLength < 7 || pduLength > 0xFFFF)
			{
				WLog_Print(transport->log, WLOG_ERROR, "tpkt - invalid pduLength: %d", pduLength);
				return -1;
			}
		}
		else
		{
			/* Fast-Path Header */
			if (header[1] & 0x80)
			{
				/* check for header bytes already was readed in previous calls */
				if (position < 3
				    && (status = transport_read_layer_bytes(transport, s, 3 - position)) != 1)
					return status;

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
				WLog_Print(transport->log, WLOG_ERROR, "fast path - invalid pduLength: %d", pduLength);
				return -1;
			}
		}
	}

	if (!Stream_EnsureCapacity(s, Stream_GetPosition(s) + pduLength))
		return -1;

	status = transport_read_layer_bytes(transport, s, pduLength - Stream_GetPosition(s));

	if (status != 1)
		return status;

	if (Stream_GetPosition(s) >= pduLength)
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), pduLength,
		            WLOG_PACKET_INBOUND);

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);
	return Stream_Length(s);
}

int transport_write(rdpTransport* transport, wStream* s)
{
	size_t length;
	int status = -1;
	int writtenlength = 0;

	if (!transport)
		return -1;

	if (!transport->frontBio)
	{
		transport_set_layer_state(transport, TRANSPORT_LAYER_CLOSED);
		return -1;
	}

	EnterCriticalSection(&(transport->WriteLock));
	length = Stream_GetPosition(s);
	writtenlength = length;
	Stream_SetPosition(s, 0);

	if (length > 0)
	{
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), length,
		            WLOG_PACKET_OUTBOUND);
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

		if (transport->blocking || transport->context->settings->WaitForOutputBufferFlush)
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
		transport_set_layer_state(transport, TRANSPORT_LAYER_CLOSED);
	}

	Stream_Release(s);
	LeaveCriticalSection(&(transport->WriteLock));
	return status;
}

DWORD transport_get_event_handles(rdpTransport* transport, HANDLE* events,
                                  DWORD count)
{
	DWORD nCount = 1; /* always the reread Event */
	DWORD tmp;

	if (events)
	{
		if (count < 1)
		{
			WLog_Print(transport->log, WLOG_ERROR, "%s: provided handles array is too small", __FUNCTION__);
			return 0;
		}

		events[0] = transport->rereadEvent;
	}

	if (!transport->GatewayEnabled)
	{
		nCount++;

		if (events)
		{
			if (nCount > count)
			{
				WLog_Print(transport->log, WLOG_ERROR,
				           "%s: provided handles array is too small (count=%"PRIu32" nCount=%"PRIu32")",
				           __FUNCTION__, count, nCount);
				return 0;
			}

			if (BIO_get_event(transport->frontBio, &events[1]) != 1)
			{
				WLog_Print(transport->log, WLOG_ERROR, "%s: error getting the frontBio handle", __FUNCTION__);
				return 0;
			}
		}
	}
	else
	{
		if (transport->rdg)
		{
			tmp = rdg_get_event_handles(transport->rdg, &events[1], count - 1);

			if (tmp == 0)
				return 0;

			nCount += tmp;
		}
		else if (transport->tsg)
		{
			tmp = tsg_get_event_handles(transport->tsg, &events[1], count - 1);

			if (tmp == 0)
				return 0;

			nCount += tmp;
		}
	}

	return nCount;
}

void transport_get_fds(rdpTransport* transport, void** rfds, int* rcount)
{
	DWORD index;
	DWORD nCount;
	HANDLE events[64];
	nCount = transport_get_event_handles(transport, events, 64);
	*rcount = nCount + 1;

	for (index = 0; index < nCount; index++)
	{
		rfds[index] = GetEventWaitObject(events[index]);
	}

	rfds[nCount] = GetEventWaitObject(transport->rereadEvent);
}

BOOL transport_is_write_blocked(rdpTransport* transport)
{
	return BIO_write_blocked(transport->frontBio);
}

int transport_drain_output_buffer(rdpTransport* transport)
{
	BOOL status = FALSE;

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
	int recv_status;
	wStream* received;
	DWORD now = GetTickCount();
	DWORD dueDate = 0;

	if (!transport || !transport->context || !transport->context->settings)
		return -1;

	dueDate = now + transport->context->settings->MaxTimeInCheckLoop;

	if (transport->haveMoreBytesToRead)
	{
		transport->haveMoreBytesToRead = FALSE;
		ResetEvent(transport->rereadEvent);
	}

	while (now < dueDate)
	{
		if (freerdp_shall_disconnect(transport->context->instance))
		{
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
				WLog_Print(transport->log, WLOG_DEBUG, "transport_check_fds: transport_read_pdu() - %i", status);

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
		recv_status = transport->ReceiveCallback(transport, received, transport->ReceiveExtra);
		Stream_Release(received);

		/* session redirection or activation */
		if (recv_status == 1 || recv_status == 2)
		{
			return recv_status;
		}

		if (recv_status < 0)
		{
			WLog_Print(transport->log, WLOG_ERROR, "transport_check_fds: transport->ReceiveCallback() - %i",
			           recv_status);
			return -1;
		}

		now = GetTickCount();
	}

	if (now >= dueDate)
	{
		SetEvent(transport->rereadEvent);
		transport->haveMoreBytesToRead = TRUE;
	}

	return 0;
}

BOOL transport_set_blocking_mode(rdpTransport* transport, BOOL blocking)
{
	transport->blocking = blocking;

	if (!BIO_set_nonblock(transport->frontBio, blocking ? FALSE : TRUE))
		return FALSE;

	return TRUE;
}

void transport_set_gateway_enabled(rdpTransport* transport, BOOL GatewayEnabled)
{
	transport->GatewayEnabled = GatewayEnabled;
}

void transport_set_nla_mode(rdpTransport* transport, BOOL NlaMode)
{
	transport->NlaMode = NlaMode;
}

BOOL transport_disconnect(rdpTransport* transport)
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
			BIO_free(transport->frontBio);
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

	if (!transport_set_layer_state(transport, TRANSPORT_LAYER_TCP))
		status = FALSE;

	return status;
}

rdpTransport* transport_new(rdpContext* context)
{
	rdpTransport* transport;
	transport = (rdpTransport*) calloc(1, sizeof(rdpTransport));

	if (!transport)
		return NULL;

	transport->log = WLog_Get(TAG);

	if (!transport->log)
		goto fail;

	transport->context = context;
	transport->ReceivePool = StreamPool_New(TRUE, BUFFER_SIZE);

	if (!transport->ReceivePool)
		goto fail;

	/* receive buffer for non-blocking read. */
	transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0);

	if (!transport->ReceiveBuffer)
		goto fail;

	transport->connectedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->connectedEvent
	    || transport->connectedEvent == INVALID_HANDLE_VALUE)
		goto fail;

	transport->rereadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->rereadEvent || transport->rereadEvent == INVALID_HANDLE_VALUE)
		goto fail;

	transport->haveMoreBytesToRead = FALSE;
	transport->blocking = TRUE;
	transport->GatewayEnabled = FALSE;

	if (!transport_set_layer_state(transport, TRANSPORT_LAYER_TCP))
		goto fail;

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

	StreamPool_Free(transport->ReceivePool);
	CloseHandle(transport->connectedEvent);
	CloseHandle(transport->rereadEvent);
	DeleteCriticalSection(&(transport->ReadLock));
	DeleteCriticalSection(&(transport->WriteLock));
	free(transport);
}

rdpContext* transport_get_context(rdpTransport* transport)
{
	if (!transport)
		return NULL;

	return transport->context;
}

BIO* transport_get_bio(rdpTransport* transport)
{
	if (!transport)
		return NULL;

	return transport->frontBio;
}

rdpNla* transport_get_nla(rdpTransport* transport)
{
	if (!transport)
		return NULL;

	return transport->nla;
}

void transport_free_nla(rdpTransport* transport)
{
	if (transport)
	{
		nla_free(transport->nla);
		transport->nla = NULL;
	}
}

BOOL transport_set_recv_callback(rdpTransport* transport, TransportRecv cb, void* extra)
{
	if (!transport)
		return FALSE;

	transport->ReceiveCallback = cb;
	transport->ReceiveExtra = extra;
	return transport->ReceiveCallback != NULL;
}

rdpTls* transport_get_tls(rdpTransport* transport)
{
	if (!transport)
		return NULL;

	return transport->tls;
}

BOOL transport_set_connected(rdpTransport* transport)
{
	if (!transport)
		return FALSE;

	return SetEvent(transport->connectedEvent);
}

SSIZE_T transport_get_sent_bytes(rdpTransport* transport, BOOL reset)
{
	SSIZE_T bytes;

	if (!transport)
		return -1;

	bytes = transport->written;

	if (reset)
		transport->written = 0;

	return bytes;
}

TRANSPORT_LAYER transport_get_layer_state(rdpTransport* transport)
{
	if (!transport)
		return TRANSPORT_LAYER_CLOSED;

	return transport->layer;
}

BOOL transport_set_layer_state(rdpTransport* transport, TRANSPORT_LAYER state)
{
	if (!transport)
		return FALSE;

	transport->layer = state;
	return TRUE;
}

BOOL transport_is_blocking(rdpTransport* transport)
{
	if (!transport)
		return FALSE;

	return transport->blocking;
}

rdpTsg* transport_get_tsg(rdpTransport* transport)
{
	if (!transport)
		return NULL;

	return transport->tsg;
}

BOOL transport_has_more_bytes_to_read(rdpTransport* transport)
{
	if (!transport)
		return FALSE;

	return transport->haveMoreBytesToRead;
}

wStream* transport_receive_pool_take(rdpTransport* transport, size_t size)
{
	if (!transport)
		return NULL;

	return StreamPool_Take(transport->ReceivePool, size);
}
