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

#include "settings.h"

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
#include "childsession.h"

#include "gateway/rdg.h"
#include "gateway/wst.h"
#include "gateway/arm.h"

#define TAG FREERDP_TAG("core.transport")

#define BUFFER_SIZE 16384

struct rdp_transport
{
	TRANSPORT_LAYER layer;
	BIO* frontBio;
	rdpRdg* rdg;
	rdpTsg* tsg;
	rdpWst* wst;
	rdpTls* tls;
	rdpContext* context;
	rdpNla* nla;
	void* ReceiveExtra;
	wStream* ReceiveBuffer;
	TransportRecv ReceiveCallback;
	wStreamPool* ReceivePool;
	HANDLE connectedEvent;
	BOOL NlaMode;
	BOOL RdstlsMode;
	BOOL AadMode;
	BOOL blocking;
	BOOL GatewayEnabled;
	CRITICAL_SECTION ReadLock;
	CRITICAL_SECTION WriteLock;
	UINT64 written;
	HANDLE rereadEvent;
	BOOL haveMoreBytesToRead;
	wLog* log;
	rdpTransportIo io;
	HANDLE ioEvent;
	BOOL useIoEvent;
	BOOL earlyUserAuth;
};

typedef struct
{
	rdpTransportLayer pub;
	void* userContextShadowPtr;
} rdpTransportLayerInt;

static void transport_ssl_cb(const SSL* ssl, int where, int ret)
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
					WLog_Print(transport->log, WLOG_ERROR, "ACCESS DENIED");
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
						UINT32 kret = 0;
						if (transport->nla)
							kret = nla_get_error(transport->nla);
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

wStream* transport_send_stream_init(WINPR_ATTR_UNUSED rdpTransport* transport, size_t size)
{
	WINPR_ASSERT(transport);

	return Stream_New(NULL, size);
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
	BIO* bufferedBio = NULL;
	const rdpSettings* settings = NULL;
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
	}

	bufferedBio = BIO_new(BIO_s_buffered_socket());
	if (!bufferedBio)
		goto fail;

	if (socketBio)
	{
		bufferedBio = BIO_push(bufferedBio, socketBio);
		if (!bufferedBio)
			goto fail;

		/* Attach the socket only when this function can no longer fail.
		 * This ensures solid ownership:
		 * - if this function fails, the caller is responsible to clean up
		 * - if this function is successful, the caller MUST NOT close the socket any more.
		 */
		BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	}
	EnterCriticalSection(&(transport->ReadLock));
	EnterCriticalSection(&(transport->WriteLock));
	transport->frontBio = bufferedBio;
	LeaveCriticalSection(&(transport->WriteLock));
	LeaveCriticalSection(&(transport->ReadLock));

	return TRUE;
fail:

	if (socketBio)
		BIO_free_all(socketBio);
	else
		closesocket((SOCKET)sockfd);

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
		case AUTH_CANCELLED:
			freerdp_set_last_error_if_not(transport_get_context(transport),
			                              FREERDP_ERROR_CONNECT_CANCELLED);
			return FALSE;
		default:
			return FALSE;
	}
}

BOOL transport_connect_tls(rdpTransport* transport)
{
	const rdpSettings* settings = NULL;
	rdpContext* context = transport_get_context(transport);

	settings = context->settings;
	WINPR_ASSERT(settings);

	/* Only prompt for password if we use TLS (NLA also calls this function) */
	if (settings->SelectedProtocol == PROTOCOL_SSL)
	{
		switch (utils_authenticate(context->instance, AUTH_TLS, FALSE))
		{
			case AUTH_SKIP:
			case AUTH_SUCCESS:
			case AUTH_NO_CREDENTIALS:
				break;
			case AUTH_CANCELLED:
				freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_CANCELLED);
				return FALSE;
			default:
				return FALSE;
		}
	}

	return IFCALLRESULT(FALSE, transport->io.TLSConnect, transport);
}

