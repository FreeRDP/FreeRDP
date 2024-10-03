/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Websocket Transport
 *
 * Copyright 2023 Michael Saxl <mike@mwsys.mine.bz>
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
#include <freerdp/version.h>

#include <winpr/assert.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/winsock.h>
#include <winpr/cred.h>

#include "../settings.h"

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/utils/ringbuffer.h>
#include <freerdp/utils/smartcardlogon.h>

#include "wst.h"
#include "websocket.h"
#include "http.h"
#include "../credssp_auth.h"
#include "../proxy.h"
#include "../rdp.h"
#include "../../crypto/opensslcompat.h"
#include "rpc_fault.h"
#include "../utils.h"

#define TAG FREERDP_TAG("core.gateway.wst")

#define AUTH_PKG NEGO_SSP_NAME

struct rdp_wst
{
	rdpContext* context;
	BOOL attached;
	BIO* frontBio;
	rdpTls* tls;
	rdpCredsspAuth* auth;
	BOOL auth_required;
	HttpContext* http;
	CRITICAL_SECTION writeSection;
	char* gwhostname;
	uint16_t gwport;
	char* gwpath;
	websocket_context wscontext;
};

static const char arm_query_param[] = "%s%cClmTk=Bearer%%20%s&X-MS-User-Agent=FreeRDP%%2F3.0";

static BOOL wst_get_gateway_credentials(rdpContext* context, rdp_auth_reason reason)
{
	WINPR_ASSERT(context);
	freerdp* instance = context->instance;

	auth_status rc = utils_authenticate_gateway(instance, reason);
	switch (rc)
	{
		case AUTH_SUCCESS:
		case AUTH_SKIP:
			return TRUE;
		case AUTH_CANCELLED:
			freerdp_set_last_error_log(instance->context, FREERDP_ERROR_CONNECT_CANCELLED);
			return FALSE;
		case AUTH_NO_CREDENTIALS:
			WLog_INFO(TAG, "No credentials provided - using NULL identity");
			return TRUE;
		case AUTH_FAILED:
		default:
			return FALSE;
	}
}

static BOOL wst_auth_init(rdpWst* wst, rdpTls* tls, TCHAR* authPkg)
{
	WINPR_ASSERT(wst);
	WINPR_ASSERT(tls);
	WINPR_ASSERT(authPkg);

	rdpContext* context = wst->context;
	rdpSettings* settings = context->settings;
	SEC_WINNT_AUTH_IDENTITY identity = { 0 };
	int rc = 0;

	wst->auth_required = TRUE;
	if (!credssp_auth_init(wst->auth, authPkg, tls->Bindings))
		return FALSE;

	if (!wst_get_gateway_credentials(context, GW_AUTH_RDG))
		return FALSE;

	if (!identity_set_from_settings(&identity, settings, FreeRDP_GatewayUsername,
	                                FreeRDP_GatewayDomain, FreeRDP_GatewayPassword))
		return FALSE;

	SEC_WINNT_AUTH_IDENTITY* identityArg = (settings->GatewayUsername ? &identity : NULL);
	if (!credssp_auth_setup_client(wst->auth, "HTTP", wst->gwhostname, identityArg, NULL))
	{
		sspi_FreeAuthIdentity(&identity);
		return FALSE;
	}
	sspi_FreeAuthIdentity(&identity);

	credssp_auth_set_flags(wst->auth, ISC_REQ_CONFIDENTIALITY | ISC_REQ_MUTUAL_AUTH);

	rc = credssp_auth_authenticate(wst->auth);
	if (rc < 0)
		return FALSE;

	return TRUE;
}

static BOOL wst_set_auth_header(rdpCredsspAuth* auth, HttpRequest* request)
{
	WINPR_ASSERT(auth);
	WINPR_ASSERT(request);

	const SecBuffer* authToken = credssp_auth_get_output_buffer(auth);
	char* base64AuthToken = NULL;

	if (authToken)
	{
		if (authToken->cbBuffer > INT_MAX)
			return FALSE;

		base64AuthToken = crypto_base64_encode(authToken->pvBuffer, (int)authToken->cbBuffer);
	}

	if (base64AuthToken)
	{
		BOOL rc = http_request_set_auth_scheme(request, credssp_auth_pkg_name(auth)) &&
		          http_request_set_auth_param(request, base64AuthToken);
		free(base64AuthToken);

		if (!rc)
			return FALSE;
	}

	return TRUE;
}

