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
		WLog_Print(transport->log, WLOG_ERROR, "krb5_parse_name_flags failed with error %d",
		           (int)ret);
	/* Find a credential cache with a specified client principal */
	else if ((ret = krb5_cc_cache_match(context, principal, &ccache)) != 0)
	{
		if ((ret = krb5_cc_default(context, &ccache)) != 0)
			WLog_Print(transport->log, WLOG_ERROR,
			           "krb5 failed to resolve credentials cache with error %d", (int)ret);
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
		WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_set_password returned error %d",
		           ret);
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
		rdpTransport* transport = (rdpTransport*)SSL_get_app_data(ssl);

		switch (ret)
		{
			case (SSL3_AL_FATAL << 8) | SSL_AD_ACCESS_DENIED:
			{
				if (!freerdp_get_last_error(transport->context))
				{
					WLog_Print(transport->log, WLOG_ERROR, "%s: ACCESS DENIED", __FUNCTION__);
					freerdp_set_last_error_log(transport->context,
					                           FREERDP_ERROR_AUTHENTICATION_FAILED);
				}
			}
			break;

			case (SSL3_AL_FATAL << 8) | SSL_AD_INTERNAL_ERROR:
			{
				if (transport->NlaMode)
				{
					UINT32 kret = 0;
#ifdef WITH_GSSAPI

					if ((strlen(transport->settings->Domain) != 0) &&
					    (strncmp(transport->settings->Domain, ".", 1) != 0))
					{
						kret = transport_krb5_check_account(
						    transport, transport->settings->Username, transport->settings->Domain,
						    transport->settings->Password);
					}
					else
#endif /* WITH_GSSAPI */
						kret = FREERDP_ERROR_CONNECT_PASSWORD_CERTAINLY_EXPIRED;

					freerdp_set_last_error_if_not(transport->context, kret);
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

BOOL transport_attach_impl(rdpTransport* transport, int sockfd)
{
	BIO* socketBio = NULL;
	BIO* bufferedBio;
	socketBio = BIO_new(BIO_s_simple_socket());

	if (!socketBio)
		goto fail;

	BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
		goto fail;

	bufferedBio = BIO_push(bufferedBio, socketBio);
	transport->frontBio = bufferedBio;
	return TRUE;
fail:

	if (socketBio)
		BIO_free_all(socketBio);
	else
		close(sockfd);

	return FALSE;
}

BOOL transport_attach(rdpTransport* transport, int sockfd)
{
	return transport->context->update->io->TransportAttach(transport, sockfd);
}

BOOL transport_connect_rdp(rdpTransport* transport)
{
	/* RDP encryption */
	return TRUE;
}

BOOL transport_connect_tls_impl(rdpTransport* transport)
{
	int tlsStatus;
	rdpTls* tls = NULL;
	rdpContext* context = transport->context;
	rdpSettings* settings = transport->settings;

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

BOOL transport_connect_tls(rdpTransport* transport)
{
	return transport->context->update->io->TLSConnect(transport);
}

BOOL transport_connect_nla(rdpTransport* transport)
{
	rdpContext* context = transport->context;
	rdpSettings* settings = context->settings;
	freerdp* instance = context->instance;
	rdpRdp* rdp = context->rdp;

	if (!transport_connect_tls(transport))
		return FALSE;

	if (!settings->Authentication)
		return TRUE;

	nla_free(rdp->nla);
	rdp->nla = nla_new(instance, transport, settings);

	if (!rdp->nla)
		return FALSE;

	transport_set_nla_mode(transport, TRUE);

	if (settings->AuthenticationServiceClass)
	{
		if (!nla_set_service_principal(rdp->nla, nla_make_spn(settings->AuthenticationServiceClass,
		                                                      settings->ServerHostname)))
			return FALSE;
	}

	if (nla_client_begin(rdp->nla) < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR, "NLA begin failed");

		freerdp_set_last_error_if_not(context, FREERDP_ERROR_AUTHENTICATION_FAILED);

		transport_set_nla_mode(transport, FALSE);
		return FALSE;
	}

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_NLA);
	return TRUE;
}

BOOL transport_connect(rdpTransport* transport, const char* hostname, UINT16 port, DWORD timeout)
{
	int sockfd;
	BOOL status = FALSE;
	rdpSettings* settings = transport->settings;
	rdpContext* context = transport->context;
	BOOL rpcFallback = !settings->GatewayHttpTransport;

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
			sockfd = freerdp_tcp_connect(context, settings, proxyHostname,
			                                                    peerPort, timeout);
		else
			sockfd = freerdp_tcp_connect(context, settings, hostname, port,
			                                                    timeout);

		if (sockfd < 0)
			return FALSE;

		if (!transport_attach(transport, sockfd))
			return FALSE;

		if (isProxyConnection)
		{
			if (!transport->context->update->io->ProxyConnect(
			        settings, transport->frontBio, proxyUsername, proxyPassword, hostname, port))
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
	rdpSettings* settings = transport->settings;

	if (!transport->tls)
		transport->tls = tls_new(transport->settings);

	transport->layer = TRANSPORT_LAYER_TLS;

	if (!tls_accept(transport->tls, transport->frontBio, settings))
		return FALSE;

	transport->frontBio = transport->tls->bio;
	return TRUE;
}

BOOL transport_accept_nla(rdpTransport* transport)
{
	rdpSettings* settings = transport->settings;
	freerdp* instance = (freerdp*)settings->instance;

	if (!transport->tls)
		transport->tls = tls_new(transport->settings);

	transport->layer = TRANSPORT_LAYER_TLS;

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
			WLog_PrintMessage(transport->log, WLOG_MESSAGE_TEXT, level, line, file, func, fmt,
			                  biofunc, buf);
		}

		free(buf);
	}
}

static SSIZE_T transport_read_layer(rdpTransport* transport, BYTE* data, size_t bytes)
{
	SSIZE_T read = 0;
	rdpRdp* rdp = transport->context->rdp;

	if (!transport->frontBio || (bytes > SSIZE_MAX))
	{
		transport->layer = TRANSPORT_LAYER_CLOSED;
		freerdp_set_last_error_if_not(transport->context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
		return -1;
	}

	while (read < (SSIZE_T)bytes)
	{
		const SSIZE_T tr = (SSIZE_T)bytes - read;
		int r = (int)((tr > INT_MAX) ? INT_MAX : tr);
		int status = BIO_read(transport->frontBio, data + read, r);

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
				freerdp_set_last_error_if_not(transport->context,
				                              FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
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

#ifdef HAVE_VALGRIND_MEMCHECK_H
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
	rdpContext* context = NULL;
	if (toRead > SSIZE_MAX)
		return 0;
	if (!transport)
		return 0;
	context = transport->context;
	if (!context)
		return 0;
	status = context->update->io->Read(context, Stream_Pointer(s), toRead);

	if (status <= 0)
		return status;

	if (transport->NextPDUBytesLeft > 0)
		transport->NextPDUBytesLeft -= status;

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
	int status = 0, read = 0;
	SSIZE_T left = 0;
	for (status = transport_handle_pdu(transport, s, &left); !status;
	     status = transport_handle_pdu(transport, s, &left))
	{
		read = transport_read_layer_bytes(transport, s, left ? left : 1);
		if (read < 0)
			return read;
		if (left > 0)
			left -= read;
	}
	return status;
}

/**
 * @brief Handle a complete PDU (NLA, fast-path or tpkt) from the filled wStream buffer.
 *
 * @param[in] transport rdpTransport
 * @param[in] s wStream
 * @return < 0 on error; 0 if not enough data is available; > 0 number of
 * bytes of the *complete* pdu
 */
int transport_handle_pdu(rdpTransport* transport, wStream* s, SSIZE_T* left_to_read)
{
	size_t position;
	size_t pduLength;
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
	if (position < 2)
		return 0;

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
			if (pduLength < 7 || pduLength > 0xFFFF)
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

	if (!Stream_EnsureCapacity(s, Stream_GetPosition(s) + pduLength))
		return -1;

	if (left_to_read)
		*left_to_read = pduLength - Stream_GetPosition(s);

	/* pdu not yet complete */
	if (Stream_GetPosition(s) < pduLength)
		return 0;

	if (Stream_GetPosition(s) >= pduLength)
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), pduLength, WLOG_PACKET_INBOUND);

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);
	return Stream_Length(s);
}

static BOOL transport_prepare_stream(rdpTransport* transport, wStream* s)
{
	if (!(transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0)))
	{
		Stream_Release(s);
		return FALSE;
	}
	if (transport->NextPDUBytesLeft < 0)
	{
		/* if we currenlty have part of next pdu in buffer,
		 * we need to move it into next pdu's buffer  */
		Stream_SetPosition(s, Stream_Length(s) + transport->NextPDUBytesLeft);
		Stream_Read(s, transport->ReceiveBuffer->pointer, -(transport->NextPDUBytesLeft));
	}
	transport->NextPDUBytesLeft = 0;
	Stream_Release(s);
	return TRUE;
}