static BOOL transport_default_connect_tls(rdpTransport* transport)
{
	int tlsStatus = 0;
	rdpTls* tls = NULL;
	rdpContext* context = NULL;
	rdpSettings* settings = NULL;

	WINPR_ASSERT(transport);

	context = transport_get_context(transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!(tls = freerdp_tls_new(context)))
		return FALSE;

	transport->tls = tls;

	if (transport->GatewayEnabled)
		transport->layer = TRANSPORT_LAYER_TSG_TLS;
	else
		transport->layer = TRANSPORT_LAYER_TLS;

	tls->hostname = settings->ServerHostname;
	tls->serverName = settings->UserSpecifiedServerName;
	tls->port = WINPR_ASSERTING_INT_CAST(int32_t, MIN(UINT16_MAX, settings->ServerPort));

	if (tls->port == 0)
		tls->port = 3389;

	tls->isGatewayTransport = FALSE;
	tlsStatus = freerdp_tls_connect(tls, transport->frontBio);

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

	/* See libfreerdp/crypto/tls.c transport_default_connect_tls
	 *
	 * we are wrapping a SSL object in the BIO and actually want to set
	 *
	 * SSL_set_info_callback there. So ensure our callback is of appropriate
	 * type for that instead of what the function prototype suggests.
	 */
	typedef void (*ssl_cb_t)(const SSL* ssl, int type, int val);
	ssl_cb_t fkt = transport_ssl_cb;

	BIO_info_cb* bfkt = WINPR_FUNC_PTR_CAST(fkt, BIO_info_cb*);
	BIO_callback_ctrl(tls->bio, BIO_CTRL_SET_CALLBACK, bfkt);
	SSL_set_app_data(tls->ssl, transport);

	if (!transport->frontBio)
	{
		WLog_Print(transport->log, WLOG_ERROR, "unable to prepend a filtering TLS bio");
		return FALSE;
	}

	return TRUE;
}

BOOL transport_connect_nla(rdpTransport* transport, BOOL earlyUserAuth)
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

	nla_set_early_user_auth(rdp->nla, earlyUserAuth);

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

BOOL transport_connect_rdstls(rdpTransport* transport)
{
	BOOL rc = FALSE;
	rdpRdstls* rdstls = NULL;
	rdpContext* context = NULL;

	WINPR_ASSERT(transport);

	context = transport_get_context(transport);
	WINPR_ASSERT(context);

	if (!transport_connect_tls(transport))
		goto fail;

	rdstls = rdstls_new(context, transport);
	if (!rdstls)
		goto fail;

	transport_set_rdstls_mode(transport, TRUE);

	if (rdstls_authenticate(rdstls) < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR, "RDSTLS authentication failed");
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_AUTHENTICATION_FAILED);
		goto fail;
	}

	transport_set_rdstls_mode(transport, FALSE);
	rc = TRUE;
fail:
	rdstls_free(rdstls);
	return rc;
}

BOOL transport_connect_aad(rdpTransport* transport)
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

	if (!rdp->aad)
		return FALSE;

	transport_set_aad_mode(transport, TRUE);

	if (aad_client_begin(rdp->aad) < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR, "AAD begin failed");

		freerdp_set_last_error_if_not(context, FREERDP_ERROR_AUTHENTICATION_FAILED);

		transport_set_aad_mode(transport, FALSE);
		return FALSE;
	}

	return rdp_client_transition_to_state(rdp, CONNECTION_STATE_AAD);
}