static BOOL wst_recv_auth_token(rdpCredsspAuth* auth, HttpResponse* response)
{
	size_t len = 0;
	const char* token64 = NULL;
	size_t authTokenLength = 0;
	BYTE* authTokenData = NULL;
	SecBuffer authToken = { 0 };
	long StatusCode = 0;
	int rc = 0;

	if (!auth || !response)
		return FALSE;

	StatusCode = http_response_get_status_code(response);
	switch (StatusCode)
	{
		case HTTP_STATUS_DENIED:
		case HTTP_STATUS_OK:
			break;
		default:
			http_response_log_error_status(WLog_Get(TAG), WLOG_WARN, response);
			return FALSE;
	}

	token64 = http_response_get_auth_token(response, credssp_auth_pkg_name(auth));

	if (!token64)
		return FALSE;

	len = strlen(token64);

	crypto_base64_decode(token64, len, &authTokenData, &authTokenLength);

	if (authTokenLength && authTokenData)
	{
		authToken.pvBuffer = authTokenData;
		authToken.cbBuffer = authTokenLength;
		credssp_auth_take_input_buffer(auth, &authToken);
	}
	else
		free(authTokenData);

	rc = credssp_auth_authenticate(auth);
	if (rc < 0)
		return FALSE;

	return TRUE;
}

static BOOL wst_tls_connect(rdpWst* wst, rdpTls* tls, UINT32 timeout)
{
	WINPR_ASSERT(wst);
	WINPR_ASSERT(tls);
	int sockfd = 0;
	long status = 0;
	BIO* socketBio = NULL;
	BIO* bufferedBio = NULL;
	rdpSettings* settings = wst->context->settings;
	const char* peerHostname = wst->gwhostname;
	UINT16 peerPort = wst->gwport;
	const char* proxyUsername = NULL;
	const char* proxyPassword = NULL;
	BOOL isProxyConnection =
	    proxy_prepare(settings, &peerHostname, &peerPort, &proxyUsername, &proxyPassword);

	sockfd = freerdp_tcp_connect(wst->context, peerHostname, peerPort, timeout);

	WLog_DBG(TAG, "connecting to %s %d", peerHostname, peerPort);
	if (sockfd < 0)
	{
		return FALSE;
	}

	socketBio = BIO_new(BIO_s_simple_socket());

	if (!socketBio)
	{
		closesocket((SOCKET)sockfd);
		return FALSE;
	}

	BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
	{
		BIO_free_all(socketBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, socketBio);
	status = BIO_set_nonblock(bufferedBio, TRUE);

	if (isProxyConnection)
	{
		if (!proxy_connect(wst->context, bufferedBio, proxyUsername, proxyPassword, wst->gwhostname,
		                   wst->gwport))
		{
			BIO_free_all(bufferedBio);
			return FALSE;
		}
	}

	if (!status)
	{
		BIO_free_all(bufferedBio);
		return FALSE;
	}

	tls->hostname = wst->gwhostname;
	tls->port = MIN(UINT16_MAX, wst->gwport);
	tls->isGatewayTransport = TRUE;
	status = freerdp_tls_connect(tls, bufferedBio);
	if (status < 1)
	{
		rdpContext* context = wst->context;
		if (status < 0)
		{
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		}
		else
		{
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_CANCELLED);
		}

		return FALSE;
	}
	return (status >= 1);
}

static wStream* wst_build_http_request(rdpWst* wst)
{
	wStream* s = NULL;
	HttpRequest* request = NULL;
	const char* uri = NULL;

	if (!wst)
		return NULL;

	uri = http_context_get_uri(wst->http);
	request = http_request_new();

	if (!request)
		return NULL;

	if (!http_request_set_method(request, "GET") || !http_request_set_uri(request, uri))
		goto out;

	if (wst->auth_required)
	{
		if (!wst_set_auth_header(wst->auth, request))
			goto out;
	}
	else if (freerdp_settings_get_string(wst->context->settings, FreeRDP_GatewayHttpExtAuthBearer))
	{
		http_request_set_auth_scheme(request, "Bearer");
		http_request_set_auth_param(
		    request,
		    freerdp_settings_get_string(wst->context->settings, FreeRDP_GatewayHttpExtAuthBearer));
	}

	s = http_request_write(wst->http, request);
out:
	http_request_free(request);

	if (s)
		Stream_SealLength(s);

	return s;
}

