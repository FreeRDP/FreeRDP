/**
* FreeRDP: A Remote Desktop Protocol Implementation
* Remote Desktop Gateway (RDG)
*
* Copyright 2015 Denis Vincent <dvincent@devolutions.net>
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
#include <winpr/winhttp.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/utils/ringbuffer.h>

#include "rdg.h"
#include "../proxy.h"
#include "../rdp.h"
#include "../../crypto/opensslcompat.h"

#include "http.h"
#include "ntlm.h"

#define TAG FREERDP_TAG("core.gateway.rdg")


/* HTTP channel response fields present flags. */
#define HTTP_CHANNEL_RESPONSE_FIELD_CHANNELID 0x1
#define HTTP_CHANNEL_RESPONSE_OPTIONAL 0x2
#define HTTP_CHANNEL_RESPONSE_FIELD_UDPPORT 0x4

/* HTTP extended auth. */
#define HTTP_EXTENDED_AUTH_NONE 0x0
#define HTTP_EXTENDED_AUTH_SC 0x1   /* Smart card authentication. */
#define HTTP_EXTENDED_AUTH_PAA 0x02   /* Pluggable authentication. */
#define HTTP_EXTENDED_AUTH_SSPI_NTLM 0x04   /* NTLM extended authentication. */

/* HTTP packet types. */
#define PKT_TYPE_HANDSHAKE_REQUEST 0x1
#define PKT_TYPE_HANDSHAKE_RESPONSE 0x2
#define PKT_TYPE_EXTENDED_AUTH_MSG 0x3
#define PKT_TYPE_TUNNEL_CREATE 0x4
#define PKT_TYPE_TUNNEL_RESPONSE 0x5
#define PKT_TYPE_TUNNEL_AUTH 0x6
#define PKT_TYPE_TUNNEL_AUTH_RESPONSE 0x7
#define PKT_TYPE_CHANNEL_CREATE 0x8
#define PKT_TYPE_CHANNEL_RESPONSE 0x9
#define PKT_TYPE_DATA 0xA
#define PKT_TYPE_SERVICE_MESSAGE 0xB
#define PKT_TYPE_REAUTH_MESSAGE 0xC
#define PKT_TYPE_KEEPALIVE 0xD
#define PKT_TYPE_CLOSE_CHANNEL 0x10
#define PKT_TYPE_CLOSE_CHANNEL_RESPONSE 0x11

/* HTTP tunnel auth fields present flags. */
#define HTTP_TUNNEL_AUTH_FIELD_SOH 0x1

/* HTTP tunnel auth response fields present flags. */
#define HTTP_TUNNEL_AUTH_RESPONSE_FIELD_REDIR_FLAGS 0x1
#define HTTP_TUNNEL_AUTH_RESPONSE_FIELD_IDLE_TIMEOUT 0x2
#define HTTP_TUNNEL_AUTH_RESPONSE_FIELD_SOH_RESPONSE 0x4

/* HTTP tunnel packet fields present flags. */
#define HTTP_TUNNEL_PACKET_FIELD_PAA_COOKIE 0x1
#define HTTP_TUNNEL_PACKET_FIELD_REAUTH 0x2

/* HTTP tunnel redir flags. */
#define HTTP_TUNNEL_REDIR_ENABLE_ALL 0x80000000
#define HTTP_TUNNEL_REDIR_DISABLE_ALL 0x40000000
#define HTTP_TUNNEL_REDIR_DISABLE_DRIVE 0x1
#define HTTP_TUNNEL_REDIR_DISABLE_PRINTER 0x2
#define HTTP_TUNNEL_REDIR_DISABLE_PORT 0x4
#define HTTP_TUNNEL_REDIR_DISABLE_CLIPBOARD 0x8
#define HTTP_TUNNEL_REDIR_DISABLE_PNP 0x10

/* HTTP tunnel response fields present flags. */
#define HTTP_TUNNEL_RESPONSE_FIELD_TUNNEL_ID 0x1
#define HTTP_TUNNEL_RESPONSE_FIELD_CAPS 0x2
#define HTTP_TUNNEL_RESPONSE_FIELD_SOH_REQ 0x4
#define HTTP_TUNNEL_RESPONSE_FIELD_CONSENT_MSG 0x10

/* HTTP capability type enumeration. */
#define HTTP_CAPABILITY_TYPE_QUAR_SOH 0x1
#define HTTP_CAPABILITY_IDLE_TIMEOUT 0x2
#define HTTP_CAPABILITY_MESSAGING_CONSENT_SIGN 0x4
#define HTTP_CAPABILITY_MESSAGING_SERVICE_MSG 0x8
#define HTTP_CAPABILITY_REAUTH 0x10
#define HTTP_CAPABILITY_UDP_TRANSPORT 0x20

enum
{
	RDG_CLIENT_STATE_INITIAL,
	RDG_CLIENT_STATE_HANDSHAKE,
	RDG_CLIENT_STATE_TUNNEL_CREATE,
	RDG_CLIENT_STATE_TUNNEL_AUTHORIZE,
	RDG_CLIENT_STATE_CHANNEL_CREATE,
	RDG_CLIENT_STATE_OPENED,
};

struct rdp_rdg
{
	rdpContext* context;
	rdpSettings* settings;
	BIO* frontBio;
	rdpTls* tlsIn;
	rdpTls* tlsOut;
	rdpNtlm* ntlm;
	HttpContext* http;
	CRITICAL_SECTION writeSection;