/**
 * @brief Callback function to handle bytes from io backends
 *
 * @param[in] context rdpContext
 * @param[in] buf const uint8_t*
 * @param[in] buf_size size_t
 * @return count of bytes succesfully handled, -1 on error
 */
static int transport_io_data_handler(rdpContext* context, const uint8_t* buf, size_t buf_size)
{
	rdpTransport* transport = NULL;
	wStream* pdu = NULL;
	if (buf_size > SSIZE_MAX)
		return -1;
	if (!context->rdp)
		return -1;
	transport = context->rdp->transport;
	if (!transport)
		return -1;

	Stream_Write(transport->ReceiveBuffer, buf, buf_size);
	pdu = transport->ReceiveBuffer;
	if (transport_handle_pdu(transport, pdu, 0) < 1)
		return 0;
	transport->ReceiveCallback(transport, pdu, transport->ReceiveExtra);

	if (!transport_prepare_stream(transport, pdu))
		return -1;

	return buf_size;
}

/**
 * @brief Callback function for io backends read implementation
 *
 * @param[in] context rdpContext
 * @param[out] buf const uint8_t*
 * @param[in] buf_size size_t
 * @return count of bytes succesfully handled
 */
static int transport_io_data_read(rdpContext* context, const uint8_t* buf, size_t buf_size)
{
	SSIZE_T status;
	rdpTransport* transport = NULL;

	transport = context->rdp->transport;

	status = transport_read_layer(transport, (BYTE*)buf, buf_size);

	return status;
}