static BOOL wst_send_http_request(rdpWst* wst, rdpTls* tls)
{
	WINPR_ASSERT(wst);
	WINPR_ASSERT(tls);

	wStream* s = wst_build_http_request(wst);
	if (!s)
		return FALSE;

	const size_t sz = Stream_Length(s);
	int status = freerdp_tls_write_all(tls, Stream_Buffer(s), sz);

	Stream_Free(s, TRUE);
	return (status >= 0);
}

static BOOL wst_handle_ok_or_forbidden(rdpWst* wst, HttpResponse** ppresponse, DWORD timeout,
                                       long* pStatusCode)
{
	WINPR_ASSERT(wst);
	WINPR_ASSERT(ppresponse);
	WINPR_ASSERT(*ppresponse);
	WINPR_ASSERT(pStatusCode);

	/* AVD returns a 403 response with a ARRAffinity cookie set. retry with that cookie */
	const char* affinity = http_response_get_setcookie(*ppresponse, "ARRAffinity");
	if (affinity && freerdp_settings_get_bool(wst->context->settings, FreeRDP_GatewayArmTransport))
	{
		WLog_DBG(TAG, "Got Affinity cookie %s", affinity);
		http_context_set_cookie(wst->http, "ARRAffinity", affinity);
		http_response_free(*ppresponse);
		*ppresponse = NULL;
		/* Terminate this connection and make a new one with the Loadbalancing Cookie */
		int fd = BIO_get_fd(wst->tls->bio, NULL);
		if (fd >= 0)
			closesocket((SOCKET)fd);
		freerdp_tls_free(wst->tls);

		wst->tls = freerdp_tls_new(wst->context);
		if (!wst_tls_connect(wst, wst->tls, timeout))
			return FALSE;

		if (freerdp_settings_get_string(wst->context->settings, FreeRDP_GatewayHttpExtAuthBearer) &&
		    freerdp_settings_get_bool(wst->context->settings, FreeRDP_GatewayArmTransport))
		{
			char* urlWithAuth = NULL;
			size_t urlLen = 0;
			char firstParam = (strchr(wst->gwpath, '?') != NULL) ? '&' : '?';
			winpr_asprintf(&urlWithAuth, &urlLen, arm_query_param, wst->gwpath, firstParam,
			               freerdp_settings_get_string(wst->context->settings,
			                                           FreeRDP_GatewayHttpExtAuthBearer));
			if (!urlWithAuth)
				return FALSE;
			free(wst->gwpath);
			wst->gwpath = urlWithAuth;
			if (!http_context_set_uri(wst->http, wst->gwpath))
				return FALSE;
			if (!http_context_enable_websocket_upgrade(wst->http, TRUE))
				return FALSE;
		}

		if (!wst_send_http_request(wst, wst->tls))
			return FALSE;
		*ppresponse = http_response_recv(wst->tls, TRUE);
		if (!*ppresponse)
			return FALSE;

		*pStatusCode = http_response_get_status_code(*ppresponse);
	}

	return TRUE;
}

static BOOL wst_handle_denied(rdpWst* wst, HttpResponse** ppresponse, long* pStatusCode)
{
	WINPR_ASSERT(wst);
	WINPR_ASSERT(ppresponse);
	WINPR_ASSERT(*ppresponse);
	WINPR_ASSERT(pStatusCode);

	if (freerdp_settings_get_string(wst->context->settings, FreeRDP_GatewayHttpExtAuthBearer))
		return FALSE;

	if (!wst_auth_init(wst, wst->tls, AUTH_PKG))
		return FALSE;
	if (!wst_send_http_request(wst, wst->tls))
		return FALSE;

	http_response_free(*ppresponse);
	*ppresponse = http_response_recv(wst->tls, TRUE);
	if (!*ppresponse)
		return FALSE;

	while (!credssp_auth_is_complete(wst->auth))
	{
		if (!wst_recv_auth_token(wst->auth, *ppresponse))
			return FALSE;

		if (credssp_auth_have_output_token(wst->auth))
		{
			if (!wst_send_http_request(wst, wst->tls))
				return FALSE;

			http_response_free(*ppresponse);
			*ppresponse = http_response_recv(wst->tls, TRUE);
			if (!*ppresponse)
				return FALSE;
		}
	}
	*pStatusCode = http_response_get_status_code(*ppresponse);
	return TRUE;
}