	UUID guid;

	int state;
	UINT16 packetRemainingCount;
	int timeout;
	UINT16 extAuth;
};

#pragma pack(push, 1)

typedef struct rdg_packet_header
{
	UINT16 type;
	UINT16 reserved;
	UINT32 packetLength;
} RdgPacketHeader;

#pragma pack(pop)

static BOOL rdg_write_packet(rdpRdg* rdg, wStream* sPacket)
{
	int status;
	wStream* sChunk;
	char chunkSize[11];
	sprintf_s(chunkSize, sizeof(chunkSize), "%"PRIXz"\r\n", Stream_Length(sPacket));
	sChunk = Stream_New(NULL, strlen(chunkSize) + Stream_Length(sPacket) + 2);

	if (!sChunk)
		return FALSE;

	Stream_Write(sChunk, chunkSize, strlen(chunkSize));
	Stream_Write(sChunk, Stream_Buffer(sPacket), Stream_Length(sPacket));
	Stream_Write(sChunk, "\r\n", 2);
	Stream_SealLength(sChunk);
	status = tls_write_all(rdg->tlsIn, Stream_Buffer(sChunk), Stream_Length(sChunk));
	Stream_Free(sChunk, TRUE);

	if (status < 0)
		return FALSE;

	return TRUE;
}

static BOOL rdg_read_all(rdpTls* tls, BYTE* buffer, int size)
{
	int status;
	int readCount = 0;

	while (readCount < size)
	{
		status = BIO_read(tls->bio, buffer, size - readCount);

		if (status <= 0)
		{
			if (!BIO_should_retry(tls->bio))
				return FALSE;

			continue;
		}

		readCount += status;
	}

	return TRUE;
}

static wStream* rdg_receive_packet(rdpRdg* rdg)
{
	wStream* s;
	size_t packetLength;
	s = Stream_New(NULL, 1024);

	if (!s)
		return NULL;

	if (!rdg_read_all(rdg->tlsOut, Stream_Buffer(s), sizeof(RdgPacketHeader)))
	{
		Stream_Free(s, TRUE);
		return NULL;
	}

	Stream_Seek(s, 4);
	Stream_Read_UINT32(s, packetLength);

	if (!Stream_EnsureCapacity(s, packetLength))
	{
		Stream_Free(s, TRUE);
		return NULL;
	}

	if (!rdg_read_all(rdg->tlsOut, Stream_Buffer(s) + sizeof(RdgPacketHeader),
	                  packetLength - sizeof(RdgPacketHeader)))
	{
		Stream_Free(s, TRUE);
		return NULL;
	}

	Stream_SetLength(s, packetLength);
	return s;
}

static BOOL rdg_send_handshake(rdpRdg* rdg)
{
	wStream* s;
	BOOL status;
	s = Stream_New(NULL, 14);

	if (!s)
		return FALSE;

	Stream_Write_UINT16(s, PKT_TYPE_HANDSHAKE_REQUEST); /* Type (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Reserved (2 bytes) */
	Stream_Write_UINT32(s, 14); /* PacketLength (4 bytes) */
	Stream_Write_UINT8(s, 1); /* VersionMajor (1 byte) */
	Stream_Write_UINT8(s, 0); /* VersionMinor (1 byte) */
	Stream_Write_UINT16(s, 0); /* ClientVersion (2 bytes), must be 0 */
	Stream_Write_UINT16(s, rdg->extAuth); /* ExtendedAuthentication (2 bytes) */
	Stream_SealLength(s);
	status = rdg_write_packet(rdg, s);
	Stream_Free(s, TRUE);

	if (status)
	{
		rdg->state = RDG_CLIENT_STATE_HANDSHAKE;
	}

	return status;
}

static BOOL rdg_send_tunnel_request(rdpRdg* rdg)
{
	wStream* s;
	BOOL status;
	UINT32 packetSize = 16;
	UINT16 fieldsPresent = 0;
	WCHAR* PAACookie = NULL;
	UINT16 PAACookieLen = 0;

	if (rdg->extAuth == HTTP_EXTENDED_AUTH_PAA)
	{
		PAACookieLen = ConvertToUnicode(CP_UTF8, 0, rdg->settings->GatewayAccessToken, -1, &PAACookie, 0);

		if (!PAACookie)
			return FALSE;

		packetSize += 2 + PAACookieLen * sizeof(WCHAR);
		fieldsPresent = HTTP_TUNNEL_PACKET_FIELD_PAA_COOKIE;
	}

	s = Stream_New(NULL, packetSize);

	if (!s)
	{
		free(PAACookie);
		return FALSE;
	}

	Stream_Write_UINT16(s, PKT_TYPE_TUNNEL_CREATE); /* Type (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Reserved (2 bytes) */
	Stream_Write_UINT32(s, packetSize); /* PacketLength (4 bytes) */
	Stream_Write_UINT32(s, HTTP_CAPABILITY_TYPE_QUAR_SOH); /* CapabilityFlags (4 bytes) */
	Stream_Write_UINT16(s, fieldsPresent); /* FieldsPresent (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Reserved (2 bytes), must be 0 */

	if (PAACookie)
	{
		Stream_Write_UINT16(s, PAACookieLen * 2); /* PAA cookie string length */
		Stream_Write_UTF16_String(s, PAACookie, PAACookieLen);
	}

	Stream_SealLength(s);
	status = rdg_write_packet(rdg, s);
	Stream_Free(s, TRUE);
	free(PAACookie);

	if (status)
	{
		rdg->state = RDG_CLIENT_STATE_TUNNEL_CREATE;
	}

	return status;
}