BOOL transport_connect(rdpTransport* transport, const char* hostname, UINT16 port, DWORD timeout)
{
	BOOL status = FALSE;
	rdpSettings* settings = NULL;
	rdpContext* context = transport_get_context(transport);
	BOOL rpcFallback = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(hostname);

	settings = context->settings;
	WINPR_ASSERT(settings);

	rpcFallback = !settings->GatewayHttpTransport;

	if (transport->GatewayEnabled)
	{
		if (settings->GatewayUrl)
		{
			WINPR_ASSERT(!transport->wst);
			transport->wst = wst_new(context);

			if (!transport->wst)
				return FALSE;

			status = wst_connect(transport->wst, timeout);

			if (status)
			{
				transport->frontBio = wst_get_front_bio_and_take_ownership(transport->wst);
				WINPR_ASSERT(transport->frontBio);
				BIO_set_nonblock(transport->frontBio, 0);
				transport->layer = TRANSPORT_LAYER_TSG;
				status = TRUE;
			}
			else
			{
				wst_free(transport->wst);
				transport->wst = NULL;
			}
		}
		if (!status && settings->GatewayHttpTransport)
		{
			WINPR_ASSERT(!transport->rdg);
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
			WINPR_ASSERT(!transport->tsg);
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
		UINT16 peerPort = 0;
		const char* proxyHostname = NULL;
		const char* proxyUsername = NULL;
		const char* proxyPassword = NULL;
		BOOL isProxyConnection =
		    proxy_prepare(settings, &proxyHostname, &peerPort, &proxyUsername, &proxyPassword);

		rdpTransportLayer* layer = NULL;
		if (isProxyConnection)
			layer = transport_connect_layer(transport, proxyHostname, peerPort, timeout);
		else
			layer = transport_connect_layer(transport, hostname, port, timeout);

		if (!layer)
			return FALSE;

		if (!transport_attach_layer(transport, layer))
		{
			transport_layer_free(layer);
			return FALSE;
		}

		if (isProxyConnection)
		{
			if (!proxy_connect(context, transport->frontBio, proxyUsername, proxyPassword, hostname,
			                   port))
				return FALSE;
		}

		status = TRUE;
	}

	return status;
}

BOOL transport_connect_childsession(rdpTransport* transport)
{
	WINPR_ASSERT(transport);

	transport->frontBio = createChildSessionBio();
	if (!transport->frontBio)
		return FALSE;

	transport->layer = TRANSPORT_LAYER_TSG;
	return TRUE;
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
	rdpSettings* settings = NULL;

	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!transport->tls)
		transport->tls = freerdp_tls_new(context);

	transport->layer = TRANSPORT_LAYER_TLS;

	if (!freerdp_tls_accept(transport->tls, transport->frontBio, settings))
		return FALSE;

	transport->frontBio = transport->tls->bio;
	return TRUE;
}

BOOL transport_accept_nla(rdpTransport* transport)
{
	rdpContext* context = transport_get_context(transport);
	rdpSettings* settings = NULL;

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
		freerdp_tls_set_alert_code(transport->tls, TLS_ALERT_LEVEL_FATAL,
		                           TLS_ALERT_DESCRIPTION_ACCESS_DENIED);
		freerdp_tls_send_alert(transport->tls);
		return FALSE;
	}

	/* don't free nla module yet, we need to copy the credentials from it first */
	transport_set_nla_mode(transport, FALSE);
	return TRUE;
}

BOOL transport_accept_rdstls(rdpTransport* transport)
{
	BOOL rc = FALSE;
	rdpRdstls* rdstls = NULL;
	rdpContext* context = NULL;

	WINPR_ASSERT(transport);

	context = transport_get_context(transport);
	WINPR_ASSERT(context);

	if (!IFCALLRESULT(FALSE, transport->io.TLSAccept, transport))
		goto fail;

	rdstls = rdstls_new(context, transport);
	if (!rdstls)
		goto fail;

	transport_set_rdstls_mode(transport, TRUE);

	if (rdstls_authenticate(rdstls) < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR, "client authentication failure");
		freerdp_tls_set_alert_code(transport->tls, TLS_ALERT_LEVEL_FATAL,
		                           TLS_ALERT_DESCRIPTION_ACCESS_DENIED);
		freerdp_tls_send_alert(transport->tls);
		goto fail;
	}

	transport_set_rdstls_mode(transport, FALSE);
	rc = TRUE;
fail:
	rdstls_free(rdstls);
	return rc;
}

#define WLog_ERR_BIO(transport, biofunc, bio) \
	transport_bio_error_log(transport, biofunc, bio, __FILE__, __func__, __LINE__)

