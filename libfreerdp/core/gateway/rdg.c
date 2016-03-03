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

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/utils/ringbuffer.h>

#include "rdg.h"
#include "../rdp.h"

#define TAG FREERDP_TAG("core.gateway.rdg")

#pragma pack(push, 1)

typedef struct rdg_packet_header
{
	UINT16 type;
	UINT16 reserved;
	UINT32 packetLength;
} RdgPacketHeader;

#pragma pack(pop)

BOOL rdg_write_packet(rdpRdg* rdg, wStream* sPacket)
{
	int status;
	wStream* sChunk;
	char chunkSize[11];

	sprintf_s(chunkSize, sizeof(chunkSize), "%X\r\n", (unsigned int) Stream_Length(sPacket));
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

wStream* rdg_receive_packet(rdpRdg* rdg)
{
	int status;
	wStream* s;
	RdgPacketHeader* packet;
	UINT32 readCount = 0;

	s = Stream_New(NULL, 1024);

	if (!s)
		return NULL;

	packet = (RdgPacketHeader*) Stream_Buffer(s);

	while (readCount < sizeof(RdgPacketHeader))
	{
		status = BIO_read(rdg->tlsOut->bio, Stream_Pointer(s), sizeof(RdgPacketHeader) - readCount);

		if (status < 0)
		{
			continue;
		}

		readCount += status;
		Stream_Seek(s, readCount);
	}

	if (Stream_Capacity(s) < packet->packetLength)
	{
		if (!Stream_EnsureCapacity(s, packet->packetLength))
		{
			Stream_Free(s, TRUE);
			return NULL;
		}
		packet = (RdgPacketHeader*) Stream_Buffer(s);
	}

	while (readCount < packet->packetLength)
	{
		status = BIO_read(rdg->tlsOut->bio, Stream_Pointer(s), packet->packetLength - readCount);

		if (status < 0)
		{
			continue;
		}

		readCount += status;
		Stream_Seek(s, readCount);
	}

	Stream_SealLength(s);

	return s;
}

BOOL rdg_send_handshake(rdpRdg* rdg)
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
	Stream_Write_UINT16(s, 0); /* ExtendedAuthentication (2 bytes) */

	Stream_SealLength(s);

	status = rdg_write_packet(rdg, s);
	Stream_Free(s, TRUE);

	if (status)
	{
		rdg->state = RDG_CLIENT_STATE_HANDSHAKE;
	}

	return status;
}

BOOL rdg_send_tunnel_request(rdpRdg* rdg)
{
	wStream* s;
	BOOL status;

	s = Stream_New(NULL, 16);

	if (!s)
		return FALSE;

	Stream_Write_UINT16(s, PKT_TYPE_TUNNEL_CREATE); /* Type (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Reserved (2 bytes) */
	Stream_Write_UINT32(s, 16); /* PacketLength (4 bytes) */

	Stream_Write_UINT32(s, HTTP_CAPABILITY_TYPE_QUAR_SOH); /* CapabilityFlags (4 bytes) */
	Stream_Write_UINT16(s, 0); /* FieldsPresent (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Reserved (2 bytes), must be 0 */

	Stream_SealLength(s);

	status = rdg_write_packet(rdg, s);
	Stream_Free(s, TRUE);

	if (status)
	{
		rdg->state = RDG_CLIENT_STATE_TUNNEL_CREATE;
	}

	return status;
}

BOOL rdg_send_tunnel_authorization(rdpRdg* rdg)
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

BOOL rdg_send_channel_create(rdpRdg* rdg)
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

wStream* rdg_build_http_request(rdpRdg* rdg, char* method)
{
	wStream* s;
	HttpRequest* request = NULL;
	SecBuffer* ntlmToken = NULL;
	char* base64NtlmToken = NULL;

	assert(method != NULL);

	request = http_request_new();

	if (!request)
		return NULL;

	http_request_set_method(request, method);
	http_request_set_uri(request, rdg->http->URI);

	if (!request->Method || !request->URI)
		return NULL;

	if (rdg->ntlm)
	{
		ntlmToken = rdg->ntlm->outputBuffer;

		if (ntlmToken)
			base64NtlmToken = crypto_base64_encode(ntlmToken->pvBuffer, ntlmToken->cbBuffer);

		if (base64NtlmToken)
		{
			http_request_set_auth_scheme(request, "NTLM");
			http_request_set_auth_param(request, base64NtlmToken);

			free(base64NtlmToken);

			if (!request->AuthScheme || !request->AuthParam)
				return NULL;
		}
	}

	if (rdg->state == RDG_CLIENT_STATE_IN_CHANNEL_AUTHORIZED)
	{
		http_request_set_transfer_encoding(request, "chunked");
	}

	s = http_request_write(rdg->http, request);
	http_request_free(request);

	if (s)
		Stream_SealLength(s);

	return s;
}