static BOOL rdg_send_tunnel_authorization(rdpRdg* rdg)
{
	int i;
	wStream* s;
	BOOL status;
	WCHAR* clientName = NULL;
	UINT16 clientNameLen;
	UINT32 packetSize;
	clientNameLen = ConvertToUnicode(CP_UTF8, 0, rdg->settings->ClientHostname, -1, &clientName, 0);

	if (!clientName)
		return FALSE;

	packetSize = 12 + clientNameLen * sizeof(WCHAR);
	s = Stream_New(NULL, packetSize);

	if (!s)
	{
		free(clientName);
		return FALSE;
	}

	Stream_Write_UINT16(s, PKT_TYPE_TUNNEL_AUTH); /* Type (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Reserved (2 bytes) */
	Stream_Write_UINT32(s, packetSize); /* PacketLength (4 bytes) */
	Stream_Write_UINT16(s, 0); /* FieldsPresent (2 bytes) */
	Stream_Write_UINT16(s, clientNameLen * 2); /* Client name string length */

	for (i = 0; i < clientNameLen; i++)
		Stream_Write_UINT16(s, clientName[i]);

	Stream_SealLength(s);
	status = rdg_write_packet(rdg, s);
	Stream_Free(s, TRUE);
	free(clientName);

	if (status)
	{
		rdg->state = RDG_CLIENT_STATE_TUNNEL_AUTHORIZE;
	}

	return status;
}

static BOOL rdg_send_channel_create(rdpRdg* rdg)
{
	int i;
	wStream* s;
	BOOL status;
	char* serverName = rdg->settings->ServerHostname;
	UINT16 serverNameLen = strlen(serverName) + 1;
	UINT32 packetSize = 16 + serverNameLen * 2;
	s = Stream_New(NULL, packetSize);

	if (!s)
		return FALSE;

	Stream_Write_UINT16(s, PKT_TYPE_CHANNEL_CREATE); /* Type (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Reserved (2 bytes) */
	Stream_Write_UINT32(s, packetSize); /* PacketLength (4 bytes) */
	Stream_Write_UINT8(s, 1); /* Number of resources. (1 byte) */
	Stream_Write_UINT8(s, 0); /* Number of alternative resources (1 byte) */
	Stream_Write_UINT16(s, rdg->settings->ServerPort); /* Resource port (2 bytes) */
	Stream_Write_UINT16(s, 3); /* Protocol number (2 bytes) */
	Stream_Write_UINT16(s, serverNameLen * 2);

	for (i = 0; i < serverNameLen; i++)
	{
		Stream_Write_UINT16(s, serverName[i]);
	}

	Stream_SealLength(s);
	status = rdg_write_packet(rdg, s);
	Stream_Free(s, TRUE);

	if (status)
	{
		rdg->state = RDG_CLIENT_STATE_CHANNEL_CREATE;
	}

	return status;
}

static BOOL rdg_set_ntlm_auth_header(rdpNtlm* ntlm, HttpRequest* request)
{
	const SecBuffer* ntlmToken = ntlm_client_get_output_buffer(ntlm);
	char* base64NtlmToken = NULL;

	if (ntlmToken)
		base64NtlmToken = crypto_base64_encode(ntlmToken->pvBuffer, ntlmToken->cbBuffer);

	if (base64NtlmToken)
	{
		BOOL rc = http_request_set_auth_param(request, base64NtlmToken);
		free(base64NtlmToken);

		if (!rc || !http_request_set_auth_scheme(request, "NTLM"))
			return FALSE;
	}

	return TRUE;
}

static wStream* rdg_build_http_request(rdpRdg* rdg, const char* method,
                                       const char* transferEncoding)
{
	wStream* s = NULL;
	HttpRequest* request = NULL;
	assert(method != NULL);
	request = http_request_new();

	if (!request)
		return NULL;

	{
		const char* uri = http_context_get_uri(rdg->http);

		if (!http_request_set_method(request, method))
			goto out;

		if (!http_request_set_uri(request, uri))
			goto out;
	}

	if (rdg->ntlm)
	{
		if (!rdg_set_ntlm_auth_header(rdg->ntlm, request))
			goto out;
	}

	if (transferEncoding)
	{
		http_request_set_transfer_encoding(request, transferEncoding);
	}

	s = http_request_write(rdg->http, request);
out:
	http_request_free(request);

	if (s)
		Stream_SealLength(s);

	return s;
}

static BOOL rdg_handle_ntlm_challenge(rdpNtlm* ntlm, HttpResponse* response)
{
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	const long StatusCode = http_response_get_status_code(response);
	const char* token64 = http_response_get_auth_token(response, "NTLM");

	if (StatusCode != HTTP_STATUS_DENIED)
	{
		WLog_DBG(TAG, "Unexpected NTLM challenge HTTP status: %d",
		         StatusCode);
		return FALSE;
	}

	if (!token64)
		return FALSE;

	crypto_base64_decode(token64, strlen(token64), &ntlmTokenData, &ntlmTokenLength);

	if (ntlmTokenData && ntlmTokenLength)
	{
		if (!ntlm_client_set_input_buffer(ntlm, FALSE, ntlmTokenData, ntlmTokenLength))
			return FALSE;
	}

	ntlm_authenticate(ntlm);
	return TRUE;
}