static void transport_bio_error_log(rdpTransport* transport, LPCSTR biofunc,
                                    WINPR_ATTR_UNUSED BIO* bio, LPCSTR file, LPCSTR func,
                                    DWORD line)
{
	unsigned long sslerr = 0;
	int saveerrno = 0;
	DWORD level = 0;

	WINPR_ASSERT(transport);

	saveerrno = errno;
	level = WLOG_ERROR;

	if (level < WLog_GetLogLevel(transport->log))
		return;

	if (ERR_peek_error() == 0)
	{
		char ebuffer[256] = { 0 };
		const char* fmt = "%s returned a system error %d: %s";
		if (saveerrno == 0)
			fmt = "%s retries exceeded";
		WLog_PrintMessage(transport->log, WLOG_MESSAGE_TEXT, level, line, file, func, fmt, biofunc,
		                  saveerrno, winpr_strerror(saveerrno, ebuffer, sizeof(ebuffer)));
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
	rdpRdp* rdp = NULL;
	rdpContext* context = NULL;

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
		rdp->inBytes += WINPR_ASSERTING_INT_CAST(uint64_t, status);
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
	SSIZE_T status = 0;
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

static SSIZE_T parse_nla_mode_pdu(rdpTransport* transport, wStream* stream)
{
	SSIZE_T pduLength = 0;
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, Stream_Buffer(stream), Stream_Length(stream));
	/*
	 * In case NlaMode is set TSRequest package(s) are expected
	 * 0x30 = DER encoded data with these bits set:
	 * bit 6 P/C constructed
	 * bit 5 tag number - sequence
	 */
	UINT8 typeEncoding = 0;
	if (Stream_GetRemainingLength(s) < 1)
		return 0;
	Stream_Read_UINT8(s, typeEncoding);
	if (typeEncoding == 0x30)
	{
		/* TSRequest (NLA) */
		UINT8 lengthEncoding = 0;
		if (Stream_GetRemainingLength(s) < 1)
			return 0;
		Stream_Read_UINT8(s, lengthEncoding);
		if (lengthEncoding & 0x80)
		{
			if ((lengthEncoding & ~(0x80)) == 1)
			{
				UINT8 length = 0;
				if (Stream_GetRemainingLength(s) < 1)
					return 0;
				Stream_Read_UINT8(s, length);
				pduLength = length;
				pduLength += 3;
			}
			else if ((lengthEncoding & ~(0x80)) == 2)
			{
				/* check for header bytes already read in previous calls */
				UINT16 length = 0;
				if (Stream_GetRemainingLength(s) < 2)
					return 0;
				Stream_Read_UINT16_BE(s, length);
				pduLength = length;
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
			pduLength = lengthEncoding;
			pduLength += 2;
		}
	}

	return pduLength;
}

static SSIZE_T parse_default_mode_pdu(rdpTransport* transport, wStream* stream)
{
	SSIZE_T pduLength = 0;
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, Stream_Buffer(stream), Stream_Length(stream));

	UINT8 version = 0;
	if (Stream_GetRemainingLength(s) < 1)
		return 0;
	Stream_Read_UINT8(s, version);
	if (version == 0x03)
	{
		/* TPKT header */
		UINT16 length = 0;
		if (Stream_GetRemainingLength(s) < 3)
			return 0;
		Stream_Seek(s, 1);
		Stream_Read_UINT16_BE(s, length);
		pduLength = length;

		/* min and max values according to ITU-T Rec. T.123 (01/2007) section 8 */
		if ((pduLength < 7) || (pduLength > 0xFFFF))
		{
			WLog_Print(transport->log, WLOG_ERROR, "tpkt - invalid pduLength: %" PRIdz, pduLength);
			return -1;
		}
	}
	else
	{
		/* Fast-Path Header */
		UINT8 length1 = 0;
		if (Stream_GetRemainingLength(s) < 1)
			return 0;
		Stream_Read_UINT8(s, length1);
		if (length1 & 0x80)
		{
			UINT8 length2 = 0;
			if (Stream_GetRemainingLength(s) < 1)
				return 0;
			Stream_Read_UINT8(s, length2);
			pduLength = ((length1 & 0x7F) << 8) | length2;
		}
		else
			pduLength = length1;

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

	return pduLength;
}

SSIZE_T transport_parse_pdu(rdpTransport* transport, wStream* s, BOOL* incomplete)
{
	SSIZE_T pduLength = 0;

	if (!transport)
		return -1;

	if (!s)
		return -1;

	if (incomplete)
		*incomplete = TRUE;

	Stream_SealLength(s);
	if (transport->NlaMode)
		pduLength = parse_nla_mode_pdu(transport, s);
	else if (transport->RdstlsMode)
		pduLength = rdstls_parse_pdu(transport->log, s);
	else
		pduLength = parse_default_mode_pdu(transport, s);

	if (pduLength <= 0)
		return pduLength;

	const size_t len = Stream_Length(s);
	if (len > WINPR_ASSERTING_INT_CAST(size_t, pduLength))
		return -1;

	if (incomplete)
		*incomplete = len < WINPR_ASSERTING_INT_CAST(size_t, pduLength);

	return pduLength;
}

static int transport_default_read_pdu(rdpTransport* transport, wStream* s)
{
	BOOL incomplete = 0;
	SSIZE_T status = 0;
	size_t pduLength = 0;
	size_t position = 0;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(s);

	/* RDS AAD Auth PDUs have no length indicator. We need to determine the end of the PDU by
	 * reading in one byte at a time until we encounter the terminating null byte */
	if (transport->AadMode)
	{
		BYTE c = '\0';
		do
		{
			const SSIZE_T rc = transport_read_layer(transport, &c, 1);
			if (rc != 1)
				return (rc == 0) ? 0 : -1;
			if (!Stream_EnsureRemainingCapacity(s, 1))
				return -1;
			Stream_Write_UINT8(s, c);
		} while (c != '\0');
	}
	else if (transport->earlyUserAuth)
	{
		if (!Stream_EnsureCapacity(s, 4))
			return -1;
		const SSIZE_T rc = transport_read_layer_bytes(transport, s, 4);
		if (rc != 1)
			return (rc == 0) ? 0 : -1;
	}
	else
	{
		/* Read in pdu length */
		status = transport_parse_pdu(transport, s, &incomplete);
		while ((status == 0) && incomplete)
		{
			if (!Stream_EnsureRemainingCapacity(s, 1))
				return -1;
			SSIZE_T rc = transport_read_layer_bytes(transport, s, 1);
			if (rc > INT32_MAX)
				return INT32_MAX;
			if (rc != 1)
				return (int)rc;
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
		else if (position < pduLength)
		{
			status = transport_read_layer_bytes(transport, s, pduLength - position);
			if (status != 1)
			{
				if ((status < INT32_MIN) || (status > INT32_MAX))
					return -1;
				return (int)status;
			}
		}

		if (Stream_GetPosition(s) >= pduLength)
			WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), pduLength,
			            WLOG_PACKET_INBOUND);
	}

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);
	const size_t len = Stream_Length(s);
	if (len > INT32_MAX)
		return -1;
	return (int)len;
}