BOOL rdg_process_out_channel_response(rdpRdg* rdg, HttpResponse* response)
{
	int status;
	wStream* s;
	char* token64 = NULL;
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	rdpNtlm* ntlm = rdg->ntlm;

	if (response->StatusCode != HTTP_STATUS_DENIED)
	{
		WLog_DBG(TAG, "RDG not supported");
		rdg->state = RDG_CLIENT_STATE_NOT_FOUND;
		return FALSE;
	}

	WLog_DBG(TAG, "Out Channel authorization required");

	if (ListDictionary_Contains(response->Authenticates, "NTLM"))
	{
		token64 = ListDictionary_GetItemValue(response->Authenticates, "NTLM");

		if (!token64)
		{
			return FALSE;
		}

		crypto_base64_decode(token64, strlen(token64), &ntlmTokenData, &ntlmTokenLength);
	}

	if (ntlmTokenData && ntlmTokenLength)
	{
		ntlm->inputBuffer[0].pvBuffer = ntlmTokenData;
		ntlm->inputBuffer[0].cbBuffer = ntlmTokenLength;
	}

	ntlm_authenticate(ntlm);

	s = rdg_build_http_request(rdg, "RDG_OUT_DATA");

	if (!s)
		return FALSE;

	status = tls_write_all(rdg->tlsOut, Stream_Buffer(s), Stream_Length(s));

	Stream_Free(s, TRUE);

	ntlm_free(rdg->ntlm);
	rdg->ntlm = NULL;

	if (status < 0)
		return FALSE;

	rdg->state = RDG_CLIENT_STATE_OUT_CHANNEL_AUTHORIZE;

	return TRUE;
}

BOOL rdg_process_out_channel_authorization(rdpRdg* rdg, HttpResponse* response)
{
	if (response->StatusCode != HTTP_STATUS_OK)
	{
		rdg->state = RDG_CLIENT_STATE_CLOSED;
		return FALSE;
	}

	WLog_DBG(TAG, "Out Channel authorization complete");
	rdg->state = RDG_CLIENT_STATE_OUT_CHANNEL_AUTHORIZED;

	return TRUE;
}

BOOL rdg_process_in_channel_response(rdpRdg* rdg, HttpResponse* response)
{
	int status;
	wStream* s;
	char* token64 = NULL;
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	rdpNtlm* ntlm = rdg->ntlm;

	WLog_DBG(TAG, "In Channel authorization required");

	if (ListDictionary_Contains(response->Authenticates, "NTLM"))
	{
		token64 = ListDictionary_GetItemValue(response->Authenticates, "NTLM");

		if (!token64)
		{
			return FALSE;
		}

		crypto_base64_decode(token64, strlen(token64), &ntlmTokenData, &ntlmTokenLength);
	}

	if (ntlmTokenData && ntlmTokenLength)
	{
		ntlm->inputBuffer[0].pvBuffer = ntlmTokenData;
		ntlm->inputBuffer[0].cbBuffer = ntlmTokenLength;
	}

	ntlm_authenticate(ntlm);

	s = rdg_build_http_request(rdg, "RDG_IN_DATA");

	if (!s)
		return FALSE;

	status = tls_write_all(rdg->tlsIn, Stream_Buffer(s), Stream_Length(s));

	Stream_Free(s, TRUE);

	ntlm_free(rdg->ntlm);
	rdg->ntlm = NULL;

	if (status < 0)
		return FALSE;

	rdg->state = RDG_CLIENT_STATE_IN_CHANNEL_AUTHORIZE;

	return TRUE;
}