BOOL wst_connect(rdpWst* wst, DWORD timeout)
{
	HttpResponse* response = NULL;
	long StatusCode = 0;

	WINPR_ASSERT(wst);
	if (!wst_tls_connect(wst, wst->tls, timeout))
		return FALSE;
	if (freerdp_settings_get_bool(wst->context->settings, FreeRDP_GatewayArmTransport))
	{
		/*
		 * If we are directed here from a ARM Gateway first
		 * we need to get a Loadbalancing Cookie (ARRAffinity)
		 * This is done by a plain GET request on the websocket URL
		 */
		http_context_enable_websocket_upgrade(wst->http, FALSE);
	}
	if (!wst_send_http_request(wst, wst->tls))
		return FALSE;

	response = http_response_recv(wst->tls, TRUE);
	if (!response)
	{
		return FALSE;
	}

	StatusCode = http_response_get_status_code(response);
	BOOL success = TRUE;
	switch (StatusCode)
	{
		case HTTP_STATUS_FORBIDDEN:
		case HTTP_STATUS_OK:
			success = wst_handle_ok_or_forbidden(wst, &response, timeout, &StatusCode);
			break;

		case HTTP_STATUS_DENIED:
			success = wst_handle_denied(wst, &response, &StatusCode);
			break;
		default:
			http_response_log_error_status(WLog_Get(TAG), WLOG_WARN, response);
			break;
	}

	const BOOL isWebsocket = http_response_is_websocket(wst->http, response);
	http_response_free(response);
	if (!success)
		return FALSE;

	if (isWebsocket)
	{
		wst->wscontext.state = WebsocketStateOpcodeAndFin;
		wst->wscontext.responseStreamBuffer = NULL;
		return TRUE;
	}
	else
	{
		char buffer[64] = { 0 };
		WLog_ERR(TAG, "Unexpected HTTP status: %s",
		         freerdp_http_status_string_format(StatusCode, buffer, ARRAYSIZE(buffer)));
	}
	return FALSE;
}