int transport_write(rdpTransport* transport, wStream* s)
{
	if (!transport)
		return -1;

	return IFCALLRESULT(-1, transport->io.WritePdu, transport, s);
}

static int transport_default_write(rdpTransport* transport, wStream* s)
{
	int status = -1;
	rdpContext* context = transport_get_context(transport);

	WINPR_ASSERT(transport);
	WINPR_ASSERT(context);

	if (!s)
		return -1;

	Stream_AddRef(s);

	rdpRdp* rdp = context->rdp;
	if (!rdp)
		goto fail;

	EnterCriticalSection(&(transport->WriteLock));
	if (!transport->frontBio)
		goto out_cleanup;

	size_t length = Stream_GetPosition(s);
	size_t writtenlength = length;
	Stream_SetPosition(s, 0);

	if (length > 0)
	{
		rdp->outBytes += length;
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), length, WLOG_PACKET_OUTBOUND);
	}

	while (length > 0)
	{
		ERR_clear_error();
		const int towrite = (length > INT32_MAX) ? INT32_MAX : (int)length;
		status = BIO_write(transport->frontBio, Stream_ConstPointer(s), towrite);

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

		const size_t ustatus = (size_t)status;
		if (ustatus > length)
		{
			status = -1;
			goto out_cleanup;
		}

		length -= ustatus;
		Stream_Seek(s, ustatus);
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

BOOL transport_get_public_key(rdpTransport* transport, const BYTE** data, DWORD* length)
{
	return IFCALLRESULT(FALSE, transport->io.GetPublicKey, transport, data, length);
}

static BOOL transport_default_get_public_key(rdpTransport* transport, const BYTE** data,
                                             DWORD* length)
{
	rdpTls* tls = transport_get_tls(transport);
	if (!tls)
		return FALSE;

	*data = tls->PublicKey;
	*length = tls->PublicKeyLength;

	return TRUE;
}

DWORD transport_get_event_handles(rdpTransport* transport, HANDLE* events, DWORD count)
{
	DWORD nCount = 0; /* always the reread Event */

	WINPR_ASSERT(transport);
	WINPR_ASSERT(events);
	WINPR_ASSERT(count > 0);

	if (events)
	{
		if (count < 1)
		{
			WLog_Print(transport->log, WLOG_ERROR, "provided handles array is too small");
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
				           "provided handles array is too small (count=%" PRIu32 " nCount=%" PRIu32
				           ")",
				           count, nCount);
				return 0;
			}

			if (transport->frontBio)
			{
				if (BIO_get_event(transport->frontBio, &events[nCount]) != 1)
				{
					WLog_Print(transport->log, WLOG_ERROR, "error getting the frontBio handle");
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
			const DWORD tmp =
			    rdg_get_event_handles(transport->rdg, &events[nCount], count - nCount);

			if (tmp == 0)
				return 0;

			nCount += tmp;
		}
		else if (transport->tsg)
		{
			const DWORD tmp =
			    tsg_get_event_handles(transport->tsg, &events[nCount], count - nCount);

			if (tmp == 0)
				return 0;

			nCount += tmp;
		}
		else if (transport->wst)
		{
			const DWORD tmp =
			    wst_get_event_handles(transport->wst, &events[nCount], count - nCount);

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
	DWORD nCount = 0;
	HANDLE events[MAXIMUM_WAIT_OBJECTS] = { 0 };

	WINPR_ASSERT(transport);
	WINPR_ASSERT(rfds);
	WINPR_ASSERT(rcount);

	nCount = transport_get_event_handles(transport, events, ARRAYSIZE(events));
	*rcount = nCount + 1;

	for (DWORD index = 0; index < nCount; index++)
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
	return BIO_write_blocked(transport->frontBio) != 0;
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

		const long rc = BIO_write_blocked(transport->frontBio);
		status = (rc != 0);
	}

	return status;
}

int transport_check_fds(rdpTransport* transport)
{
	int status = 0;
	state_run_t recv_status = STATE_RUN_FAILED;
	wStream* received = NULL;
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
			(void)ResetEvent(transport->rereadEvent);
		}
		return status;
	}

	received = transport->ReceiveBuffer;
	transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0);
	if (!transport->ReceiveBuffer)
	{
		Stream_Release(received);
		return -1;
	}

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
		(void)SetEvent(transport->rereadEvent);
	}
	return recv_status;
}