BOOL rdg_process_in_channel_authorization(rdpRdg* rdg, HttpResponse* response)
{
	wStream* s;
	int status;

	if (response->StatusCode != HTTP_STATUS_OK)
	{
		rdg->state = RDG_CLIENT_STATE_CLOSED;
		return FALSE;
	}

	WLog_DBG(TAG, "In Channel authorization complete");
	rdg->state = RDG_CLIENT_STATE_IN_CHANNEL_AUTHORIZED;

	s = rdg_build_http_request(rdg, "RDG_IN_DATA");

	if (!s)
		return FALSE;

	status = tls_write_all(rdg->tlsIn, Stream_Buffer(s), Stream_Length(s));
	
	Stream_Free(s, TRUE);

	if (status <= 0)
		return FALSE;

	return TRUE;
}

BOOL rdg_process_handshake_response(rdpRdg* rdg, wStream* s)
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

BOOL rdg_process_tunnel_response(rdpRdg* rdg, wStream* s)
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

BOOL rdg_process_tunnel_authorization_response(rdpRdg* rdg, wStream* s)
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

BOOL rdg_process_channel_response(rdpRdg* rdg, wStream* s)
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

BOOL rdg_process_packet(rdpRdg* rdg, wStream* s)
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


BOOL rdg_out_channel_recv(rdpRdg* rdg)
{
	wStream* s;
	BOOL status = TRUE;
	HttpResponse* response = NULL;

	switch (rdg->state)
	{
		case RDG_CLIENT_STATE_OUT_CHANNEL_REQUEST:
			response = http_response_recv(rdg->tlsOut);
			if (!response)
			{
				return FALSE;
			}
			status = rdg_process_out_channel_response(rdg, response);
			http_response_free(response);
			break;

		case RDG_CLIENT_STATE_OUT_CHANNEL_AUTHORIZE:
			response = http_response_recv(rdg->tlsOut);
			if (!response)
			{
				return FALSE;
			}
			status = rdg_process_out_channel_authorization(rdg, response);
			http_response_free(response);
			break;

		default:
			s = rdg_receive_packet(rdg);
			if (s)
			{
				status = rdg_process_packet(rdg, s);
				Stream_Free(s, TRUE);
			}
	}

	return status;
}

BOOL rdg_in_channel_recv(rdpRdg* rdg)
{
	BOOL status = TRUE;
	HttpResponse* response = NULL;

	switch (rdg->state)
	{
		case RDG_CLIENT_STATE_IN_CHANNEL_REQUEST:
			response = http_response_recv(rdg->tlsIn);

			if (!response)
				return FALSE;

			status = rdg_process_in_channel_response(rdg, response);
			http_response_free(response);
			break;

		case RDG_CLIENT_STATE_IN_CHANNEL_AUTHORIZE:
			response = http_response_recv(rdg->tlsIn);

			if (!response)
				return FALSE;

			status = rdg_process_in_channel_authorization(rdg, response);
			http_response_free(response);
			break;
	}

	return status;
}