static BOOL rdg_skip_seed_payload(rdpTls* tls, size_t lastResponseLength)
{
	BYTE seed_payload[10];

	/* Per [MS-TSGU] 3.3.5.1 step 4, after final OK response RDG server sends
	 * random "seed" payload of limited size. In practice it's 10 bytes.
	 */
	if (lastResponseLength < sizeof(seed_payload))
	{
		if (!rdg_read_all(tls, seed_payload,
		                  sizeof(seed_payload) - lastResponseLength))
		{
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL rdg_process_handshake_response(rdpRdg* rdg, wStream* s)
{
	HRESULT errorCode;
	WLog_DBG(TAG, "Handshake response received");

	if (rdg->state != RDG_CLIENT_STATE_HANDSHAKE)
	{
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;

	Stream_Seek(s, 8);
	Stream_Read_UINT32(s, errorCode);

	if (FAILED(errorCode))
	{
		WLog_DBG(TAG, "Handshake error %x", errorCode);
		return FALSE;
	}

	return rdg_send_tunnel_request(rdg);
}

static BOOL rdg_process_tunnel_response(rdpRdg* rdg, wStream* s)
{
	HRESULT errorCode;
	WLog_DBG(TAG, "Tunnel response received");

	if (rdg->state != RDG_CLIENT_STATE_TUNNEL_CREATE)
	{
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < 14)
		return FALSE;

	Stream_Seek(s, 10);
	Stream_Read_UINT32(s, errorCode);

	if (FAILED(errorCode))
	{
		WLog_DBG(TAG, "Tunnel creation error %x", errorCode);
		return FALSE;
	}

	return rdg_send_tunnel_authorization(rdg);
}

static BOOL rdg_process_tunnel_authorization_response(rdpRdg* rdg, wStream* s)
{
	HRESULT errorCode;
	WLog_DBG(TAG, "Tunnel authorization received");

	if (rdg->state != RDG_CLIENT_STATE_TUNNEL_AUTHORIZE)
	{
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;

	Stream_Seek(s, 8);
	Stream_Read_UINT32(s, errorCode);

	if (FAILED(errorCode))
	{
		WLog_DBG(TAG, "Tunnel authorization error %x", errorCode);
		return FALSE;
	}

	return rdg_send_channel_create(rdg);
}

static BOOL rdg_process_channel_response(rdpRdg* rdg, wStream* s)
{
	HRESULT errorCode;
	WLog_DBG(TAG, "Channel response received");

	if (rdg->state != RDG_CLIENT_STATE_CHANNEL_CREATE)
	{
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;

	Stream_Seek(s, 8);
	Stream_Read_UINT32(s, errorCode);

	if (FAILED(errorCode))
	{
		WLog_DBG(TAG, "Channel error %x", errorCode);
		return FALSE;
	}

	rdg->state = RDG_CLIENT_STATE_OPENED;
	return TRUE;
}

static BOOL rdg_process_packet(rdpRdg* rdg, wStream* s)
{
	BOOL status = TRUE;
	UINT16 type;
	Stream_SetPosition(s, 0);

	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Peek_UINT16(s, type);

	switch (type)
	{
		case PKT_TYPE_HANDSHAKE_RESPONSE:
			status = rdg_process_handshake_response(rdg, s);
			break;

		case PKT_TYPE_TUNNEL_RESPONSE:
			status = rdg_process_tunnel_response(rdg, s);
			break;

		case PKT_TYPE_TUNNEL_AUTH_RESPONSE:
			status = rdg_process_tunnel_authorization_response(rdg, s);
			break;

		case PKT_TYPE_CHANNEL_RESPONSE:
			status = rdg_process_channel_response(rdg, s);
			break;

		case PKT_TYPE_DATA:
			assert(FALSE);
			return FALSE;
	}

	return status;
}

DWORD rdg_get_event_handles(rdpRdg* rdg, HANDLE* events, DWORD count)
{
	DWORD nCount = 0;
	assert(rdg != NULL);

	if (rdg->tlsOut && rdg->tlsOut->bio)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(rdg->tlsOut->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	if (rdg->tlsIn && rdg->tlsIn->bio)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(rdg->tlsIn->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	return nCount;
}

static BOOL rdg_get_gateway_credentials(rdpContext* context)
{
	rdpSettings* settings = context->settings;
	freerdp* instance = context->instance;

	if (!settings->GatewayPassword || !settings->GatewayUsername ||
	    !strlen(settings->GatewayPassword) || !strlen(settings->GatewayUsername))
	{
		if (instance->GatewayAuthenticate)
		{
			BOOL proceed = instance->GatewayAuthenticate(instance, &settings->GatewayUsername,
			               &settings->GatewayPassword, &settings->GatewayDomain);

			if (!proceed)
			{
				freerdp_set_last_error(context, FREERDP_ERROR_CONNECT_CANCELLED);
				return FALSE;
			}

			if (settings->GatewayUseSameCredentials)
			{
				if (settings->GatewayUsername)
				{
					free(settings->Username);

					if (!(settings->Username = _strdup(settings->GatewayUsername)))
						return FALSE;
				}

				if (settings->GatewayDomain)
				{
					free(settings->Domain);

					if (!(settings->Domain = _strdup(settings->GatewayDomain)))
						return FALSE;
				}

				if (settings->GatewayPassword)
				{
					free(settings->Password);

					if (!(settings->Password = _strdup(settings->GatewayPassword)))
						return FALSE;
				}
			}
		}
	}

	return TRUE;
}

static BOOL rdg_ntlm_init(rdpRdg* rdg, rdpTls* tls)
{
	rdpContext* context = rdg->context;
	rdpSettings* settings = context->settings;
	rdg->ntlm = ntlm_new();

	if (!rdg->ntlm)
		return FALSE;

	if (!rdg_get_gateway_credentials(context))
		return FALSE;

	if (!ntlm_client_init(rdg->ntlm, TRUE, settings->GatewayUsername, settings->GatewayDomain,
	                      settings->GatewayPassword, tls->Bindings))
		return FALSE;

	if (!ntlm_client_make_spn(rdg->ntlm, _T("HTTP"), settings->GatewayHostname))
		return FALSE;

	if (!ntlm_authenticate(rdg->ntlm))
		return FALSE;

	return TRUE;
}

static BOOL rdg_send_http_request(rdpRdg* rdg, rdpTls* tls, const char* method,
                                  const char* transferEncoding)
{
	wStream* s = NULL;
	int status;
	s = rdg_build_http_request(rdg, method, transferEncoding);

	if (!s)
		return FALSE;

	status = tls_write_all(tls, Stream_Buffer(s), Stream_Length(s));
	Stream_Free(s, TRUE);
	return (status >= 0);
}

static BOOL rdg_tls_connect(rdpRdg* rdg, rdpTls* tls, const char* peerAddress, int timeout)
{
	int sockfd = 0;
	long status = 0;
	BIO* socketBio = NULL;
	BIO* bufferedBio = NULL;
	rdpSettings* settings = rdg->settings;
	const char* peerHostname = settings->GatewayHostname;
	UINT16 peerPort = settings->GatewayPort;
	const char* proxyUsername, *proxyPassword;
	BOOL isProxyConnection = proxy_prepare(settings, &peerHostname, &peerPort, &proxyUsername,
	                                       &proxyPassword);
	sockfd = freerdp_tcp_connect(rdg->context, settings,
	                             peerAddress ? peerAddress : peerHostname,
	                             peerPort, timeout);

	if (sockfd < 0)
	{
		return FALSE;
	}

	socketBio = BIO_new(BIO_s_simple_socket());

	if (!socketBio)
	{
		closesocket(sockfd);
		return FALSE;
	}

	BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
	{
		closesocket(sockfd);
		BIO_free(socketBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, socketBio);
	status = BIO_set_nonblock(bufferedBio, TRUE);

	if (isProxyConnection)
	{
		if (!proxy_connect(settings, bufferedBio, proxyUsername, proxyPassword, settings->GatewayHostname,
		                   settings->GatewayPort))
			return FALSE;
	}

	if (!status)
	{
		BIO_free_all(bufferedBio);
		return FALSE;
	}

	tls->hostname = settings->GatewayHostname;
	tls->port = settings->GatewayPort;
	tls->isGatewayTransport = TRUE;
	status = tls_connect(tls, bufferedBio);
	return (status >= 1);
}

static BOOL rdg_establish_data_connection(rdpRdg* rdg, rdpTls* tls,
        const char* method, const char* peerAddress, int timeout, BOOL* rpcFallback)
{
	HttpResponse* response = NULL;
	long StatusCode = -1;
	SSIZE_T bodyLength;

	if (!rdg_tls_connect(rdg, tls, peerAddress, timeout))
		return FALSE;

	if (rdg->extAuth == HTTP_EXTENDED_AUTH_NONE)
	{
		if (!rdg_ntlm_init(rdg, tls))
			return FALSE;

		if (!rdg_send_http_request(rdg, tls, method, NULL))
			return FALSE;

		response = http_response_recv(tls);

		if (!response)
			return FALSE;

		StatusCode = http_response_get_status_code(response);

		if (StatusCode == HTTP_STATUS_NOT_FOUND)
		{
			WLog_INFO(TAG, "RD Gateway does not support HTTP transport.");

			if (rpcFallback) *rpcFallback = TRUE;

			http_response_free(response);
			return FALSE;
		}

		if (!rdg_handle_ntlm_challenge(rdg->ntlm, response))
		{
			http_response_free(response);
			return FALSE;
		}

		http_response_free(response);
	}

	if (!rdg_send_http_request(rdg, tls, method, NULL))
		return FALSE;

	ntlm_free(rdg->ntlm);
	rdg->ntlm = NULL;
	response = http_response_recv(tls);

	if (!response)
		return FALSE;

	bodyLength = http_response_get_body_length(response);
	http_response_free(response);
	WLog_DBG(TAG, "%s authorization result: %ld", method, StatusCode);

	if (StatusCode != HTTP_STATUS_OK)
		return FALSE;

	if (strcmp(method, "RDG_OUT_DATA") == 0)
	{
		if (!rdg_skip_seed_payload(tls, bodyLength))
			return FALSE;
	}
	else
	{
		if (!rdg_send_http_request(rdg, tls, method, "chunked"))
			return FALSE;
	}

	return TRUE;
}

static BOOL rdg_tunnel_connect(rdpRdg* rdg)
{
	BOOL status;
	wStream* s;
	rdg_send_handshake(rdg);

	while (rdg->state < RDG_CLIENT_STATE_OPENED)
	{
		status = FALSE;
		s = rdg_receive_packet(rdg);

		if (s)
		{
			status = rdg_process_packet(rdg, s);
			Stream_Free(s, TRUE);
		}

		if (!status)
		{
			rdg->context->rdp->transport->layer = TRANSPORT_LAYER_CLOSED;
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rdg_connect(rdpRdg* rdg, int timeout, BOOL* rpcFallback)
{
	BOOL status;
	SOCKET outConnSocket = 0;
	char* peerAddress = NULL;
	assert(rdg != NULL);
	status = rdg_establish_data_connection(
	             rdg, rdg->tlsOut, "RDG_OUT_DATA", NULL, timeout, rpcFallback);

	if (status)
	{
		/* Establish IN connection with the same peer/server as OUT connection,
		 * even when server hostname resolves to different IP addresses.
		 */
		BIO_get_socket(rdg->tlsOut->underlying, &outConnSocket);
		peerAddress = freerdp_tcp_get_peer_address(outConnSocket);
		status = rdg_establish_data_connection(
		             rdg, rdg->tlsIn, "RDG_IN_DATA", peerAddress, timeout, NULL);
		free(peerAddress);
	}

	if (!status)
	{
		rdg->context->rdp->transport->layer = TRANSPORT_LAYER_CLOSED;
		return FALSE;
	}

	status = rdg_tunnel_connect(rdg);

	if (!status)
		return FALSE;

	return TRUE;
}

static int rdg_write_data_packet(rdpRdg* rdg, BYTE* buf, int size)
{
	int status;
	wStream* sChunk;
	int packetSize = size + 10;
	char chunkSize[11];

	if (size < 1)
		return 0;

	sprintf_s(chunkSize, sizeof(chunkSize), "%X\r\n", packetSize);
	sChunk = Stream_New(NULL, strlen(chunkSize) + packetSize + 2);

	if (!sChunk)
		return -1;

	Stream_Write(sChunk, chunkSize, strlen(chunkSize));
	Stream_Write_UINT16(sChunk, PKT_TYPE_DATA);   /* Type */
	Stream_Write_UINT16(sChunk, 0);   /* Reserved */
	Stream_Write_UINT32(sChunk, packetSize);   /* Packet length */
	Stream_Write_UINT16(sChunk, size);   /* Data size */
	Stream_Write(sChunk, buf, size);   /* Data */
	Stream_Write(sChunk, "\r\n", 2);
	Stream_SealLength(sChunk);
	status = tls_write_all(rdg->tlsIn, Stream_Buffer(sChunk), Stream_Length(sChunk));
	Stream_Free(sChunk, TRUE);

	if (status < 0)
		return -1;

	return size;
}

static BOOL rdg_process_close_packet(rdpRdg* rdg)
{
	int status;
	wStream* sChunk;
	int packetSize = 12;
	char chunkSize[11];
	sprintf_s(chunkSize, sizeof(chunkSize), "%X\r\n", packetSize);
	sChunk = Stream_New(NULL, strlen(chunkSize) + packetSize + 2);

	if (!sChunk)
		return FALSE;

	Stream_Write(sChunk, chunkSize, strlen(chunkSize));
	Stream_Write_UINT16(sChunk, PKT_TYPE_CLOSE_CHANNEL_RESPONSE);   /* Type */
	Stream_Write_UINT16(sChunk, 0);   /* Reserved */
	Stream_Write_UINT32(sChunk, packetSize);   /* Packet length */
	Stream_Write_UINT32(sChunk, 0);   /* Status code */
	Stream_Write(sChunk, "\r\n", 2);
	Stream_SealLength(sChunk);
	status = tls_write_all(rdg->tlsIn, Stream_Buffer(sChunk), Stream_Length(sChunk));
	Stream_Free(sChunk, TRUE);
	return (status < 0 ? FALSE : TRUE);
}

static BOOL rdg_process_keep_alive_packet(rdpRdg* rdg)
{
	int status;
	wStream* sChunk;
	int packetSize = 8;
	char chunkSize[11];
	sprintf_s(chunkSize, sizeof(chunkSize), "%X\r\n", packetSize);
	sChunk = Stream_New(NULL, strlen(chunkSize) + packetSize + 2);

	if (!sChunk)
		return FALSE;

	Stream_Write(sChunk, chunkSize, strlen(chunkSize));
	Stream_Write_UINT16(sChunk, PKT_TYPE_KEEPALIVE);   /* Type */
	Stream_Write_UINT16(sChunk, 0);   /* Reserved */
	Stream_Write_UINT32(sChunk, packetSize);   /* Packet length */
	Stream_Write(sChunk, "\r\n", 2);
	Stream_SealLength(sChunk);
	status = tls_write_all(rdg->tlsIn, Stream_Buffer(sChunk), Stream_Length(sChunk));
	Stream_Free(sChunk, TRUE);
	return (status < 0 ? FALSE : TRUE);
}

static BOOL rdg_process_unknown_packet(rdpRdg* rdg, int type)
{
	WLog_WARN(TAG, "Unknown Control Packet received: %X", type);
	return TRUE;
}

static BOOL rdg_process_control_packet(rdpRdg* rdg, int type, size_t packetLength)
{
	wStream* s = NULL;
	size_t readCount = 0;
	int status;
	size_t payloadSize = packetLength - sizeof(RdgPacketHeader);

	if (payloadSize)
	{
		s = Stream_New(NULL, payloadSize);

		if (!s)
			return FALSE;

		while (readCount < payloadSize)
		{
			status = BIO_read(rdg->tlsOut->bio, Stream_Pointer(s), sizeof(RdgPacketHeader) - readCount);

			if (status <= 0)
			{
				if (!BIO_should_retry(rdg->tlsOut->bio))
				{
					Stream_Free(s, TRUE);
					return FALSE;
				}

				continue;
			}

			Stream_Seek(s, status);
			readCount += status;
		}
	}

	switch (type)
	{
		case PKT_TYPE_CLOSE_CHANNEL:
			EnterCriticalSection(&rdg->writeSection);
			status = rdg_process_close_packet(rdg);
			LeaveCriticalSection(&rdg->writeSection);
			break;

		case PKT_TYPE_KEEPALIVE:
			EnterCriticalSection(&rdg->writeSection);
			status = rdg_process_keep_alive_packet(rdg);
			LeaveCriticalSection(&rdg->writeSection);
			break;

		default:
			status = rdg_process_unknown_packet(rdg, type);
			break;
	}

	Stream_Free(s, TRUE);
	return status;
}

static int rdg_read_data_packet(rdpRdg* rdg, BYTE* buffer, int size)
{
	RdgPacketHeader header;
	size_t readCount = 0;
	int readSize;
	int status;

	if (!rdg->packetRemainingCount)
	{
		while (readCount < sizeof(RdgPacketHeader))
		{
			status = BIO_read(rdg->tlsOut->bio, (BYTE*)(&header) + readCount,
			                  sizeof(RdgPacketHeader) - readCount);

			if (status <= 0)
			{
				if (!BIO_should_retry(rdg->tlsOut->bio))
					return -1;

				if (!readCount)
					return 0;

				BIO_wait_read(rdg->tlsOut->bio, 50);
				continue;
			}

			readCount += status;
		}

		if (header.type != PKT_TYPE_DATA)
		{
			status = rdg_process_control_packet(rdg, header.type, header.packetLength);

			if (!status)
				return -1;

			return 0;
		}

		readCount = 0;

		while (readCount < 2)
		{
			status = BIO_read(rdg->tlsOut->bio, (BYTE*)(&rdg->packetRemainingCount) + readCount, 2 - readCount);

			if (status < 0)
			{
				if (!BIO_should_retry(rdg->tlsOut->bio))
					return -1;

				BIO_wait_read(rdg->tlsOut->bio, 50);
				continue;
			}

			readCount += status;
		}
	}

	readSize = (rdg->packetRemainingCount < size ? rdg->packetRemainingCount : size);
	status = BIO_read(rdg->tlsOut->bio, buffer, readSize);

	if (status <= 0)
	{
		if (!BIO_should_retry(rdg->tlsOut->bio))
		{
			return -1;
		}

		return 0;
	}

	rdg->packetRemainingCount -= status;
	return status;
}

static int rdg_bio_write(BIO* bio, const char* buf, int num)
{
	int status;
	rdpRdg* rdg = (rdpRdg*) BIO_get_data(bio);
	BIO_clear_flags(bio, BIO_FLAGS_WRITE);
	EnterCriticalSection(&rdg->writeSection);
	status = rdg_write_data_packet(rdg, (BYTE*) buf, num);
	LeaveCriticalSection(&rdg->writeSection);

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

static int rdg_bio_read(BIO* bio, char* buf, int size)
{
	int status;
	rdpRdg* rdg = (rdpRdg*) BIO_get_data(bio);
	status = rdg_read_data_packet(rdg, (BYTE*) buf, size);

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

static int rdg_bio_puts(BIO* bio, const char* str)
{
	return -2;
}

static int rdg_bio_gets(BIO* bio, char* str, int size)
{
	return -2;
}

static long rdg_bio_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	int status = -1;
	rdpRdg* rdg = (rdpRdg*) BIO_get_data(bio);
	rdpTls* tlsOut = rdg->tlsOut;
	rdpTls* tlsIn = rdg->tlsIn;

	if (cmd == BIO_CTRL_FLUSH)
	{
		(void)BIO_flush(tlsOut->bio);
		(void)BIO_flush(tlsIn->bio);
		status = 1;
	}
	else if (cmd == BIO_C_SET_NONBLOCK)
	{
		status = 1;
	}
	else if (cmd == BIO_C_READ_BLOCKED)
	{
		BIO* bio = tlsOut->bio;
		status = BIO_read_blocked(bio);
	}
	else if (cmd == BIO_C_WRITE_BLOCKED)
	{
		BIO* bio = tlsIn->bio;
		status = BIO_write_blocked(bio);
	}
	else if (cmd == BIO_C_WAIT_READ)
	{
		int timeout = (int) arg1;
		BIO* bio = tlsOut->bio;

		if (BIO_read_blocked(bio))
			return BIO_wait_read(bio, timeout);
		else if (BIO_write_blocked(bio))
			return BIO_wait_write(bio, timeout);
		else
			status = 1;
	}
	else if (cmd == BIO_C_WAIT_WRITE)
	{
		int timeout = (int) arg1;
		BIO* bio = tlsIn->bio;

		if (BIO_write_blocked(bio))
			status = BIO_wait_write(bio, timeout);
		else if (BIO_read_blocked(bio))
			status = BIO_wait_read(bio, timeout);
		else
			status = 1;
	}
	else if (cmd == BIO_C_GET_EVENT || cmd == BIO_C_GET_FD)
	{
		/*
		 * A note about BIO_C_GET_FD:
		 * Even if two FDs are part of RDG, only one FD can be returned here.
		 *
		 * In FreeRDP, BIO FDs are only used for polling, so it is safe to use the outgoing FD only
		 *
		 * See issue #3602
		 */
		status = BIO_ctrl(tlsOut->bio, cmd, arg1, arg2);
	}

	return status;
}

static int rdg_bio_new(BIO* bio)
{
	BIO_set_init(bio, 1);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	return 1;
}

static int rdg_bio_free(BIO* bio)
{
	return 1;
}

static BIO_METHOD* BIO_s_rdg(void)
{
	static BIO_METHOD* bio_methods = NULL;

	if (bio_methods == NULL)
	{
		if (!(bio_methods = BIO_meth_new(BIO_TYPE_TSG, "RDGateway")))
			return NULL;

		BIO_meth_set_write(bio_methods, rdg_bio_write);
		BIO_meth_set_read(bio_methods, rdg_bio_read);
		BIO_meth_set_puts(bio_methods, rdg_bio_puts);
		BIO_meth_set_gets(bio_methods, rdg_bio_gets);
		BIO_meth_set_ctrl(bio_methods, rdg_bio_ctrl);
		BIO_meth_set_create(bio_methods, rdg_bio_new);
		BIO_meth_set_destroy(bio_methods, rdg_bio_free);
	}

	return bio_methods;
}

rdpRdg* rdg_new(rdpContext* context)
{
	rdpRdg* rdg;
	RPC_CSTR stringUuid;
	char bracedUuid[40];
	RPC_STATUS rpcStatus;

	if (!context)
		return NULL;

	rdg = (rdpRdg*) calloc(1, sizeof(rdpRdg));

	if (!rdg)
		return NULL;

	rdg->state = RDG_CLIENT_STATE_INITIAL;
	rdg->context = context;
	rdg->settings = rdg->context->settings;
	rdg->extAuth = HTTP_EXTENDED_AUTH_NONE;

	if (rdg->settings->GatewayAccessToken)
		rdg->extAuth = HTTP_EXTENDED_AUTH_PAA;

	UuidCreate(&rdg->guid);
	rpcStatus = UuidToStringA(&rdg->guid, &stringUuid);

	if (rpcStatus == RPC_S_OUT_OF_MEMORY)
		goto rdg_alloc_error;

	sprintf_s(bracedUuid, sizeof(bracedUuid), "{%s}", stringUuid);
	RpcStringFreeA(&stringUuid);
	rdg->tlsOut = tls_new(rdg->settings);

	if (!rdg->tlsOut)
		goto rdg_alloc_error;

	rdg->tlsIn = tls_new(rdg->settings);

	if (!rdg->tlsIn)
		goto rdg_alloc_error;

	rdg->http = http_context_new();

	if (!rdg->http)
		goto rdg_alloc_error;

	if (!http_context_set_uri(rdg->http, "/remoteDesktopGateway/") ||
	    !http_context_set_accept(rdg->http, "*/*") ||
	    !http_context_set_cache_control(rdg->http, "no-cache") ||
	    !http_context_set_pragma(rdg->http, "no-cache") ||
	    !http_context_set_connection(rdg->http, "Keep-Alive") ||
	    !http_context_set_user_agent(rdg->http, "MS-RDGateway/1.0") ||
	    !http_context_set_host(rdg->http, rdg->settings->GatewayHostname) ||
	    !http_context_set_rdg_connection_id(rdg->http, bracedUuid))
		goto rdg_alloc_error;

	if (rdg->extAuth != HTTP_EXTENDED_AUTH_NONE)
	{
		switch (rdg->extAuth)
		{
			case HTTP_EXTENDED_AUTH_PAA:
				if (!http_context_set_rdg_auth_scheme(rdg->http, "PAA"))
					goto rdg_alloc_error;

				break;

			default:
				WLog_DBG(TAG, "RDG extended authentication method %d not supported", rdg->extAuth);
		}
	}

	rdg->frontBio = BIO_new(BIO_s_rdg());

	if (!rdg->frontBio)
		goto rdg_alloc_error;

	BIO_set_data(rdg->frontBio, rdg);
	InitializeCriticalSection(&rdg->writeSection);
	return rdg;
rdg_alloc_error:
	rdg_free(rdg);
	return NULL;
}

void rdg_free(rdpRdg* rdg)
{
	if (!rdg)
		return;

	tls_free(rdg->tlsOut);
	tls_free(rdg->tlsIn);
	http_context_free(rdg->http);
	ntlm_free(rdg->ntlm);
	DeleteCriticalSection(&rdg->writeSection);
	free(rdg);
}

BIO* rdg_get_bio(rdpRdg* rdg)
{
	if (!rdg)
		return NULL;

	return rdg->frontBio;
}