/**
 * @brief Callback function for io backend write implementation
 *
 * @param[in] context rdpContext
 * @param[in] buf const uint8_t*
 * @param[in] buf_size size_t
 * @return count of bytes succesfully handled
 */
static int transport_io_data_write(rdpContext* context, const uint8_t* buf, size_t buf_size)
{
	int status = -1;
	size_t length;
	int writtenlength = 0;
	rdpRdp* rdp = context->rdp;
	rdpTransport* transport = context->rdp->transport;

	if (!transport->frontBio)
	{
		transport->layer = TRANSPORT_LAYER_CLOSED;
		freerdp_set_last_error_if_not(transport->context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
		goto fail;
	}

	EnterCriticalSection(&(transport->WriteLock));
	length = buf_size;
	writtenlength = length;

	if (length > 0)
	{
		rdp->outBytes += length;
		WLog_Packet(transport->log, WLOG_TRACE, buf, length, WLOG_PACKET_OUTBOUND);
	}

	while (length > 0)
	{
		status = BIO_write(transport->frontBio, buf + writtenlength - length, length);

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

		if (transport->blocking || transport->settings->WaitForOutputBufferFlush)
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
	}

out_cleanup:

	if (status < 0)
	{
		/* A write error indicates that the peer has dropped the connection */
		transport->layer = TRANSPORT_LAYER_CLOSED;
		freerdp_set_last_error_if_not(transport->context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
	}

	LeaveCriticalSection(&(transport->WriteLock));
fail:
	if (!length)
		return writtenlength;
	return status;
}

void transport_register_default_io_callbacks(rdpUpdate* update)
{
	rdpIoUpdate io = { 0 };
	io.context = update->context;
	io.TCPConnect = freerdp_tcp_connect_impl;
	io.TLSConnect = transport_connect_tls_impl;
	io.TransportAttach = transport_attach_impl;
	io.ProxyConnect = proxy_connect_impl;
	io.TransportDisconnect = transport_disconnect_impl;
	io.DataHandler = transport_io_data_handler;
	io.Read = transport_io_data_read;
	io.Write = transport_io_data_write;
	freerdp_set_transport_callbacks(update->context, &io);
}

void freerdp_set_transport_callbacks(rdpContext* context, void* io_callbacks)
{
	rdpIoUpdate* io = ((rdpIoUpdate*)io_callbacks);
	/* NOTE: DataHandler should not be switched */
	io->DataHandler = transport_io_data_handler;
	/* NOTE: add context ponter in case user forgot to do so */
	io->context = context;
	*(context->update->io) = *io;
}

int transport_write(rdpTransport* transport, wStream* s)
{
	int status = -1;
	rdpRdp* rdp = NULL;

	if (!s)
		return -1;

	if (!transport)
		goto fail;

	rdp = transport->context->rdp;

	status = rdp->update->io->Write(rdp->context, Stream_Buffer(s), Stream_GetPosition(s));

	if (status > 0)
		transport->written += status;
fail:
	Stream_Release(s);
	return status;
}

DWORD transport_get_event_handles(rdpTransport* transport, HANDLE* events, DWORD count)
{
	DWORD nCount = 1; /* always the reread Event */
	DWORD tmp;

	if (events)
	{
		if (count < 1)
		{
			WLog_Print(transport->log, WLOG_ERROR, "%s: provided handles array is too small",
			           __FUNCTION__);
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
				           "%s: provided handles array is too small (count=%" PRIu32
				           " nCount=%" PRIu32 ")",
				           __FUNCTION__, count, nCount);
				return 0;
			}

			if (BIO_get_event(transport->frontBio, &events[1]) != 1)
			{
				WLog_Print(transport->log, WLOG_ERROR, "%s: error getting the frontBio handle",
				           __FUNCTION__);
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

	if (!transport)
		return -1;

	if (transport->layer == TRANSPORT_LAYER_CLOSED)
	{
		WLog_Print(transport->log, WLOG_DEBUG, "transport_check_fds: transport layer closed");
		freerdp_set_last_error_if_not(transport->context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
		return -1;
	}

	{
		if (freerdp_shall_disconnect(transport->context->instance))
		{
			return -1;
		}

		if ((status = transport_read_layer_bytes(
		         transport, transport->ReceiveBuffer,
		         transport->NextPDUBytesLeft ? transport->NextPDUBytesLeft : 1)) <= 0)
		{
			if (status < 0)
			{
				WLog_Print(transport->log, WLOG_DEBUG,
				           "transport_check_fds: transport_read_layer_bytes() - %i", status);
			}
			return status;
		}

		if ((status = transport_handle_pdu(transport, transport->ReceiveBuffer,
		                                   &transport->NextPDUBytesLeft)) <= 0)
		{
			if (status < 0)
			{
				WLog_Print(transport->log, WLOG_DEBUG,
				           "transport_check_fds: transport_handle_pdu() - %i", status);
			}
			else
			{
				SetEvent(transport->rereadEvent);
			}
			return status;
		}
		ResetEvent(transport->rereadEvent);

		received = transport->ReceiveBuffer;

		/**
		 * status:
		 * 	-1: error
		 * 	 0: success
		 * 	 1: redirection
		 */
		recv_status = transport->ReceiveCallback(transport, received, transport->ReceiveExtra);

		if (!transport_prepare_stream(transport, received))
			return -1;

		/* session redirection or activation */
		if (recv_status == 1 || recv_status == 2)
		{
			return recv_status;
		}

		if (recv_status < 0)
		{
			WLog_Print(transport->log, WLOG_ERROR,
			           "transport_check_fds: transport->ReceiveCallback() - %i", recv_status);
			return -1;
		}
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

BOOL transport_disconnect_impl(rdpTransport* transport)
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

BOOL transport_disconnect(rdpTransport* transport)
{
	return transport->context->update->io->TransportDisconnect(transport);
}


rdpTransport* transport_new(rdpContext* context)
{
	rdpTransport* transport;
	transport = (rdpTransport*)calloc(1, sizeof(rdpTransport));

	if (!transport)
		return NULL;

	transport->log = WLog_Get(TAG);

	if (!transport->log)
		goto out_free_transport;

	transport->context = context;
	transport->settings = context->settings;
	transport->ReceivePool = StreamPool_New(TRUE, BUFFER_SIZE);

	if (!transport->ReceivePool)
		goto out_free_transport;

	/* receive buffer for non-blocking read. */
	transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0);

	if (!transport->ReceiveBuffer)
		goto out_free_receivepool;

	transport->connectedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->connectedEvent || transport->connectedEvent == INVALID_HANDLE_VALUE)
		goto out_free_receivebuffer;

	transport->rereadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->rereadEvent || transport->rereadEvent == INVALID_HANDLE_VALUE)
		goto out_free_connectedEvent;

	transport->blocking = TRUE;
	transport->GatewayEnabled = FALSE;
	transport->layer = TRANSPORT_LAYER_TCP;

	if (!InitializeCriticalSectionAndSpinCount(&(transport->ReadLock), 4000))
		goto out_free_rereadEvent;

	if (!InitializeCriticalSectionAndSpinCount(&(transport->WriteLock), 4000))
		goto out_free_readlock;

	return transport;
out_free_readlock:
	DeleteCriticalSection(&(transport->ReadLock));
out_free_rereadEvent:
	CloseHandle(transport->rereadEvent);
out_free_connectedEvent:
	CloseHandle(transport->connectedEvent);
out_free_receivebuffer:
	StreamPool_Return(transport->ReceivePool, transport->ReceiveBuffer);
out_free_receivepool:
	StreamPool_Free(transport->ReceivePool);
out_free_transport:
	free(transport);
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
	DeleteCriticalSection(&(transport->ReadLock));
	DeleteCriticalSection(&(transport->WriteLock));
	free(transport);
}