DWORD rdg_get_event_handles(rdpRdg* rdg, HANDLE* events, DWORD count)
{
	DWORD nCount = 0;

	assert(rdg != NULL);

	if (events && (nCount < count))
		events[nCount++] = rdg->readEvent;
	else
		return 0;

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

BOOL rdg_check_event_handles(rdpRdg* rdg)
{
	HANDLE event = NULL;

	assert(rdg != NULL);

	BIO_get_event(rdg->tlsOut->bio, &event);

	if (WaitForSingleObject(event, 0) == WAIT_OBJECT_0)
	{
		return rdg_out_channel_recv(rdg);
	}

	BIO_get_event(rdg->tlsIn->bio, &event);

	if (WaitForSingleObject(event, 0) == WAIT_OBJECT_0)
	{
		return rdg_in_channel_recv(rdg);
	}

	return TRUE;
}

BOOL rdg_ncacn_http_ntlm_init(rdpRdg* rdg, rdpTls* tls)
{
	rdpNtlm* ntlm = rdg->ntlm;
	rdpContext* context = rdg->context;
	rdpSettings* settings = context->settings;
	freerdp* instance = context->instance;

	if (!settings->GatewayPassword || !settings->GatewayUsername || !strlen(settings->GatewayPassword) || !strlen(settings->GatewayUsername))
	{
		if (instance->GatewayAuthenticate)
		{
			BOOL proceed = instance->GatewayAuthenticate(instance, &settings->GatewayUsername, &settings->GatewayPassword, &settings->GatewayDomain);

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

	if (!ntlm_client_init(ntlm, TRUE, settings->GatewayUsername, settings->GatewayDomain, settings->GatewayPassword, tls->Bindings))
	{
		return FALSE;
	}

	if (!ntlm_client_make_spn(ntlm, _T("HTTP"), settings->GatewayHostname))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL rdg_send_out_channel_request(rdpRdg*rdg)
{
	wStream* s = NULL;
	int status;

	rdg->ntlm = ntlm_new();

	if (!rdg->ntlm)
		return FALSE;

	status = rdg_ncacn_http_ntlm_init(rdg, rdg->tlsOut);

	if (!status)
		return FALSE;

	status = ntlm_authenticate(rdg->ntlm);

	if (!status)
		return FALSE;

	s = rdg_build_http_request(rdg, "RDG_OUT_DATA");

	if (!s)
		return FALSE;

	status = tls_write_all(rdg->tlsOut, Stream_Buffer(s), Stream_Length(s));

	Stream_Free(s, TRUE);

	if (status < 0)
		return FALSE;

	rdg->state = RDG_CLIENT_STATE_OUT_CHANNEL_REQUEST;

	return TRUE;
}

BOOL rdg_send_in_channel_request(rdpRdg*rdg)
{
	int status;
	wStream* s = NULL;

	rdg->ntlm = ntlm_new();

	if (!rdg->ntlm)
		return FALSE;

	status = rdg_ncacn_http_ntlm_init(rdg, rdg->tlsIn);

	if (!status)
		return FALSE;

	status = ntlm_authenticate(rdg->ntlm);

	if (!status)
		return FALSE;

	s = rdg_build_http_request(rdg, "RDG_IN_DATA");

	if (!s)
		return FALSE;

	status = tls_write_all(rdg->tlsIn, Stream_Buffer(s), Stream_Length(s));

	Stream_Free(s, TRUE);

	if (status < 0)
		return FALSE;

	rdg->state = RDG_CLIENT_STATE_IN_CHANNEL_REQUEST;

	return TRUE;
}

BOOL rdg_tls_out_connect(rdpRdg* rdg, const char* hostname, UINT16 port, int timeout)
{
	int sockfd = 0;
	int status = 0;
	BIO* socketBio = NULL;
	BIO* bufferedBio = NULL;
	rdpSettings* settings = rdg->settings;

	assert(hostname != NULL);

	sockfd = freerdp_tcp_connect(rdg->context, settings, settings->GatewayHostname,
					settings->GatewayPort, timeout);

	if (sockfd < 1)
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
		BIO_free(socketBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, socketBio);
	status = BIO_set_nonblock(bufferedBio, TRUE);

	if (!status)
	{
		BIO_free_all(bufferedBio);
		return FALSE;
	}

	rdg->tlsOut->hostname = settings->GatewayHostname;
	rdg->tlsOut->port = settings->GatewayPort;
	rdg->tlsOut->isGatewayTransport = TRUE;
	status = tls_connect(rdg->tlsOut, bufferedBio);

	if (status < 1)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL rdg_tls_in_connect(rdpRdg* rdg, const char* hostname, UINT16 port, int timeout)
{
	int sockfd = 0;
	int status = 0;
	BIO* socketBio = NULL;
	BIO* bufferedBio = NULL;
	rdpSettings* settings = rdg->settings;

	assert(hostname != NULL);

	sockfd = freerdp_tcp_connect(rdg->context, settings, settings->GatewayHostname,
					settings->GatewayPort, timeout);

	if (sockfd < 1)
		return FALSE;

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
		BIO_free(socketBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, socketBio);
	status = BIO_set_nonblock(bufferedBio, TRUE);

	if (!status)
	{
		BIO_free_all(bufferedBio);
		return FALSE;
	}

	rdg->tlsIn->hostname = settings->GatewayHostname;
	rdg->tlsIn->port = settings->GatewayPort;
	rdg->tlsIn->isGatewayTransport = TRUE;
	status = tls_connect(rdg->tlsIn, bufferedBio);

	if (status < 1)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL rdg_out_channel_connect(rdpRdg* rdg, const char* hostname, UINT16 port, int timeout)
{
	BOOL status;
	DWORD nCount;
	HANDLE events[8];

	assert(hostname != NULL);

	status = rdg_tls_out_connect(rdg, hostname, port, timeout);

	if (!status)
		return FALSE;

	status = rdg_send_out_channel_request(rdg);

	if (!status)
		return FALSE;

	nCount = rdg_get_event_handles(rdg, events, 8);

	if (nCount == 0)
		return FALSE;

	while (rdg->state <= RDG_CLIENT_STATE_OUT_CHANNEL_AUTHORIZE)
	{
		WaitForMultipleObjects(nCount, events, FALSE, 100);
		status = rdg_check_event_handles(rdg);

		if (!status)
		{
			rdg->context->rdp->transport->layer = TRANSPORT_LAYER_CLOSED;
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rdg_in_channel_connect(rdpRdg* rdg, const char* hostname, UINT16 port, int timeout)
{
	BOOL status;
	DWORD nCount;
	HANDLE events[8];

	assert(hostname != NULL);

	status = rdg_tls_in_connect(rdg, hostname, port, timeout);

	if (!status)
		return FALSE;

	status = rdg_send_in_channel_request(rdg);

	if (!status)
		return FALSE;

	nCount = rdg_get_event_handles(rdg, events, 8);

	if (nCount == 0)
		return FALSE;

	while (rdg->state <= RDG_CLIENT_STATE_IN_CHANNEL_AUTHORIZE)
	{
		WaitForMultipleObjects(nCount, events, FALSE, 100);
		status = rdg_check_event_handles(rdg);

		if (!status)
		{
			rdg->context->rdp->transport->layer = TRANSPORT_LAYER_CLOSED;
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rdg_tunnel_connect(rdpRdg* rdg)
{
	BOOL status;
	DWORD nCount;
	HANDLE events[8];

	rdg_send_handshake(rdg);

	nCount = rdg_get_event_handles(rdg, events, 8);

	if (nCount == 0)
		return FALSE;

	while (rdg->state < RDG_CLIENT_STATE_OPENED)
	{
		WaitForMultipleObjects(nCount, events, FALSE, 100);
		status = rdg_check_event_handles(rdg);

		if (!status)
		{
			rdg->context->rdp->transport->layer = TRANSPORT_LAYER_CLOSED;
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rdg_connect(rdpRdg* rdg, const char* hostname, UINT16 port, int timeout)
{
	BOOL status;

	assert(rdg != NULL);

	status = rdg_out_channel_connect(rdg, hostname, port, timeout);

	if (!status)
		return FALSE;

	status = rdg_in_channel_connect(rdg, hostname, port, timeout);

	if (!status)
		return FALSE;

	status = rdg_tunnel_connect(rdg);

	if (!status)
		return FALSE;

	return TRUE;
}

int rdg_write_data_packet(rdpRdg* rdg, BYTE* buf, int size)
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

BOOL rdg_process_close_packet(rdpRdg* rdg)
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

BOOL rdg_process_keep_alive_packet(rdpRdg* rdg)
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

BOOL rdg_process_unknown_packet(rdpRdg* rdg, int type)
{
	WLog_WARN(TAG, "Unknown Control Packet received: %X", type);

	return TRUE;
}

BOOL rdg_process_control_packet(rdpRdg* rdg, int type, int packetLength)
{
	wStream* s = NULL;
	int readCount = 0;
	int status;
	int payloadSize = packetLength - sizeof(RdgPacketHeader);

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

int rdg_read_data_packet(rdpRdg* rdg, BYTE* buffer, int size)
{
	RdgPacketHeader header;
	int readCount = 0;
	int readSize;
	int status;
	int pending;

	if (!rdg->packetRemainingCount)
	{
		while (readCount < sizeof(RdgPacketHeader))
		{
			status = BIO_read(rdg->tlsOut->bio, (BYTE*)(&header) + readCount, sizeof(RdgPacketHeader) - readCount);

			if (status <= 0)
			{
				if (!BIO_should_retry(rdg->tlsOut->bio))
				{
					return -1;
				}
				if (!readCount)
				{
					return 0;
				}
				BIO_wait_read(rdg->tlsOut->bio, 50);
				continue;
			}

			readCount += status;
		}

		if (header.type != PKT_TYPE_DATA)
		{
			status = rdg_process_control_packet(rdg, header.type, header.packetLength);
			if (!status)
			{
				return -1;
			}
			return 0;
		}

		readCount = 0;

		while (readCount < 2)
		{
			status = BIO_read(rdg->tlsOut->bio, (BYTE*)(&rdg->packetRemainingCount) + readCount, 2 - readCount);

			if (status < 0)
			{
				if (!BIO_should_retry(rdg->tlsOut->bio))
				{
					return -1;
				}
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

	pending = BIO_pending(rdg->tlsOut->bio);

	if (pending > 0)
		SetEvent(rdg->readEvent);
	else
		ResetEvent(rdg->readEvent);

	return status;
}

long rdg_bio_callback(BIO* bio, int mode, const char* argp, int argi, long argl, long ret)
{
	return 1;
}

static int rdg_bio_write(BIO* bio, const char* buf, int num)
{
	int status;
	rdpRdg* rdg = (rdpRdg*) bio->ptr;

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
	rdpRdg* rdg = (rdpRdg*) bio->ptr;

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
	int status = 0;
	rdpRdg* rdg = (rdpRdg*)bio->ptr;
	rdpTls* tlsOut = rdg->tlsOut;
	rdpTls* tlsIn = rdg->tlsIn;

	if (cmd == BIO_CTRL_FLUSH)
	{
		(void)BIO_flush(tlsOut->bio);
		(void)BIO_flush(tlsIn->bio);
		status = 1;
	}
	else if (cmd == BIO_C_GET_EVENT)
	{
		if (arg2)
		{
			BIO_get_event(rdg->tlsOut->bio, arg2);
			status = 1;
		}
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

	return status;
}

static int rdg_bio_new(BIO* bio)
{
	bio->init = 1;
	bio->num = 0;
	bio->ptr = NULL;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;
	return 1;
}

static int rdg_bio_free(BIO* bio)
{
	return 1;
}

static BIO_METHOD rdg_bio_methods =
{
	BIO_TYPE_TSG,
	"RDGateway",
	rdg_bio_write,
	rdg_bio_read,
	rdg_bio_puts,
	rdg_bio_gets,
	rdg_bio_ctrl,
	rdg_bio_new,
	rdg_bio_free,
	NULL,
};

BIO_METHOD* BIO_s_rdg(void)
{
	return &rdg_bio_methods;
}

rdpRdg* rdg_new(rdpTransport* transport)
{
	rdpRdg* rdg;
	RPC_CSTR stringUuid;
	char bracedUuid[40];
	RPC_STATUS rpcStatus;

	assert(transport != NULL);

	rdg = (rdpRdg*) calloc(1, sizeof(rdpRdg));

	if (rdg)
	{
		rdg->state = RDG_CLIENT_STATE_INITIAL;
		rdg->context = transport->context;
		rdg->settings = rdg->context->settings;

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

		http_context_set_uri(rdg->http, "/remoteDesktopGateway/");
		http_context_set_accept(rdg->http, "*/*");
		http_context_set_cache_control(rdg->http, "no-cache");
		http_context_set_pragma(rdg->http, "no-cache");
		http_context_set_connection(rdg->http, "Keep-Alive");
		http_context_set_user_agent(rdg->http, "MS-RDGateway/1.0");
		http_context_set_host(rdg->http, rdg->settings->GatewayHostname);
		http_context_set_rdg_connection_id(rdg->http, bracedUuid);

		if (!rdg->http->URI || !rdg->http->Accept || !rdg->http->CacheControl ||
				!rdg->http->Pragma || !rdg->http->Connection || !rdg->http->UserAgent
				|| !rdg->http->Host || !rdg->http->RdgConnectionId)
		{
			goto rdg_alloc_error;
		}

		rdg->frontBio = BIO_new(BIO_s_rdg());

		if (!rdg->frontBio)
			goto rdg_alloc_error;

		rdg->frontBio->ptr = rdg;

		rdg->readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		if (!rdg->readEvent)
			goto rdg_alloc_error;
        
		InitializeCriticalSection(&rdg->writeSection);
	}

	return rdg;

rdg_alloc_error:
	rdg_free(rdg);
	return NULL;
}

void rdg_free(rdpRdg* rdg)
{
	if (!rdg)
		return;

	if (rdg->tlsOut)
	{
		tls_free(rdg->tlsOut);
		rdg->tlsOut = NULL;
	}

	if (rdg->tlsIn)
	{
		tls_free(rdg->tlsIn);
		rdg->tlsIn = NULL;
	}

	if (rdg->http)
	{
		http_context_free(rdg->http);
		rdg->http = NULL;
	}

	if (rdg->ntlm)
	{
		ntlm_free(rdg->ntlm);
		rdg->ntlm = NULL;
	}

	if (rdg->readEvent)
	{
		CloseHandle(rdg->readEvent);
		rdg->readEvent = NULL;
	}
    
	DeleteCriticalSection(&rdg->writeSection);

	free(rdg);
}