DWORD wst_get_event_handles(rdpWst* wst, HANDLE* events, DWORD count)
{
	DWORD nCount = 0;
	WINPR_ASSERT(wst != NULL);

	if (wst->tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(wst->tls->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	return nCount;
}

static int wst_bio_write(BIO* bio, const char* buf, int num)
{
	int status = 0;
	WINPR_ASSERT(bio);
	WINPR_ASSERT(buf);

	rdpWst* wst = (rdpWst*)BIO_get_data(bio);
	WINPR_ASSERT(wst);
	BIO_clear_flags(bio, BIO_FLAGS_WRITE);
	EnterCriticalSection(&wst->writeSection);
	status = websocket_write(wst->tls->bio, (const BYTE*)buf, num, WebsocketBinaryOpcode);
	LeaveCriticalSection(&wst->writeSection);

	if (status < 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
		return -1;
	}
	else if (status < num)
	{
		BIO_set_flags(bio, BIO_FLAGS_WRITE);
		WSASetLastError(WSAEWOULDBLOCK);
	}
	else
	{
		BIO_set_flags(bio, BIO_FLAGS_WRITE);
	}

	return status;
}

static int wst_bio_read(BIO* bio, char* buf, int size)
{
	int status = 0;
	WINPR_ASSERT(bio);
	WINPR_ASSERT(buf);

	rdpWst* wst = (rdpWst*)BIO_get_data(bio);
	WINPR_ASSERT(wst);

	while (status <= 0)
	{
		status = websocket_read(wst->tls->bio, (BYTE*)buf, size, &wst->wscontext);
		if (status <= 0)
		{
			if (!BIO_should_retry(wst->tls->bio))
				return -1;
			return 0;
		}
	}

	if (status < 0)
	{
		BIO_clear_retry_flags(bio);
		return -1;
	}
	else if (status == 0)
	{
		BIO_set_retry_read(bio);
		WSASetLastError(WSAEWOULDBLOCK);
		return -1;
	}
	else
	{
		BIO_set_flags(bio, BIO_FLAGS_READ);
	}

	return status;
}

static int wst_bio_puts(BIO* bio, const char* str)
{
	WINPR_UNUSED(bio);
	WINPR_UNUSED(str);
	return -2;
}

// NOLINTNEXTLINE(readability-non-const-parameter)
static int wst_bio_gets(BIO* bio, char* str, int size)
{
	WINPR_UNUSED(bio);
	WINPR_UNUSED(str);
	WINPR_UNUSED(size);
	return -2;
}

static long wst_bio_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	long status = -1;
	WINPR_ASSERT(bio);

	rdpWst* wst = (rdpWst*)BIO_get_data(bio);
	WINPR_ASSERT(wst);
	rdpTls* tls = wst->tls;

	if (cmd == BIO_CTRL_FLUSH)
	{
		(void)BIO_flush(tls->bio);
		status = 1;
	}
	else if (cmd == BIO_C_SET_NONBLOCK)
	{
		status = 1;
	}
	else if (cmd == BIO_C_READ_BLOCKED)
	{
		status = BIO_read_blocked(tls->bio);
	}
	else if (cmd == BIO_C_WRITE_BLOCKED)
	{
		status = BIO_write_blocked(tls->bio);
	}
	else if (cmd == BIO_C_WAIT_READ)
	{
		int timeout = (int)arg1;

		if (BIO_read_blocked(tls->bio))
			return BIO_wait_read(tls->bio, timeout);
		status = 1;
	}
	else if (cmd == BIO_C_WAIT_WRITE)
	{
		int timeout = (int)arg1;

		if (BIO_write_blocked(tls->bio))
			status = BIO_wait_write(tls->bio, timeout);
		else
			status = 1;
	}
	else if (cmd == BIO_C_GET_EVENT || cmd == BIO_C_GET_FD)
	{
		status = BIO_ctrl(tls->bio, cmd, arg1, arg2);
	}
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	else if (cmd == BIO_CTRL_GET_KTLS_SEND)
	{
		/* Even though BIO_get_ktls_send says that returning negative values is valid
		 * openssl internal sources are full of if(!BIO_get_ktls_send && ) stuff. This has some
		 * nasty sideeffects. return 0 as proper no KTLS offloading flag
		 */
		status = 0;
	}
	else if (cmd == BIO_CTRL_GET_KTLS_RECV)
	{
		/* Even though BIO_get_ktls_recv says that returning negative values is valid
		 * there is no reason to trust  trust negative values are implemented right everywhere
		 */
		status = 0;
	}
#endif
	return status;
}

static int wst_bio_new(BIO* bio)
{
	BIO_set_init(bio, 1);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	return 1;
}

static int wst_bio_free(BIO* bio)
{
	WINPR_UNUSED(bio);
	return 1;
}

static BIO_METHOD* BIO_s_wst(void)
{
	static BIO_METHOD* bio_methods = NULL;

	if (bio_methods == NULL)
	{
		if (!(bio_methods = BIO_meth_new(BIO_TYPE_TSG, "WSTransport")))
			return NULL;

		BIO_meth_set_write(bio_methods, wst_bio_write);
		BIO_meth_set_read(bio_methods, wst_bio_read);
		BIO_meth_set_puts(bio_methods, wst_bio_puts);
		BIO_meth_set_gets(bio_methods, wst_bio_gets);
		BIO_meth_set_ctrl(bio_methods, wst_bio_ctrl);
		BIO_meth_set_create(bio_methods, wst_bio_new);
		BIO_meth_set_destroy(bio_methods, wst_bio_free);
	}

	return bio_methods;
}

static BOOL wst_parse_url(rdpWst* wst, const char* url)
{
	const char* hostStart = NULL;
	const char* pos = NULL;
	WINPR_ASSERT(wst);
	WINPR_ASSERT(url);

	free(wst->gwhostname);
	wst->gwhostname = NULL;
	free(wst->gwpath);
	wst->gwpath = NULL;

	if (strncmp("wss://", url, 6) != 0)
	{
		if (strncmp("https://", url, 8) != 0)
		{
			WLog_ERR(TAG, "Websocket URL is invalid. Only wss:// or https:// URLs are supported");
			return FALSE;
		}
		else
			hostStart = url + 8;
	}
	else
		hostStart = url + 6;

	pos = hostStart;
	while (*pos != '\0' && *pos != ':' && *pos != '/')
		pos++;
	free(wst->gwhostname);
	wst->gwhostname = NULL;
	if (pos - hostStart == 0)
		return FALSE;
	wst->gwhostname = malloc(sizeof(char) * (pos - hostStart + 1));
	if (!wst->gwhostname)
		return FALSE;
	strncpy(wst->gwhostname, hostStart, (pos - hostStart));
	wst->gwhostname[pos - hostStart] = '\0';

	if (*pos == ':')
	{
		char port[6];
		char* portNumberEnd = NULL;
		pos++;
		const char* portStart = pos;
		while (*pos != '\0' && *pos != '/')
			pos++;
		if (pos - portStart > 5 || pos - portStart == 0)
			return FALSE;
		strncpy(port, portStart, (pos - portStart));
		port[pos - portStart] = '\0';
		long _p = strtol(port, &portNumberEnd, 10);
		if (portNumberEnd && (*portNumberEnd == '\0') && (_p > 0) && (_p <= UINT16_MAX))
			wst->gwport = (uint16_t)_p;
		else
			return FALSE;
	}
	else
		wst->gwport = 443;
	wst->gwpath = _strdup(pos);
	if (!wst->gwpath)
		return FALSE;
	return TRUE;
}

rdpWst* wst_new(rdpContext* context)
{
	rdpWst* wst = NULL;

	if (!context)
		return NULL;

	wst = (rdpWst*)calloc(1, sizeof(rdpWst));

	if (wst)
	{
		wst->context = context;

		wst->gwhostname = NULL;
		wst->gwport = 443;
		wst->gwpath = NULL;

		if (!wst_parse_url(wst, context->settings->GatewayUrl))
			goto wst_alloc_error;

		wst->tls = freerdp_tls_new(wst->context);
		if (!wst->tls)
			goto wst_alloc_error;

		wst->http = http_context_new();

		if (!wst->http)
			goto wst_alloc_error;

		if (!http_context_set_uri(wst->http, wst->gwpath) ||
		    !http_context_set_accept(wst->http, "*/*") ||
		    !http_context_set_cache_control(wst->http, "no-cache") ||
		    !http_context_set_pragma(wst->http, "no-cache") ||
		    !http_context_set_connection(wst->http, "Keep-Alive") ||
		    !http_context_set_user_agent(wst->http, FREERDP_USER_AGENT) ||
		    !http_context_set_x_ms_user_agent(wst->http, FREERDP_USER_AGENT) ||
		    !http_context_set_host(wst->http, wst->gwhostname) ||
		    !http_context_enable_websocket_upgrade(wst->http, TRUE))
		{
			goto wst_alloc_error;
		}

		wst->frontBio = BIO_new(BIO_s_wst());

		if (!wst->frontBio)
			goto wst_alloc_error;

		BIO_set_data(wst->frontBio, wst);
		InitializeCriticalSection(&wst->writeSection);
		wst->auth = credssp_auth_new(context);
		if (!wst->auth)
			goto wst_alloc_error;
	}

	return wst;
wst_alloc_error:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	wst_free(wst);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void wst_free(rdpWst* wst)
{
	if (!wst)
		return;

	freerdp_tls_free(wst->tls);
	http_context_free(wst->http);
	credssp_auth_free(wst->auth);
	free(wst->gwhostname);
	free(wst->gwpath);

	if (!wst->attached)
		BIO_free_all(wst->frontBio);

	DeleteCriticalSection(&wst->writeSection);

	if (wst->wscontext.responseStreamBuffer != NULL)
		Stream_Free(wst->wscontext.responseStreamBuffer, TRUE);

	free(wst);
}

BIO* wst_get_front_bio_and_take_ownership(rdpWst* wst)
{
	if (!wst)
		return NULL;

	wst->attached = TRUE;
	return wst->frontBio;
}