BOOL transport_set_blocking_mode(rdpTransport* transport, BOOL blocking)
{
	WINPR_ASSERT(transport);

	return IFCALLRESULT(FALSE, transport->io.SetBlockingMode, transport, blocking);
}

static BOOL transport_default_set_blocking_mode(rdpTransport* transport, BOOL blocking)
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

rdpTransportLayer* transport_connect_layer(rdpTransport* transport, const char* hostname, int port,
                                           DWORD timeout)
{
	WINPR_ASSERT(transport);

	return IFCALLRESULT(NULL, transport->io.ConnectLayer, transport, hostname, port, timeout);
}

static rdpTransportLayer* transport_default_connect_layer(rdpTransport* transport,
                                                          const char* hostname, int port,
                                                          DWORD timeout)
{
	rdpContext* context = transport_get_context(transport);
	WINPR_ASSERT(context);

	return freerdp_tcp_connect_layer(context, hostname, port, timeout);
}

BOOL transport_attach_layer(rdpTransport* transport, rdpTransportLayer* layer)
{
	WINPR_ASSERT(transport);
	WINPR_ASSERT(layer);

	return IFCALLRESULT(FALSE, transport->io.AttachLayer, transport, layer);
}

static BOOL transport_default_attach_layer(rdpTransport* transport, rdpTransportLayer* layer)
{
	BIO* layerBio = BIO_new(BIO_s_transport_layer());
	if (!layerBio)
		goto fail;

	BIO* bufferedBio = BIO_new(BIO_s_buffered_socket());
	if (!bufferedBio)
		goto fail;

	bufferedBio = BIO_push(bufferedBio, layerBio);
	if (!bufferedBio)
		goto fail;

	/* BIO takes over the layer reference at this point. */
	BIO_set_data(layerBio, layer);

	transport->frontBio = bufferedBio;

	return TRUE;

fail:
	if (layerBio)
		BIO_free_all(layerBio);

	return FALSE;
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

void transport_set_rdstls_mode(rdpTransport* transport, BOOL RdstlsMode)
{
	WINPR_ASSERT(transport);
	transport->RdstlsMode = RdstlsMode;
}

void transport_set_aad_mode(rdpTransport* transport, BOOL AadMode)
{
	WINPR_ASSERT(transport);
	transport->AadMode = AadMode;
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

	EnterCriticalSection(&(transport->ReadLock));
	EnterCriticalSection(&(transport->WriteLock));
	if (transport->tls)
	{
		freerdp_tls_free(transport->tls);
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

	if (transport->wst)
	{
		wst_free(transport->wst);
		transport->wst = NULL;
	}

	transport->frontBio = NULL;
	transport->layer = TRANSPORT_LAYER_TCP;
	transport->earlyUserAuth = FALSE;
	LeaveCriticalSection(&(transport->WriteLock));
	LeaveCriticalSection(&(transport->ReadLock));
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

	// transport->io.DataHandler = transport_data_handler;
	transport->io.TCPConnect = freerdp_tcp_default_connect;
	transport->io.TLSConnect = transport_default_connect_tls;
	transport->io.TLSAccept = transport_default_accept_tls;
	transport->io.TransportAttach = transport_default_attach;
	transport->io.TransportDisconnect = transport_default_disconnect;
	transport->io.ReadPdu = transport_default_read_pdu;
	transport->io.WritePdu = transport_default_write;
	transport->io.ReadBytes = transport_read_layer;
	transport->io.GetPublicKey = transport_default_get_public_key;
	transport->io.SetBlockingMode = transport_default_set_blocking_mode;
	transport->io.ConnectLayer = transport_default_connect_layer;
	transport->io.AttachLayer = transport_default_attach_layer;

	return transport;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	transport_free(transport);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void transport_free(rdpTransport* transport)
{
	if (!transport)
		return;

	transport_disconnect(transport);

	EnterCriticalSection(&(transport->ReadLock));
	if (transport->ReceiveBuffer)
		Stream_Release(transport->ReceiveBuffer);
	LeaveCriticalSection(&(transport->ReadLock));

	(void)StreamPool_WaitForReturn(transport->ReceivePool, INFINITE);

	EnterCriticalSection(&(transport->ReadLock));
	EnterCriticalSection(&(transport->WriteLock));

	nla_free(transport->nla);
	StreamPool_Free(transport->ReceivePool);
	(void)CloseHandle(transport->connectedEvent);
	(void)CloseHandle(transport->rereadEvent);
	(void)CloseHandle(transport->ioEvent);

	LeaveCriticalSection(&(transport->ReadLock));
	DeleteCriticalSection(&(transport->ReadLock));

	LeaveCriticalSection(&(transport->WriteLock));
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
	freerdp_tls_free(transport->tls);
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
	if (!transport->frontBio)
		return NULL;
	return StreamPool_Take(transport->ReceivePool, size);
}

UINT64 transport_get_bytes_sent(rdpTransport* transport, BOOL resetCount)
{
	UINT64 rc = 0;
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

void transport_set_early_user_auth_mode(rdpTransport* transport, BOOL EUAMode)
{
	WINPR_ASSERT(transport);
	transport->earlyUserAuth = EUAMode;
	WLog_Print(transport->log, WLOG_DEBUG, "Early User Auth Mode: %s", EUAMode ? "on" : "off");
}

rdpTransportLayer* transport_layer_new(WINPR_ATTR_UNUSED rdpTransport* transport,
                                       size_t contextSize)
{
	rdpTransportLayerInt* layer = (rdpTransportLayerInt*)calloc(1, sizeof(rdpTransportLayerInt));
	if (!layer)
		return NULL;

	if (contextSize)
	{
		layer->userContextShadowPtr = calloc(1, contextSize);
		if (!layer->userContextShadowPtr)
		{
			free(layer);
			return NULL;
		}
	}
	layer->pub.userContext = layer->userContextShadowPtr;

	return &layer->pub;
}

void transport_layer_free(rdpTransportLayer* layer)
{
	rdpTransportLayerInt* intern = (rdpTransportLayerInt*)layer;
	if (!layer)
		return;

	IFCALL(intern->pub.Close, intern->pub.userContext);
	free(intern->userContextShadowPtr);
	free(intern);
}

static int transport_layer_bio_write(BIO* bio, const char* buf, int size)
{
	if (!buf || !size)
		return 0;
	if (size < 0)
		return -1;

	WINPR_ASSERT(bio);

	rdpTransportLayer* layer = (rdpTransportLayer*)BIO_get_data(bio);
	if (!layer)
		return -1;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY);

	errno = 0;
	const int status = IFCALLRESULT(-1, layer->Write, layer->userContext, buf, size);

	if (status >= 0 && status < size)
		BIO_set_flags(bio, (BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY));

	switch (errno)
	{
		case EAGAIN:
			BIO_set_flags(bio, (BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY));
			break;
		default:
			break;
	}

	return status;
}

static int transport_layer_bio_read(BIO* bio, char* buf, int size)
{
	if (!buf || !size)
		return 0;
	if (size < 0)
		return -1;

	WINPR_ASSERT(bio);

	rdpTransportLayer* layer = (rdpTransportLayer*)BIO_get_data(bio);
	if (!layer)
		return -1;

	BIO_clear_flags(bio, BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY);
	errno = 0;
	const int status = IFCALLRESULT(-1, layer->Read, layer->userContext, buf, size);

	switch (errno)
	{
		case EAGAIN:
			BIO_set_flags(bio, (BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY));
			break;
		default:
			break;
	}

	return status;
}

static int transport_layer_bio_puts(WINPR_ATTR_UNUSED BIO* bio, WINPR_ATTR_UNUSED const char* str)
{
	return 1;
}

static int transport_layer_bio_gets(WINPR_ATTR_UNUSED BIO* bio, WINPR_ATTR_UNUSED char* str,
                                    WINPR_ATTR_UNUSED int size)
{
	return 1;
}

static long transport_layer_bio_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	WINPR_ASSERT(bio);

	rdpTransportLayer* layer = (rdpTransportLayer*)BIO_get_data(bio);
	if (!layer)
		return -1;

	int status = -1;
	switch (cmd)
	{
		case BIO_C_GET_EVENT:
			*((HANDLE*)arg2) = IFCALLRESULT(NULL, layer->GetEvent, layer->userContext);
			status = 1;
			break;

		case BIO_C_SET_NONBLOCK:
			status = 1;
			break;

		case BIO_C_WAIT_READ:
		{
			int timeout = (int)arg1;
			BOOL r = IFCALLRESULT(FALSE, layer->Wait, layer->userContext, FALSE,
			                      WINPR_ASSERTING_INT_CAST(uint32_t, timeout));
			/* Convert timeout to error return */
			if (!r)
			{
				errno = ETIMEDOUT;
				status = 0;
			}
			else
				status = 1;
			break;
		}

		case BIO_C_WAIT_WRITE:
		{
			int timeout = (int)arg1;
			BOOL r = IFCALLRESULT(FALSE, layer->Wait, layer->userContext, TRUE,
			                      WINPR_ASSERTING_INT_CAST(uint32_t, timeout));
			/* Convert timeout to error return */
			if (!r)
			{
				errno = ETIMEDOUT;
				status = 0;
			}
			else
				status = 1;
			break;
		}

		case BIO_CTRL_GET_CLOSE:
			status = BIO_get_shutdown(bio);
			break;

		case BIO_CTRL_SET_CLOSE:
			BIO_set_shutdown(bio, (int)arg1);
			status = 1;
			break;

		case BIO_CTRL_FLUSH:
		case BIO_CTRL_DUP:
			status = 1;
			break;

		default:
			status = 0;
			break;
	}

	return status;
}

static int transport_layer_bio_new(BIO* bio)
{
	WINPR_ASSERT(bio);

	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	BIO_set_init(bio, 1);
	return 1;
}

static int transport_layer_bio_free(BIO* bio)
{
	if (!bio)
		return 0;

	rdpTransportLayer* layer = (rdpTransportLayer*)BIO_get_data(bio);
	if (layer)
		transport_layer_free(layer);

	BIO_set_data(bio, NULL);
	BIO_set_init(bio, 0);
	BIO_set_flags(bio, 0);

	return 1;
}

BIO_METHOD* BIO_s_transport_layer(void)
{
	static BIO_METHOD* bio_methods = NULL;

	if (bio_methods == NULL)
	{
		if (!(bio_methods = BIO_meth_new(BIO_TYPE_SIMPLE, "TransportLayer")))
			return NULL;

		BIO_meth_set_write(bio_methods, transport_layer_bio_write);
		BIO_meth_set_read(bio_methods, transport_layer_bio_read);
		BIO_meth_set_puts(bio_methods, transport_layer_bio_puts);
		BIO_meth_set_gets(bio_methods, transport_layer_bio_gets);
		BIO_meth_set_ctrl(bio_methods, transport_layer_bio_ctrl);
		BIO_meth_set_create(bio_methods, transport_layer_bio_new);
		BIO_meth_set_destroy(bio_methods, transport_layer_bio_free);
	}

	return bio_methods;
}
