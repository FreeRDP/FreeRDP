/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Terminal Server Gateway (TSG)
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/error.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/log.h>

#include "rpc_bind.h"
#include "rpc_client.h"
#include "tsg.h"
#include "../../crypto/opensslcompat.h"

#define TAG FREERDP_TAG("core.gateway.tsg")

#define TSG_CAPABILITY_TYPE_NAP 0x00000001

#define TSG_PACKET_TYPE_HEADER 0x00004844
#define TSG_PACKET_TYPE_VERSIONCAPS 0x00005643
#define TSG_PACKET_TYPE_QUARCONFIGREQUEST 0x00005143
#define TSG_PACKET_TYPE_QUARREQUEST 0x00005152
#define TSG_PACKET_TYPE_RESPONSE 0x00005052
#define TSG_PACKET_TYPE_QUARENC_RESPONSE 0x00004552
#define TSG_PACKET_TYPE_CAPS_RESPONSE 0x00004350
#define TSG_PACKET_TYPE_MSGREQUEST_PACKET 0x00004752
#define TSG_PACKET_TYPE_MESSAGE_PACKET 0x00004750
#define TSG_PACKET_TYPE_AUTH 0x00004054
#define TSG_PACKET_TYPE_REAUTH 0x00005250

typedef WCHAR* RESOURCENAME;

typedef struct
{
	RESOURCENAME* resourceName;
	UINT32 numResourceNames;
	RESOURCENAME* alternateResourceNames;
	UINT16 numAlternateResourceNames;
	UINT32 Port;
} TSENDPOINTINFO, *PTSENDPOINTINFO;

typedef struct
{
	UINT16 ComponentId;
	UINT16 PacketId;
} TSG_PACKET_HEADER, *PTSG_PACKET_HEADER;

typedef struct
{
	UINT32 capabilities;
} TSG_CAPABILITY_NAP, *PTSG_CAPABILITY_NAP;

typedef union
{
	TSG_CAPABILITY_NAP tsgCapNap;
} TSG_CAPABILITIES_UNION, *PTSG_CAPABILITIES_UNION;

typedef struct
{
	UINT32 capabilityType;
	TSG_CAPABILITIES_UNION tsgPacket;
} TSG_PACKET_CAPABILITIES, *PTSG_PACKET_CAPABILITIES;

typedef struct
{
	TSG_PACKET_HEADER tsgHeader;
	PTSG_PACKET_CAPABILITIES tsgCaps;
	UINT32 numCapabilities;
	UINT16 majorVersion;
	UINT16 minorVersion;
	UINT16 quarantineCapabilities;
} TSG_PACKET_VERSIONCAPS, *PTSG_PACKET_VERSIONCAPS;

typedef struct
{
	UINT32 flags;
} TSG_PACKET_QUARCONFIGREQUEST, *PTSG_PACKET_QUARCONFIGREQUEST;

typedef struct
{
	UINT32 flags;
	WCHAR* machineName;
	UINT32 nameLength;
	BYTE* data;
	UINT32 dataLen;
} TSG_PACKET_QUARREQUEST, *PTSG_PACKET_QUARREQUEST;

typedef struct
{
	BOOL enableAllRedirections;
	BOOL disableAllRedirections;
	BOOL driveRedirectionDisabled;
	BOOL printerRedirectionDisabled;
	BOOL portRedirectionDisabled;
	BOOL reserved;
	BOOL clipboardRedirectionDisabled;
	BOOL pnpRedirectionDisabled;
} TSG_REDIRECTION_FLAGS, *PTSG_REDIRECTION_FLAGS;

typedef struct
{
	UINT32 flags;
	UINT32 reserved;
	BYTE* responseData;
	UINT32 responseDataLen;
	TSG_REDIRECTION_FLAGS redirectionFlags;
} TSG_PACKET_RESPONSE, *PTSG_PACKET_RESPONSE;

typedef struct
{
	UINT32 flags;
	UINT32 certChainLen;
	WCHAR* certChainData;
	GUID nonce;
	PTSG_PACKET_VERSIONCAPS versionCaps;
} TSG_PACKET_QUARENC_RESPONSE, *PTSG_PACKET_QUARENC_RESPONSE;

typedef struct
{
	INT32 isDisplayMandatory;
	INT32 isConsentMandatory;
	UINT32 msgBytes;
	WCHAR* msgBuffer;
} TSG_PACKET_STRING_MESSAGE;

typedef struct
{
	UINT64 tunnelContext;
} TSG_PACKET_REAUTH_MESSAGE, *PTSG_PACKET_REAUTH_MESSAGE;

typedef struct
{
	UINT32 msgID;
	UINT32 msgType;
	INT32 isMsgPresent;
} TSG_PACKET_MSG_RESPONSE, *PTSG_PACKET_MSG_RESPONSE;

typedef struct
{
	TSG_PACKET_QUARENC_RESPONSE pktQuarEncResponse;
	TSG_PACKET_MSG_RESPONSE pktConsentMessage;
} TSG_PACKET_CAPS_RESPONSE, *PTSG_PACKET_CAPS_RESPONSE;

typedef struct
{
	UINT32 maxMessagesPerBatch;
} TSG_PACKET_MSG_REQUEST, *PTSG_PACKET_MSG_REQUEST;

typedef struct
{
	TSG_PACKET_VERSIONCAPS tsgVersionCaps;
	UINT32 cookieLen;
	BYTE* cookie;
} TSG_PACKET_AUTH, *PTSG_PACKET_AUTH;

typedef union
{
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;
	PTSG_PACKET_AUTH packetAuth;
} TSG_INITIAL_PACKET_TYPE_UNION, *PTSG_INITIAL_PACKET_TYPE_UNION;

typedef struct
{
	UINT64 tunnelContext;
	UINT32 packetId;
	TSG_INITIAL_PACKET_TYPE_UNION tsgInitialPacket;
} TSG_PACKET_REAUTH, *PTSG_PACKET_REAUTH;

typedef union
{
	PTSG_PACKET_HEADER packetHeader;
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;
	PTSG_PACKET_QUARCONFIGREQUEST packetQuarConfigRequest;
	PTSG_PACKET_QUARREQUEST packetQuarRequest;
	PTSG_PACKET_RESPONSE packetResponse;
	PTSG_PACKET_QUARENC_RESPONSE packetQuarEncResponse;
	PTSG_PACKET_CAPS_RESPONSE packetCapsResponse;
	PTSG_PACKET_MSG_REQUEST packetMsgRequest;
	PTSG_PACKET_MSG_RESPONSE packetMsgResponse;
	PTSG_PACKET_AUTH packetAuth;
	PTSG_PACKET_REAUTH packetReauth;
} TSG_PACKET_TYPE_UNION;

typedef struct
{
	UINT32 packetId;
	TSG_PACKET_TYPE_UNION tsgPacket;
} TSG_PACKET, *PTSG_PACKET;

struct rdp_tsg
{
	BIO* bio;
	rdpRpc* rpc;
	UINT16 Port;
	LPWSTR Hostname;
	LPWSTR MachineName;
	TSG_STATE state;
	UINT32 TunnelId;
	UINT32 ChannelId;
	BOOL reauthSequence;
	rdpTransport* transport;
	UINT64 ReauthTunnelContext;
	CONTEXT_HANDLE TunnelContext;
	CONTEXT_HANDLE ChannelContext;
	CONTEXT_HANDLE NewTunnelContext;
	CONTEXT_HANDLE NewChannelContext;
	TSG_PACKET_REAUTH packetReauth;
	TSG_PACKET_CAPABILITIES tsgCaps;
	TSG_PACKET_VERSIONCAPS packetVersionCaps;
};

static const char* tsg_packet_id_to_string(UINT32 packetId)
{
	switch (packetId)
	{
		case TSG_PACKET_TYPE_HEADER:
			return "TSG_PACKET_TYPE_HEADER";
		case TSG_PACKET_TYPE_VERSIONCAPS:
			return "TSG_PACKET_TYPE_VERSIONCAPS";
		case TSG_PACKET_TYPE_QUARCONFIGREQUEST:
			return "TSG_PACKET_TYPE_QUARCONFIGREQUEST";
		case TSG_PACKET_TYPE_QUARREQUEST:
			return "TSG_PACKET_TYPE_QUARREQUEST";
		case TSG_PACKET_TYPE_RESPONSE:
			return "TSG_PACKET_TYPE_RESPONSE";
		case TSG_PACKET_TYPE_QUARENC_RESPONSE:
			return "TSG_PACKET_TYPE_QUARENC_RESPONSE";
		case TSG_CAPABILITY_TYPE_NAP:
			return "TSG_CAPABILITY_TYPE_NAP";
		case TSG_PACKET_TYPE_CAPS_RESPONSE:
			return "TSG_PACKET_TYPE_CAPS_RESPONSE";
		case TSG_PACKET_TYPE_MSGREQUEST_PACKET:
			return "TSG_PACKET_TYPE_MSGREQUEST_PACKET";
		case TSG_PACKET_TYPE_MESSAGE_PACKET:
			return "TSG_PACKET_TYPE_MESSAGE_PACKET";
		case TSG_PACKET_TYPE_AUTH:
			return "TSG_PACKET_TYPE_AUTH";
		case TSG_PACKET_TYPE_REAUTH:
			return "TSG_PACKET_TYPE_REAUTH";
		default:
			return "UNKNOWN";
	}
}

static const char* tsg_state_to_string(TSG_STATE state)
{
	switch (state)
	{
		case TSG_STATE_INITIAL:
			return "TSG_STATE_INITIAL";
		case TSG_STATE_CONNECTED:
			return "TSG_STATE_CONNECTED";
		case TSG_STATE_AUTHORIZED:
			return "TSG_STATE_AUTHORIZED";
		case TSG_STATE_CHANNEL_CREATED:
			return "TSG_STATE_CHANNEL_CREATED";
		case TSG_STATE_PIPE_CREATED:
			return "TSG_STATE_PIPE_CREATED";
		case TSG_STATE_TUNNEL_CLOSE_PENDING:
			return "TSG_STATE_TUNNEL_CLOSE_PENDING";
		case TSG_STATE_CHANNEL_CLOSE_PENDING:
			return "TSG_STATE_CHANNEL_CLOSE_PENDING";
		case TSG_STATE_FINAL:
			return "TSG_STATE_FINAL";
		default:
			return "TSG_STATE_UNKNOWN";
	}
}

static BOOL tsg_print(char** buffer, size_t* len, const char* fmt, ...)
{
	int rc;
	va_list ap;
	if (!buffer || !len || !fmt)
		return FALSE;
	va_start(ap, fmt);
	rc = vsnprintf(*buffer, *len, fmt, ap);
	va_end(ap);
	if ((rc < 0) || ((size_t)rc > *len))
		return FALSE;
	*len -= (size_t)rc;
	*buffer += (size_t)rc;
	return TRUE;
}

static BOOL tsg_packet_header_to_string(char** buffer, size_t* length,
                                        const TSG_PACKET_HEADER* header)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(header);

	return tsg_print(buffer, length,
	                 "header { ComponentId=0x%04" PRIx16 ", PacketId=0x%04" PRIx16 " }",
	                 header->ComponentId, header->PacketId);
}

static BOOL tsg_type_capability_nap_to_string(char** buffer, size_t* length,
                                              const TSG_CAPABILITY_NAP* cur)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(cur);

	return tsg_print(buffer, length, "%s { capabilities=0x%08" PRIx32 " }",
	                 tsg_packet_id_to_string(TSG_CAPABILITY_TYPE_NAP), cur->capabilities);
}

static BOOL tsg_packet_capabilities_to_string(char** buffer, size_t* length,
                                              const TSG_PACKET_CAPABILITIES* caps, UINT32 numCaps)
{
	UINT32 x;

	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "capabilities { "))
		return FALSE;

	for (x = 0; x < numCaps; x++)
	{
		const TSG_PACKET_CAPABILITIES* cur = &caps[x];
		switch (cur->capabilityType)
		{
			case TSG_CAPABILITY_TYPE_NAP:
				if (!tsg_type_capability_nap_to_string(buffer, length, &cur->tsgPacket.tsgCapNap))
					return FALSE;
				break;
			default:
				if (!tsg_print(buffer, length, "TSG_UNKNOWN_CAPABILITY"))
					return FALSE;
				break;
		}
	}
	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_versioncaps_to_string(char** buffer, size_t* length,
                                             const TSG_PACKET_VERSIONCAPS* caps)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "versioncaps { "))
		return FALSE;
	if (!tsg_packet_header_to_string(buffer, length, &caps->tsgHeader))
		return FALSE;

	if (!tsg_print(buffer, length, " "))
		return FALSE;

	if (!tsg_packet_capabilities_to_string(buffer, length, caps->tsgCaps, caps->numCapabilities))
		return FALSE;

	if (!tsg_print(buffer, length,
	               " numCapabilities=0x%08" PRIx32 ", majorVersion=0x%04" PRIx16
	               ", minorVersion=0x%04" PRIx16 ", quarantineCapabilities=0x%04" PRIx16,
	               caps->numCapabilities, caps->majorVersion, caps->minorVersion,
	               caps->quarantineCapabilities))
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_quarconfigrequest_to_string(char** buffer, size_t* length,
                                                   const TSG_PACKET_QUARCONFIGREQUEST* caps)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "quarconfigrequest { "))
		return FALSE;

	if (!tsg_print(buffer, length, " "))
		return FALSE;

	if (!tsg_print(buffer, length, " flags=0x%08" PRIx32, caps->flags))
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_quarrequest_to_string(char** buffer, size_t* length,
                                             const TSG_PACKET_QUARREQUEST* caps)
{
	BOOL rc = FALSE;
	char* name = NULL;
	char* strdata = NULL;

	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "quarrequest { "))
		return FALSE;

	if (!tsg_print(buffer, length, " "))
		return FALSE;

	if (caps->nameLength > 0)
	{
		if (caps->nameLength > INT_MAX)
			return FALSE;
		if (ConvertFromUnicode(CP_UTF8, 0, caps->machineName, (int)caps->nameLength, &name, 0, NULL,
		                       NULL) < 0)
			return FALSE;
	}

	strdata = winpr_BinToHexString(caps->data, caps->dataLen, TRUE);
	if (strdata || (caps->dataLen == 0))
		rc = tsg_print(buffer, length,
		               " flags=0x%08" PRIx32 ", machineName=%s [%" PRIu32 "], data[%" PRIu32 "]=%s",
		               caps->flags, name, caps->nameLength, caps->dataLen, strdata);
	free(name);
	free(strdata);
	if (!rc)
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static const char* tsg_bool_to_string(BOOL val)
{
	if (val)
		return "true";
	return "false";
}

static const char* tsg_redirection_flags_to_string(char* buffer, size_t size,
                                                   const TSG_REDIRECTION_FLAGS* flags)
{
	WINPR_ASSERT(buffer || (size == 0));
	WINPR_ASSERT(flags);

	_snprintf(buffer, size,
	          "enableAllRedirections=%s,  disableAllRedirections=%s, driveRedirectionDisabled=%s, "
	          "printerRedirectionDisabled=%s, portRedirectionDisabled=%s, reserved=%s, "
	          "clipboardRedirectionDisabled=%s, pnpRedirectionDisabled=%s",
	          tsg_bool_to_string(flags->enableAllRedirections),
	          tsg_bool_to_string(flags->disableAllRedirections),
	          tsg_bool_to_string(flags->driveRedirectionDisabled),
	          tsg_bool_to_string(flags->printerRedirectionDisabled),
	          tsg_bool_to_string(flags->portRedirectionDisabled),
	          tsg_bool_to_string(flags->reserved),
	          tsg_bool_to_string(flags->clipboardRedirectionDisabled),
	          tsg_bool_to_string(flags->pnpRedirectionDisabled));
	return buffer;
}

static BOOL tsg_packet_response_to_string(char** buffer, size_t* length,
                                          const TSG_PACKET_RESPONSE* caps)
{
	BOOL rc = FALSE;
	char* strdata = NULL;
	char tbuffer[8192] = { 0 };

	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "response { "))
		return FALSE;

	if (!tsg_print(buffer, length, " "))
		return FALSE;

	strdata = winpr_BinToHexString(caps->responseData, caps->responseDataLen, TRUE);
	if (strdata || (caps->responseDataLen == 0))
		rc = tsg_print(
		    buffer, length,
		    " flags=0x%08" PRIx32 ", reserved=0x%08" PRIx32 ", responseData[%" PRIu32
		    "]=%s, redirectionFlags={ %s }",
		    caps->flags, caps->reserved, caps->responseDataLen, strdata,
		    tsg_redirection_flags_to_string(tbuffer, ARRAYSIZE(tbuffer), &caps->redirectionFlags));
	free(strdata);
	if (!rc)
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_quarenc_response_to_string(char** buffer, size_t* length,
                                                  const TSG_PACKET_QUARENC_RESPONSE* caps)
{
	BOOL rc = FALSE;
	char* strdata = NULL;
	RPC_CSTR uuid;
	char tbuffer[8192] = { 0 };
	size_t size = ARRAYSIZE(tbuffer);
	char* ptbuffer = tbuffer;

	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "quarenc_response { "))
		return FALSE;

	if (!tsg_print(buffer, length, " "))
		return FALSE;

	if (caps->certChainLen > 0)
	{
		if (caps->certChainLen > INT_MAX)
			return FALSE;
		if (ConvertFromUnicode(CP_UTF8, 0, caps->certChainData, (int)caps->certChainLen, &strdata,
		                       0, NULL, NULL) <= 0)
			return FALSE;
	}

	tsg_packet_versioncaps_to_string(&ptbuffer, &size, caps->versionCaps);
	UuidToStringA(&caps->nonce, &uuid);
	if (strdata || (caps->certChainLen == 0))
		rc =
		    tsg_print(buffer, length,
		              " flags=0x%08" PRIx32 ", certChain[%" PRIu32 "]=%s, nonce=%s, versionCaps=%s",
		              caps->flags, caps->certChainLen, strdata, uuid, tbuffer);
	free(strdata);
	free(uuid);
	if (!rc)
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_message_response_to_string(char** buffer, size_t* length,
                                                  const TSG_PACKET_MSG_RESPONSE* caps)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "msg_response { "))
		return FALSE;

	if (!tsg_print(buffer, length,
	               " msgID=0x%08" PRIx32 ", msgType=0x%08" PRIx32 ", isMsgPresent=%" PRId32,
	               caps->msgID, caps->msgType, caps->isMsgPresent))
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_caps_response_to_string(char** buffer, size_t* length,
                                               const TSG_PACKET_CAPS_RESPONSE* caps)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "caps_response { "))
		return FALSE;

	if (!tsg_packet_quarenc_response_to_string(buffer, length, &caps->pktQuarEncResponse))
		return FALSE;

	if (!tsg_packet_message_response_to_string(buffer, length, &caps->pktConsentMessage))
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_message_request_to_string(char** buffer, size_t* length,
                                                 const TSG_PACKET_MSG_REQUEST* caps)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "caps_message_request { "))
		return FALSE;

	if (!tsg_print(buffer, length, " maxMessagesPerBatch=%" PRIu32, caps->maxMessagesPerBatch))
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_auth_to_string(char** buffer, size_t* length, const TSG_PACKET_AUTH* caps)
{
	BOOL rc = FALSE;
	char* strdata = NULL;
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "caps_message_request { "))
		return FALSE;

	if (!tsg_packet_versioncaps_to_string(buffer, length, &caps->tsgVersionCaps))
		return FALSE;

	strdata = winpr_BinToHexString(caps->cookie, caps->cookieLen, TRUE);
	if (strdata || (caps->cookieLen == 0))
		rc = tsg_print(buffer, length, " cookie[%" PRIu32 "]=%s", caps->cookieLen, strdata);
	free(strdata);
	if (!rc)
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static BOOL tsg_packet_reauth_to_string(char** buffer, size_t* length,
                                        const TSG_PACKET_REAUTH* caps)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "caps_message_request { "))
		return FALSE;

	if (!tsg_print(buffer, length, " tunnelContext=0x%016" PRIx64 ", packetId=%s [0x%08" PRIx32 "]",
	               caps->tunnelContext, tsg_packet_id_to_string(caps->packetId), caps->packetId))
		return FALSE;

	switch (caps->packetId)
	{
		case TSG_PACKET_TYPE_VERSIONCAPS:
			rc = tsg_packet_versioncaps_to_string(buffer, length,
			                                      caps->tsgInitialPacket.packetVersionCaps);
			break;
		case TSG_PACKET_TYPE_AUTH:
			rc = tsg_packet_auth_to_string(buffer, length, caps->tsgInitialPacket.packetAuth);
			break;
		default:
			rc = tsg_print(buffer, length, "TODO: Unhandled packet type %s [0x%08" PRIx32 "]",
			               tsg_packet_id_to_string(caps->packetId), caps->packetId);
			break;
	}

	if (!rc)
		return FALSE;

	return tsg_print(buffer, length, " }");
}

static const char* tsg_packet_to_string(const TSG_PACKET* packet)
{
	size_t len = 8192;
	static char sbuffer[8193] = { 0 };
	char* buffer = sbuffer;

	if (!tsg_print(&buffer, &len, "TSG_PACKET { packetId=%s [0x%08" PRIx32 "], ",
	               tsg_packet_id_to_string(packet->packetId), packet->packetId))
		goto fail;

	switch (packet->packetId)
	{
		case TSG_PACKET_TYPE_HEADER:
			if (!tsg_packet_header_to_string(&buffer, &len, packet->tsgPacket.packetHeader))
				goto fail;
			break;
		case TSG_PACKET_TYPE_VERSIONCAPS:
			if (!tsg_packet_versioncaps_to_string(&buffer, &len,
			                                      packet->tsgPacket.packetVersionCaps))
				goto fail;
			break;
		case TSG_PACKET_TYPE_QUARCONFIGREQUEST:
			if (!tsg_packet_quarconfigrequest_to_string(&buffer, &len,
			                                            packet->tsgPacket.packetQuarConfigRequest))
				goto fail;
			break;
		case TSG_PACKET_TYPE_QUARREQUEST:
			if (!tsg_packet_quarrequest_to_string(&buffer, &len,
			                                      packet->tsgPacket.packetQuarRequest))
				goto fail;
			break;
		case TSG_PACKET_TYPE_RESPONSE:
			if (!tsg_packet_response_to_string(&buffer, &len, packet->tsgPacket.packetResponse))
				goto fail;
			break;
		case TSG_PACKET_TYPE_QUARENC_RESPONSE:
			if (!tsg_packet_quarenc_response_to_string(&buffer, &len,
			                                           packet->tsgPacket.packetQuarEncResponse))
				goto fail;
			break;
		case TSG_PACKET_TYPE_CAPS_RESPONSE:
			if (!tsg_packet_caps_response_to_string(&buffer, &len,
			                                        packet->tsgPacket.packetCapsResponse))
				goto fail;
			break;
		case TSG_PACKET_TYPE_MSGREQUEST_PACKET:
			if (!tsg_packet_message_request_to_string(&buffer, &len,
			                                          packet->tsgPacket.packetMsgRequest))
				goto fail;
			break;
		case TSG_PACKET_TYPE_MESSAGE_PACKET:
			if (!tsg_packet_message_response_to_string(&buffer, &len,
			                                           packet->tsgPacket.packetMsgResponse))
				goto fail;
			break;
		case TSG_PACKET_TYPE_AUTH:
			if (!tsg_packet_auth_to_string(&buffer, &len, packet->tsgPacket.packetAuth))
				goto fail;
			break;
		case TSG_PACKET_TYPE_REAUTH:
			if (!tsg_packet_reauth_to_string(&buffer, &len, packet->tsgPacket.packetReauth))
				goto fail;
			break;
		default:
			if (!tsg_print(&buffer, &len, "INVALID"))
				goto fail;
			break;
	}

	if (!tsg_print(&buffer, &len, " }"))
		goto fail;

fail:
	return sbuffer;
}

static BOOL tsg_stream_align(wStream* s, size_t align)
{
	size_t pos;
	size_t offset = 0;

	if (!s)
		return FALSE;

	pos = Stream_GetPosition(s);

	if ((pos % align) != 0)
		offset = align - pos % align;

	return Stream_SafeSeek(s, offset);
}

static BIO_METHOD* BIO_s_tsg(void);
/**
 * RPC Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378623/
 * Remote Procedure Call: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378651/
 * RPC NDR Interface Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802752/
 */

/**
 * call sequence with silent reauth:
 *
 * TsProxyCreateTunnelRequest()
 * TsProxyCreateTunnelResponse(TunnelContext)
 * TsProxyAuthorizeTunnelRequest(TunnelContext)
 * TsProxyAuthorizeTunnelResponse()
 * TsProxyMakeTunnelCallRequest(TunnelContext)
 * TsProxyCreateChannelRequest(TunnelContext)
 * TsProxyCreateChannelResponse(ChannelContext)
 * TsProxySetupReceivePipeRequest(ChannelContext)
 * TsProxySendToServerRequest(ChannelContext)
 *
 * ...
 *
 * TsProxyMakeTunnelCallResponse(reauth)
 * TsProxyCreateTunnelRequest()
 * TsProxyMakeTunnelCallRequest(TunnelContext)
 * TsProxyCreateTunnelResponse(NewTunnelContext)
 * TsProxyAuthorizeTunnelRequest(NewTunnelContext)
 * TsProxyAuthorizeTunnelResponse()
 * TsProxyCreateChannelRequest(NewTunnelContext)
 * TsProxyCreateChannelResponse(NewChannelContext)
 * TsProxyCloseChannelRequest(NewChannelContext)
 * TsProxyCloseTunnelRequest(NewTunnelContext)
 * TsProxyCloseChannelResponse(NullChannelContext)
 * TsProxyCloseTunnelResponse(NullTunnelContext)
 * TsProxySendToServerRequest(ChannelContext)
 */

static int TsProxySendToServer(handle_t IDL_handle, const byte pRpcMessage[], UINT32 count,
                               UINT32* lengths)
{
	wStream* s;
	rdpTsg* tsg;
	size_t length;
	const byte* buffer1 = NULL;
	const byte* buffer2 = NULL;
	const byte* buffer3 = NULL;
	UINT32 buffer1Length;
	UINT32 buffer2Length;
	UINT32 buffer3Length;
	UINT32 numBuffers = 0;
	UINT32 totalDataBytes = 0;
	tsg = (rdpTsg*)IDL_handle;
	buffer1Length = buffer2Length = buffer3Length = 0;

	if (count > 0)
	{
		numBuffers++;
		buffer1 = &pRpcMessage[0];
		buffer1Length = lengths[0];
		totalDataBytes += lengths[0] + 4;
	}

	if (count > 1)
	{
		numBuffers++;
		buffer2 = &pRpcMessage[1];
		buffer2Length = lengths[1];
		totalDataBytes += lengths[1] + 4;
	}

	if (count > 2)
	{
		numBuffers++;
		buffer3 = &pRpcMessage[2];
		buffer3Length = lengths[2];
		totalDataBytes += lengths[2] + 4;
	}

	length = 28ull + totalDataBytes;
	if (length > INT_MAX)
		return -1;
	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return -1;
	}

	/* PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE_NR (20 bytes) */
	Stream_Write(s, &tsg->ChannelContext.ContextType, 4); /* ContextType (4 bytes) */
	Stream_Write(s, tsg->ChannelContext.ContextUuid, 16); /* ContextUuid (16 bytes) */
	Stream_Write_UINT32_BE(s, totalDataBytes);            /* totalDataBytes (4 bytes) */
	Stream_Write_UINT32_BE(s, numBuffers);                /* numBuffers (4 bytes) */

	if (buffer1Length > 0)
		Stream_Write_UINT32_BE(s, buffer1Length); /* buffer1Length (4 bytes) */

	if (buffer2Length > 0)
		Stream_Write_UINT32_BE(s, buffer2Length); /* buffer2Length (4 bytes) */

	if (buffer3Length > 0)
		Stream_Write_UINT32_BE(s, buffer3Length); /* buffer3Length (4 bytes) */

	if (buffer1Length > 0)
		Stream_Write(s, buffer1, buffer1Length); /* buffer1 (variable) */

	if (buffer2Length > 0)
		Stream_Write(s, buffer2, buffer2Length); /* buffer2 (variable) */

	if (buffer3Length > 0)
		Stream_Write(s, buffer3, buffer3Length); /* buffer3 (variable) */

	if (!rpc_client_write_call(tsg->rpc, s, TsProxySendToServerOpnum))
		return -1;

	return (int)length;
}

/**
 * OpNum = 1
 *
 * HRESULT TsProxyCreateTunnel(
 * [in, ref] PTSG_PACKET tsgPacket,
 * [out, ref] PTSG_PACKET* tsgPacketResponse,
 * [out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* tunnelContext,
 * [out] unsigned long* tunnelId
 * );
 */

static BOOL TsProxyCreateTunnelWriteRequest(rdpTsg* tsg, const PTSG_PACKET tsgPacket)
{
	BOOL rc = FALSE;
	BOOL write = TRUE;
	UINT16 opnum = 0;
	wStream* s;
	rdpRpc* rpc;

	if (!tsg || !tsg->rpc)
		return FALSE;

	rpc = tsg->rpc;
	WLog_DBG(TAG, "%s: %s", __FUNCTION__, tsg_packet_to_string(tsgPacket));
	s = Stream_New(NULL, 108);

	if (!s)
		return FALSE;

	switch (tsgPacket->packetId)
	{
		case TSG_PACKET_TYPE_VERSIONCAPS:
		{
			PTSG_PACKET_VERSIONCAPS packetVersionCaps = tsgPacket->tsgPacket.packetVersionCaps;
			PTSG_CAPABILITY_NAP tsgCapNap = &packetVersionCaps->tsgCaps->tsgPacket.tsgCapNap;
			Stream_Write_UINT32(s, tsgPacket->packetId); /* PacketId (4 bytes) */
			Stream_Write_UINT32(s, tsgPacket->packetId); /* SwitchValue (4 bytes) */
			Stream_Write_UINT32(s, 0x00020000);          /* PacketVersionCapsPtr (4 bytes) */
			Stream_Write_UINT16(
			    s, packetVersionCaps->tsgHeader.ComponentId); /* ComponentId (2 bytes) */
			Stream_Write_UINT16(s, packetVersionCaps->tsgHeader.PacketId); /* PacketId (2 bytes) */
			Stream_Write_UINT32(s, 0x00020004); /* TsgCapsPtr (4 bytes) */
			Stream_Write_UINT32(s,
			                    packetVersionCaps->numCapabilities); /* NumCapabilities (4 bytes) */
			Stream_Write_UINT16(s, packetVersionCaps->majorVersion); /* MajorVersion (2 bytes) */
			Stream_Write_UINT16(s, packetVersionCaps->minorVersion); /* MinorVersion (2 bytes) */
			Stream_Write_UINT16(
			    s,
			    packetVersionCaps->quarantineCapabilities); /* QuarantineCapabilities (2 bytes) */
			/* 4-byte alignment (30 + 2) */
			Stream_Write_UINT16(s, 0x0000);                             /* pad (2 bytes) */
			Stream_Write_UINT32(s, packetVersionCaps->numCapabilities); /* MaxCount (4 bytes) */
			Stream_Write_UINT32(
			    s, packetVersionCaps->tsgCaps->capabilityType); /* CapabilityType (4 bytes) */
			Stream_Write_UINT32(
			    s, packetVersionCaps->tsgCaps->capabilityType); /* SwitchValue (4 bytes) */
			Stream_Write_UINT32(s, tsgCapNap->capabilities);    /* capabilities (4 bytes) */
			/**
			 * The following 60-byte structure is apparently undocumented,
			 * but parts of it can be matched to known C706 data structures.
			 */
			/*
			 * 8-byte constant (8A E3 13 71 02 F4 36 71) also observed here:
			 * http://lists.samba.org/archive/cifs-protocol/2010-July/001543.html
			 */
			Stream_Write_UINT8(s, 0x8A);
			Stream_Write_UINT8(s, 0xE3);
			Stream_Write_UINT8(s, 0x13);
			Stream_Write_UINT8(s, 0x71);
			Stream_Write_UINT8(s, 0x02);
			Stream_Write_UINT8(s, 0xF4);
			Stream_Write_UINT8(s, 0x36);
			Stream_Write_UINT8(s, 0x71);
			Stream_Write_UINT32(s, 0x00040001); /* 1.4 (version?) */
			Stream_Write_UINT32(s, 0x00000001); /* 1 (element count?) */
			/* p_cont_list_t */
			Stream_Write_UINT8(s, 2);       /* ncontext_elem */
			Stream_Write_UINT8(s, 0x40);    /* reserved1 */
			Stream_Write_UINT16(s, 0x0028); /* reserved2 */
			/* p_syntax_id_t */
			Stream_Write(s, &TSGU_UUID, sizeof(p_uuid_t));
			Stream_Write_UINT32(s, TSGU_SYNTAX_IF_VERSION);
			/* p_syntax_id_t */
			Stream_Write(s, &NDR_UUID, sizeof(p_uuid_t));
			Stream_Write_UINT32(s, NDR_SYNTAX_IF_VERSION);
			opnum = TsProxyCreateTunnelOpnum;
		}
		break;

		case TSG_PACKET_TYPE_REAUTH:
		{
			PTSG_PACKET_REAUTH packetReauth = tsgPacket->tsgPacket.packetReauth;
			PTSG_PACKET_VERSIONCAPS packetVersionCaps =
			    packetReauth->tsgInitialPacket.packetVersionCaps;
			PTSG_CAPABILITY_NAP tsgCapNap = &packetVersionCaps->tsgCaps->tsgPacket.tsgCapNap;
			Stream_Write_UINT32(s, tsgPacket->packetId);         /* PacketId (4 bytes) */
			Stream_Write_UINT32(s, tsgPacket->packetId);         /* SwitchValue (4 bytes) */
			Stream_Write_UINT32(s, 0x00020000);                  /* PacketReauthPtr (4 bytes) */
			Stream_Write_UINT32(s, 0);                           /* ??? (4 bytes) */
			Stream_Write_UINT64(s, packetReauth->tunnelContext); /* TunnelContext (8 bytes) */
			Stream_Write_UINT32(s, TSG_PACKET_TYPE_VERSIONCAPS); /* PacketId (4 bytes) */
			Stream_Write_UINT32(s, TSG_PACKET_TYPE_VERSIONCAPS); /* SwitchValue (4 bytes) */
			Stream_Write_UINT32(s, 0x00020004); /* PacketVersionCapsPtr (4 bytes) */
			Stream_Write_UINT16(
			    s, packetVersionCaps->tsgHeader.ComponentId); /* ComponentId (2 bytes) */
			Stream_Write_UINT16(s, packetVersionCaps->tsgHeader.PacketId); /* PacketId (2 bytes) */
			Stream_Write_UINT32(s, 0x00020008); /* TsgCapsPtr (4 bytes) */
			Stream_Write_UINT32(s,
			                    packetVersionCaps->numCapabilities); /* NumCapabilities (4 bytes) */
			Stream_Write_UINT16(s, packetVersionCaps->majorVersion); /* MajorVersion (2 bytes) */
			Stream_Write_UINT16(s, packetVersionCaps->minorVersion); /* MinorVersion (2 bytes) */
			Stream_Write_UINT16(
			    s,
			    packetVersionCaps->quarantineCapabilities); /* QuarantineCapabilities (2 bytes) */
			/* 4-byte alignment (30 + 2) */
			Stream_Write_UINT16(s, 0x0000);                             /* pad (2 bytes) */
			Stream_Write_UINT32(s, packetVersionCaps->numCapabilities); /* MaxCount (4 bytes) */
			Stream_Write_UINT32(
			    s, packetVersionCaps->tsgCaps->capabilityType); /* CapabilityType (4 bytes) */
			Stream_Write_UINT32(
			    s, packetVersionCaps->tsgCaps->capabilityType); /* SwitchValue (4 bytes) */
			Stream_Write_UINT32(s, tsgCapNap->capabilities);    /* capabilities (4 bytes) */
			opnum = TsProxyCreateTunnelOpnum;
		}
		break;

		default:
			write = FALSE;
			break;
	}

	rc = TRUE;

	if (write)
		return rpc_client_write_call(rpc, s, opnum);

	Stream_Free(s, TRUE);
	return rc;
}

static BOOL TsProxyCreateTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu,
                                            CONTEXT_HANDLE* tunnelContext, UINT32* tunnelId)
{
	BOOL rc = FALSE;
	UINT32 count;
	UINT32 Pointer;
	PTSG_PACKET packet;
	UINT32 SwitchValue;
	UINT32 MessageSwitchValue = 0;
	UINT32 IsMessagePresent;
	rdpContext* context;
	PTSG_PACKET_CAPABILITIES tsgCaps = NULL;
	PTSG_PACKET_VERSIONCAPS versionCaps = NULL;
	TSG_PACKET_STRING_MESSAGE packetStringMessage;
	PTSG_PACKET_CAPS_RESPONSE packetCapsResponse = NULL;
	PTSG_PACKET_QUARENC_RESPONSE packetQuarEncResponse = NULL;

	WINPR_ASSERT(tsg);
	WINPR_ASSERT(tsg->rpc);
	WINPR_ASSERT(tsg->rpc->transport);

	context = transport_get_context(tsg->rpc->transport);
	WINPR_ASSERT(context);

	if (!pdu)
		return FALSE;

	packet = (PTSG_PACKET)calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 12))
		goto fail;

	Stream_Seek_UINT32(pdu->s);                   /* PacketPtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packet->packetId); /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);      /* SwitchValue (4 bytes) */

	WLog_DBG(TAG, "%s: %s", __FUNCTION__, tsg_packet_id_to_string(packet->packetId));

	if ((packet->packetId == TSG_PACKET_TYPE_CAPS_RESPONSE) &&
	    (SwitchValue == TSG_PACKET_TYPE_CAPS_RESPONSE))
	{
		packetCapsResponse = (PTSG_PACKET_CAPS_RESPONSE)calloc(1, sizeof(TSG_PACKET_CAPS_RESPONSE));

		if (!packetCapsResponse)
			goto fail;

		packet->tsgPacket.packetCapsResponse = packetCapsResponse;

		if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 32))
			goto fail;

		Stream_Seek_UINT32(pdu->s); /* PacketQuarResponsePtr (4 bytes) */
		Stream_Read_UINT32(pdu->s,
		                   packetCapsResponse->pktQuarEncResponse.flags); /* Flags (4 bytes) */
		Stream_Read_UINT32(
		    pdu->s,
		    packetCapsResponse->pktQuarEncResponse.certChainLen); /* CertChainLength (4 bytes) */
		Stream_Seek_UINT32(pdu->s);                               /* CertChainDataPtr (4 bytes) */
		Stream_Read(pdu->s, &packetCapsResponse->pktQuarEncResponse.nonce,
		            16);                     /* Nonce (16 bytes) */
		Stream_Read_UINT32(pdu->s, Pointer); /* VersionCapsPtr (4 bytes) */

		if ((Pointer == 0x0002000C) || (Pointer == 0x00020008))
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 16))
				goto fail;

			Stream_Seek_UINT32(pdu->s);                     /* MsgId (4 bytes) */
			Stream_Seek_UINT32(pdu->s);                     /* MsgType (4 bytes) */
			Stream_Read_UINT32(pdu->s, IsMessagePresent);   /* IsMessagePresent (4 bytes) */
			Stream_Read_UINT32(pdu->s, MessageSwitchValue); /* MessageSwitchValue (4 bytes) */
		}

		if (packetCapsResponse->pktQuarEncResponse.certChainLen > 0)
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 16))
				goto fail;

			Stream_Read_UINT32(pdu->s, Pointer); /* MsgPtr (4 bytes): 0x00020014 */
			Stream_Seek_UINT32(pdu->s);          /* MaxCount (4 bytes) */
			Stream_Seek_UINT32(pdu->s);          /* Offset (4 bytes) */
			Stream_Read_UINT32(pdu->s, count);   /* ActualCount (4 bytes) */

			/*
			 * CertChainData is a wide character string, and the count is
			 * given in characters excluding the null terminator, therefore:
			 * size = (count * 2)
			 */
			if (!Stream_SafeSeek(pdu->s, count * 2)) /* CertChainData */
				goto fail;

			/* 4-byte alignment */
			if (!tsg_stream_align(pdu->s, 4))
				goto fail;
		}
		else
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 4))
				goto fail;

			Stream_Read_UINT32(pdu->s, Pointer); /* Ptr (4 bytes) */
		}

		versionCaps = (PTSG_PACKET_VERSIONCAPS)calloc(1, sizeof(TSG_PACKET_VERSIONCAPS));

		if (!versionCaps)
			goto fail;

		packetCapsResponse->pktQuarEncResponse.versionCaps = versionCaps;

		if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 18))
			goto fail;

		Stream_Read_UINT16(pdu->s, versionCaps->tsgHeader.ComponentId); /* ComponentId (2 bytes) */
		Stream_Read_UINT16(pdu->s, versionCaps->tsgHeader.PacketId);    /* PacketId (2 bytes) */

		if (versionCaps->tsgHeader.ComponentId != TS_GATEWAY_TRANSPORT)
		{
			WLog_ERR(TAG, "Unexpected ComponentId: 0x%04" PRIX16 ", Expected TS_GATEWAY_TRANSPORT",
			         versionCaps->tsgHeader.ComponentId);
			goto fail;
		}

		Stream_Read_UINT32(pdu->s, Pointer);                      /* TsgCapsPtr (4 bytes) */
		Stream_Read_UINT32(pdu->s, versionCaps->numCapabilities); /* NumCapabilities (4 bytes) */
		Stream_Read_UINT16(pdu->s, versionCaps->majorVersion);    /* MajorVersion (2 bytes) */
		Stream_Read_UINT16(pdu->s, versionCaps->minorVersion);    /* MinorVersion (2 bytes) */
		Stream_Read_UINT16(
		    pdu->s, versionCaps->quarantineCapabilities); /* QuarantineCapabilities (2 bytes) */

		/* 4-byte alignment */
		if (!tsg_stream_align(pdu->s, 4))
			goto fail;

		tsgCaps = (PTSG_PACKET_CAPABILITIES)calloc(1, sizeof(TSG_PACKET_CAPABILITIES));

		if (!tsgCaps)
			goto fail;

		versionCaps->tsgCaps = tsgCaps;

		if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 16))
			goto fail;

		Stream_Seek_UINT32(pdu->s);                          /* MaxCount (4 bytes) */
		Stream_Read_UINT32(pdu->s, tsgCaps->capabilityType); /* CapabilityType (4 bytes) */
		Stream_Read_UINT32(pdu->s, SwitchValue);             /* SwitchValue (4 bytes) */

		if ((SwitchValue != TSG_CAPABILITY_TYPE_NAP) ||
		    (tsgCaps->capabilityType != TSG_CAPABILITY_TYPE_NAP))
		{
			WLog_ERR(TAG,
			         "Unexpected CapabilityType: 0x%08" PRIX32 ", Expected TSG_CAPABILITY_TYPE_NAP",
			         tsgCaps->capabilityType);
			goto fail;
		}

		Stream_Read_UINT32(pdu->s,
		                   tsgCaps->tsgPacket.tsgCapNap.capabilities); /* Capabilities (4 bytes) */

		switch (MessageSwitchValue)
		{
			case TSG_ASYNC_MESSAGE_CONSENT_MESSAGE:
			case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
				if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 16))
					goto fail;

				Stream_Read_INT32(pdu->s, packetStringMessage.isDisplayMandatory);
				Stream_Read_INT32(pdu->s, packetStringMessage.isConsentMandatory);
				Stream_Read_UINT32(pdu->s, packetStringMessage.msgBytes);
				Stream_Read_UINT32(pdu->s, Pointer);

				if (Pointer)
				{
					if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 12))
						goto fail;

					Stream_Seek_UINT32(pdu->s); /* MaxCount (4 bytes) */
					Stream_Seek_UINT32(pdu->s); /* Offset (4 bytes) */
					Stream_Seek_UINT32(pdu->s); /* Length (4 bytes) */
				}

				if (packetStringMessage.msgBytes > TSG_MESSAGING_MAX_MESSAGE_LENGTH)
				{
					WLog_ERR(TAG, "Out of Spec Message Length %" PRIu32 "",
					         packetStringMessage.msgBytes);
					goto fail;
				}

				packetStringMessage.msgBuffer = (WCHAR*)Stream_Pointer(pdu->s);
				if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, packetStringMessage.msgBytes))
					goto fail;

				if (context->instance)
				{
					rc = IFCALLRESULT(
					    TRUE, context->instance->PresentGatewayMessage, context->instance,
					    TSG_ASYNC_MESSAGE_CONSENT_MESSAGE ? GATEWAY_MESSAGE_CONSENT
					                                      : TSG_ASYNC_MESSAGE_SERVICE_MESSAGE,
					    packetStringMessage.isDisplayMandatory != 0,
					    packetStringMessage.isConsentMandatory != 0, packetStringMessage.msgBytes,
					    packetStringMessage.msgBuffer);
					if (!rc)
						goto fail;
				}

				Stream_Seek(pdu->s, packetStringMessage.msgBytes);
				break;

			case TSG_ASYNC_MESSAGE_REAUTH:
			{
				if (!tsg_stream_align(pdu->s, 8))
					goto fail;

				if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 8))
					goto fail;

				Stream_Seek_UINT64(pdu->s); /* TunnelContext (8 bytes) */
			}
			break;

			default:
				WLog_ERR(TAG, "Unexpected Message Type: 0x%" PRIX32 "", MessageSwitchValue);
				goto fail;
		}

		if (!tsg_stream_align(pdu->s, 4))
			goto fail;

		/* TunnelContext (20 bytes) */
		if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 24))
			goto fail;

		Stream_Read_UINT32(pdu->s, tunnelContext->ContextType); /* ContextType (4 bytes) */
		Stream_Read(pdu->s, tunnelContext->ContextUuid, 16);    /* ContextUuid (16 bytes) */
		Stream_Read_UINT32(pdu->s, *tunnelId);                  /* TunnelId (4 bytes) */
		                                                        /* ReturnValue (4 bytes) */
	}
	else if ((packet->packetId == TSG_PACKET_TYPE_QUARENC_RESPONSE) &&
	         (SwitchValue == TSG_PACKET_TYPE_QUARENC_RESPONSE))
	{
		packetQuarEncResponse =
		    (PTSG_PACKET_QUARENC_RESPONSE)calloc(1, sizeof(TSG_PACKET_QUARENC_RESPONSE));

		if (!packetQuarEncResponse)
			goto fail;

		packet->tsgPacket.packetQuarEncResponse = packetQuarEncResponse;

		if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 32))
			goto fail;

		Stream_Seek_UINT32(pdu->s); /* PacketQuarResponsePtr (4 bytes) */
		Stream_Read_UINT32(pdu->s, packetQuarEncResponse->flags); /* Flags (4 bytes) */
		Stream_Read_UINT32(pdu->s,
		                   packetQuarEncResponse->certChainLen); /* CertChainLength (4 bytes) */
		Stream_Seek_UINT32(pdu->s);                              /* CertChainDataPtr (4 bytes) */
		Stream_Read(pdu->s, &packetQuarEncResponse->nonce, 16);  /* Nonce (16 bytes) */

		if (packetQuarEncResponse->certChainLen > 0)
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 16))
				goto fail;

			Stream_Read_UINT32(pdu->s, Pointer); /* Ptr (4 bytes): 0x0002000C */
			Stream_Seek_UINT32(pdu->s);          /* MaxCount (4 bytes) */
			Stream_Seek_UINT32(pdu->s);          /* Offset (4 bytes) */
			Stream_Read_UINT32(pdu->s, count);   /* ActualCount (4 bytes) */

			/*
			 * CertChainData is a wide character string, and the count is
			 * given in characters excluding the null terminator, therefore:
			 * size = (count * 2)
			 */
			if (!Stream_SafeSeek(pdu->s, count * 2)) /* CertChainData */
				goto fail;

			/* 4-byte alignment */
			if (!tsg_stream_align(pdu->s, 4))
				goto fail;
		}
		else
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 4))
				goto fail;

			Stream_Read_UINT32(pdu->s, Pointer); /* Ptr (4 bytes): 0x00020008 */
		}

		versionCaps = (PTSG_PACKET_VERSIONCAPS)calloc(1, sizeof(TSG_PACKET_VERSIONCAPS));

		if (!versionCaps)
			goto fail;

		packetQuarEncResponse->versionCaps = versionCaps;

		if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 18))
			goto fail;

		Stream_Read_UINT16(pdu->s, versionCaps->tsgHeader.ComponentId); /* ComponentId (2 bytes) */
		Stream_Read_UINT16(pdu->s, versionCaps->tsgHeader.PacketId);    /* PacketId (2 bytes) */

		if (versionCaps->tsgHeader.ComponentId != TS_GATEWAY_TRANSPORT)
		{
			WLog_ERR(TAG, "Unexpected ComponentId: 0x%04" PRIX16 ", Expected TS_GATEWAY_TRANSPORT",
			         versionCaps->tsgHeader.ComponentId);
			goto fail;
		}

		Stream_Read_UINT32(pdu->s, Pointer);                      /* TsgCapsPtr (4 bytes) */
		Stream_Read_UINT32(pdu->s, versionCaps->numCapabilities); /* NumCapabilities (4 bytes) */
		Stream_Read_UINT16(pdu->s, versionCaps->majorVersion);    /* MajorVersion (2 bytes) */
		Stream_Read_UINT16(pdu->s, versionCaps->minorVersion);    /* MinorVersion (2 bytes) */
		Stream_Read_UINT16(
		    pdu->s, versionCaps->quarantineCapabilities); /* QuarantineCapabilities (2 bytes) */

		/* 4-byte alignment */
		if (!tsg_stream_align(pdu->s, 4))
			goto fail;

		if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 36))
			goto fail;

		/* Not sure exactly what this is */
		Stream_Seek_UINT32(pdu->s); /* 0x00000001 (4 bytes) */
		Stream_Seek_UINT32(pdu->s); /* 0x00000001 (4 bytes) */
		Stream_Seek_UINT32(pdu->s); /* 0x00000001 (4 bytes) */
		Stream_Seek_UINT32(pdu->s); /* 0x00000002 (4 bytes) */
		/* TunnelContext (20 bytes) */
		Stream_Read_UINT32(pdu->s, tunnelContext->ContextType); /* ContextType (4 bytes) */
		Stream_Read(pdu->s, tunnelContext->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	}
	else
	{
		WLog_ERR(TAG,
		         "Unexpected PacketId: 0x%08" PRIX32 ", Expected TSG_PACKET_TYPE_CAPS_RESPONSE "
		         "or TSG_PACKET_TYPE_QUARENC_RESPONSE",
		         packet->packetId);
		goto fail;
	}

	rc = TRUE;
fail:
	free(packetQuarEncResponse);
	free(packetCapsResponse);
	free(versionCaps);
	free(tsgCaps);
	free(packet);
	return rc;
}

/**
 * OpNum = 2
 *
 * HRESULT TsProxyAuthorizeTunnel(
 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
 * [in, ref] PTSG_PACKET tsgPacket,
 * [out, ref] PTSG_PACKET* tsgPacketResponse
 * );
 *
 */

static BOOL TsProxyAuthorizeTunnelWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* tunnelContext)
{
	size_t pad;
	wStream* s;
	size_t count;
	size_t offset;
	rdpRpc* rpc;

	if (!tsg || !tsg->rpc || !tunnelContext || !tsg->MachineName)
		return FALSE;

	count = _wcslen(tsg->MachineName) + 1;
	if (count > UINT32_MAX)
		return FALSE;

	rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxyAuthorizeTunnelWriteRequest");
	s = Stream_New(NULL, 1024 + count * 2);

	if (!s)
		return FALSE;

	/* TunnelContext (20 bytes) */
	Stream_Write_UINT32(s, tunnelContext->ContextType); /* ContextType (4 bytes) */
	Stream_Write(s, &tunnelContext->ContextUuid, 16);   /* ContextUuid (16 bytes) */
	/* 4-byte alignment */
	Stream_Write_UINT32(s, TSG_PACKET_TYPE_QUARREQUEST); /* PacketId (4 bytes) */
	Stream_Write_UINT32(s, TSG_PACKET_TYPE_QUARREQUEST); /* SwitchValue (4 bytes) */
	Stream_Write_UINT32(s, 0x00020000);                  /* PacketQuarRequestPtr (4 bytes) */
	Stream_Write_UINT32(s, 0x00000000);                  /* Flags (4 bytes) */
	Stream_Write_UINT32(s, 0x00020004);                  /* MachineNamePtr (4 bytes) */
	Stream_Write_UINT32(s, (UINT32)count);               /* NameLength (4 bytes) */
	Stream_Write_UINT32(s, 0x00020008);                  /* DataPtr (4 bytes) */
	Stream_Write_UINT32(s, 0);                           /* DataLength (4 bytes) */
	/* MachineName */
	Stream_Write_UINT32(s, (UINT32)count);                 /* MaxCount (4 bytes) */
	Stream_Write_UINT32(s, 0);                             /* Offset (4 bytes) */
	Stream_Write_UINT32(s, (UINT32)count);                 /* ActualCount (4 bytes) */
	Stream_Write_UTF16_String(s, tsg->MachineName, count); /* Array */
	/* 4-byte alignment */
	offset = Stream_GetPosition(s);
	pad = rpc_offset_align(&offset, 4);
	Stream_Zero(s, pad);
	Stream_Write_UINT32(s, 0x00000000); /* MaxCount (4 bytes) */
	Stream_SealLength(s);
	return rpc_client_write_call(rpc, s, TsProxyAuthorizeTunnelOpnum);
}

static BOOL TsProxyAuthorizeTunnelReadResponse(RPC_PDU* pdu)
{
	BOOL rc = FALSE;
	UINT32 Pointer;
	UINT32 SizeValue;
	UINT32 SwitchValue;
	UINT32 idleTimeout;
	PTSG_PACKET packet = NULL;
	PTSG_PACKET_RESPONSE packetResponse = NULL;

	if (!pdu)
		return FALSE;

	packet = (PTSG_PACKET)calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 68))
		goto fail;

	Stream_Seek_UINT32(pdu->s);                   /* PacketPtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packet->packetId); /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);      /* SwitchValue (4 bytes) */

	WLog_DBG(TAG, "%s: %s", __FUNCTION__, tsg_packet_id_to_string(packet->packetId));

	if (packet->packetId == E_PROXY_NAP_ACCESSDENIED)
	{
		WLog_ERR(TAG, "status: E_PROXY_NAP_ACCESSDENIED (0x%08X)", E_PROXY_NAP_ACCESSDENIED);
		WLog_ERR(TAG, "Ensure that the Gateway Connection Authorization Policy is correct");
		goto fail;
	}

	if ((packet->packetId != TSG_PACKET_TYPE_RESPONSE) || (SwitchValue != TSG_PACKET_TYPE_RESPONSE))
	{
		WLog_ERR(TAG, "Unexpected PacketId: 0x%08" PRIX32 ", Expected TSG_PACKET_TYPE_RESPONSE",
		         packet->packetId);
		goto fail;
	}

	packetResponse = (PTSG_PACKET_RESPONSE)calloc(1, sizeof(TSG_PACKET_RESPONSE));

	if (!packetResponse)
		goto fail;

	packet->tsgPacket.packetResponse = packetResponse;
	Stream_Read_UINT32(pdu->s, Pointer);               /* PacketResponsePtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packetResponse->flags); /* Flags (4 bytes) */

	if (packetResponse->flags != TSG_PACKET_TYPE_QUARREQUEST)
	{
		WLog_ERR(TAG,
		         "Unexpected Packet Response Flags: 0x%08" PRIX32
		         ", Expected TSG_PACKET_TYPE_QUARREQUEST",
		         packetResponse->flags);
		goto fail;
	}

	Stream_Seek_UINT32(pdu->s);                                  /* Reserved (4 bytes) */
	Stream_Read_UINT32(pdu->s, Pointer);                         /* ResponseDataPtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packetResponse->responseDataLen); /* ResponseDataLength (4 bytes) */
	Stream_Read_INT32(pdu->s, packetResponse->redirectionFlags
	                              .enableAllRedirections); /* EnableAllRedirections (4 bytes) */
	Stream_Read_INT32(pdu->s, packetResponse->redirectionFlags
	                              .disableAllRedirections); /* DisableAllRedirections (4 bytes) */
	Stream_Read_INT32(pdu->s,
	                  packetResponse->redirectionFlags
	                      .driveRedirectionDisabled); /* DriveRedirectionDisabled (4 bytes) */
	Stream_Read_INT32(pdu->s,
	                  packetResponse->redirectionFlags
	                      .printerRedirectionDisabled); /* PrinterRedirectionDisabled (4 bytes) */
	Stream_Read_INT32(pdu->s, packetResponse->redirectionFlags
	                              .portRedirectionDisabled); /* PortRedirectionDisabled (4 bytes) */
	Stream_Read_INT32(pdu->s, packetResponse->redirectionFlags.reserved); /* Reserved (4 bytes) */
	Stream_Read_INT32(
	    pdu->s, packetResponse->redirectionFlags
	                .clipboardRedirectionDisabled); /* ClipboardRedirectionDisabled (4 bytes) */
	Stream_Read_INT32(pdu->s, packetResponse->redirectionFlags
	                              .pnpRedirectionDisabled); /* PnpRedirectionDisabled (4 bytes) */
	Stream_Read_UINT32(pdu->s, SizeValue);                  /* (4 bytes) */

	if (SizeValue != packetResponse->responseDataLen)
	{
		WLog_ERR(TAG, "Unexpected size value: %" PRIu32 ", expected: %" PRIu32 "", SizeValue,
		         packetResponse->responseDataLen);
		goto fail;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, SizeValue))
		goto fail;

	if (SizeValue == 4)
		Stream_Read_UINT32(pdu->s, idleTimeout);
	else
		Stream_Seek(pdu->s, SizeValue); /* ResponseData */

	rc = TRUE;
fail:
	free(packetResponse);
	free(packet);
	return rc;
}

/**
 * OpNum = 3
 *
 * HRESULT TsProxyMakeTunnelCall(
 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
 * [in] unsigned long procId,
 * [in, ref] PTSG_PACKET tsgPacket,
 * [out, ref] PTSG_PACKET* tsgPacketResponse
 * );
 */

static BOOL TsProxyMakeTunnelCallWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* tunnelContext,
                                              UINT32 procId)
{
	wStream* s;
	rdpRpc* rpc;

	if (!tsg || !tsg->rpc || !tunnelContext)
		return FALSE;

	rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxyMakeTunnelCallWriteRequest");
	s = Stream_New(NULL, 40);

	if (!s)
		return FALSE;

	/* TunnelContext (20 bytes) */
	Stream_Write_UINT32(s, tunnelContext->ContextType); /* ContextType (4 bytes) */
	Stream_Write(s, tunnelContext->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	Stream_Write_UINT32(s, procId);                     /* ProcId (4 bytes) */
	/* 4-byte alignment */
	Stream_Write_UINT32(s, TSG_PACKET_TYPE_MSGREQUEST_PACKET); /* PacketId (4 bytes) */
	Stream_Write_UINT32(s, TSG_PACKET_TYPE_MSGREQUEST_PACKET); /* SwitchValue (4 bytes) */
	Stream_Write_UINT32(s, 0x00020000);                        /* PacketMsgRequestPtr (4 bytes) */
	Stream_Write_UINT32(s, 0x00000001);                        /* MaxMessagesPerBatch (4 bytes) */
	return rpc_client_write_call(rpc, s, TsProxyMakeTunnelCallOpnum);
}

static BOOL TsProxyReadPacketSTringMessage(rdpTsg* tsg, wStream* s, TSG_PACKET_STRING_MESSAGE* msg)
{
	UINT32 Pointer, ActualCount, MaxCount;
	if (!tsg || !s || !msg)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 32))
		return FALSE;

	Stream_Read_UINT32(s, Pointer);                /* ConsentMessagePtr (4 bytes) */
	Stream_Read_INT32(s, msg->isDisplayMandatory); /* IsDisplayMandatory (4 bytes) */
	Stream_Read_INT32(s, msg->isConsentMandatory); /* IsConsentMandatory (4 bytes) */
	Stream_Read_UINT32(s, msg->msgBytes);          /* MsgBytes (4 bytes) */
	Stream_Read_UINT32(s, Pointer);                /* MsgPtr (4 bytes) */
	Stream_Read_UINT32(s, MaxCount);               /* MaxCount (4 bytes) */
	Stream_Seek_UINT32(s);                         /* Offset (4 bytes) */
	Stream_Read_UINT32(s, ActualCount);            /* ActualCount (4 bytes) */

	if (msg->msgBytes < ActualCount * 2)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, msg->msgBytes))
		return FALSE;

	msg->msgBuffer = (WCHAR*)Stream_Pointer(s);
	Stream_Seek(s, msg->msgBytes);

	return TRUE;
}

static BOOL TsProxyMakeTunnelCallReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BOOL rc = FALSE;
	UINT32 Pointer;
	UINT32 SwitchValue;
	TSG_PACKET packet;
	rdpContext* context;
	char* messageText = NULL;
	TSG_PACKET_MSG_RESPONSE packetMsgResponse = { 0 };
	TSG_PACKET_STRING_MESSAGE packetStringMessage = { 0 };
	TSG_PACKET_REAUTH_MESSAGE packetReauthMessage = { 0 };

	WINPR_ASSERT(tsg);
	WINPR_ASSERT(tsg->rpc);

	context = transport_get_context(tsg->rpc->transport);
	WINPR_ASSERT(context);

	/* This is an asynchronous response */

	if (!pdu)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 28))
		goto fail;

	Stream_Seek_UINT32(pdu->s);                  /* PacketPtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packet.packetId); /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);     /* SwitchValue (4 bytes) */

	WLog_DBG(TAG, "%s: %s", __FUNCTION__, tsg_packet_id_to_string(packet.packetId));

	if ((packet.packetId != TSG_PACKET_TYPE_MESSAGE_PACKET) ||
	    (SwitchValue != TSG_PACKET_TYPE_MESSAGE_PACKET))
	{
		WLog_ERR(TAG,
		         "Unexpected PacketId: 0x%08" PRIX32 ", Expected TSG_PACKET_TYPE_MESSAGE_PACKET",
		         packet.packetId);
		goto fail;
	}

	Stream_Read_UINT32(pdu->s, Pointer);                       /* PacketMsgResponsePtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packetMsgResponse.msgID);       /* MsgId (4 bytes) */
	Stream_Read_UINT32(pdu->s, packetMsgResponse.msgType);     /* MsgType (4 bytes) */
	Stream_Read_INT32(pdu->s, packetMsgResponse.isMsgPresent); /* IsMsgPresent (4 bytes) */

	/* 2.2.9.2.1.9 TSG_PACKET_MSG_RESPONSE: Ignore empty message body. */
	if (!packetMsgResponse.isMsgPresent)
	{
		rc = TRUE;
		goto fail;
	}

	Stream_Read_UINT32(pdu->s, SwitchValue); /* SwitchValue (4 bytes) */

	switch (SwitchValue)
	{
		case TSG_ASYNC_MESSAGE_CONSENT_MESSAGE:
			if (!TsProxyReadPacketSTringMessage(tsg, pdu->s, &packetStringMessage))
				goto fail;

			ConvertFromUnicode(CP_UTF8, 0, packetStringMessage.msgBuffer,
			                   packetStringMessage.msgBytes / 2, &messageText, 0, NULL, NULL);

			WLog_INFO(TAG, "Consent Message: %s", messageText);
			free(messageText);

			if (context->instance)
			{
				rc = IFCALLRESULT(TRUE, context->instance->PresentGatewayMessage, context->instance,
				                  GATEWAY_MESSAGE_CONSENT,
				                  packetStringMessage.isDisplayMandatory != 0,
				                  packetStringMessage.isConsentMandatory != 0,
				                  packetStringMessage.msgBytes, packetStringMessage.msgBuffer);
				if (!rc)
					goto fail;
			}

			break;

		case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
			if (!TsProxyReadPacketSTringMessage(tsg, pdu->s, &packetStringMessage))
				goto fail;

			ConvertFromUnicode(CP_UTF8, 0, packetStringMessage.msgBuffer,
			                   packetStringMessage.msgBytes / 2, &messageText, 0, NULL, NULL);

			WLog_INFO(TAG, "Service Message: %s", messageText);
			free(messageText);

			if (context->instance)
			{
				rc = IFCALLRESULT(TRUE, context->instance->PresentGatewayMessage, context->instance,
				                  GATEWAY_MESSAGE_SERVICE,
				                  packetStringMessage.isDisplayMandatory != 0,
				                  packetStringMessage.isConsentMandatory != 0,
				                  packetStringMessage.msgBytes, packetStringMessage.msgBuffer);
				if (!rc)
					goto fail;
			}
			break;

		case TSG_ASYNC_MESSAGE_REAUTH:
			if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 20))
				goto fail;

			Stream_Read_UINT32(pdu->s, Pointer); /* ReauthMessagePtr (4 bytes) */
			Stream_Seek_UINT32(pdu->s);          /* alignment pad (4 bytes) */
			Stream_Read_UINT64(pdu->s,
			                   packetReauthMessage.tunnelContext); /* TunnelContext (8 bytes) */
			Stream_Seek_UINT32(pdu->s);                            /* ReturnValue (4 bytes) */
			tsg->ReauthTunnelContext = packetReauthMessage.tunnelContext;
			break;

		default:
			WLog_ERR(TAG, "unexpected message type: %" PRIu32 "", SwitchValue);
			goto fail;
	}

	rc = TRUE;
fail:
	return rc;
}

/**
 * OpNum = 4
 *
 * HRESULT TsProxyCreateChannel(
 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
 * [in, ref] PTSENDPOINTINFO tsEndPointInfo,
 * [out] PCHANNEL_CONTEXT_HANDLE_SERIALIZE* channelContext,
 * [out] unsigned long* channelId
 * );
 */

static BOOL TsProxyCreateChannelWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* tunnelContext)
{
	size_t count;
	wStream* s;
	rdpRpc* rpc;
	WLog_DBG(TAG, "TsProxyCreateChannelWriteRequest");

	if (!tsg || !tsg->rpc || !tunnelContext || !tsg->Hostname)
		return FALSE;

	rpc = tsg->rpc;
	count = _wcslen(tsg->Hostname) + 1;
	if (count > UINT32_MAX)
		return FALSE;
	s = Stream_New(NULL, 60 + count * 2);

	if (!s)
		return FALSE;

	/* TunnelContext (20 bytes) */
	Stream_Write_UINT32(s, tunnelContext->ContextType); /* ContextType (4 bytes) */
	Stream_Write(s, tunnelContext->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	/* TSENDPOINTINFO */
	Stream_Write_UINT32(s, 0x00020000); /* ResourceNamePtr (4 bytes) */
	Stream_Write_UINT32(s, 0x00000001); /* NumResourceNames (4 bytes) */
	Stream_Write_UINT32(s, 0x00000000); /* AlternateResourceNamesPtr (4 bytes) */
	Stream_Write_UINT16(s, 0x0000);     /* NumAlternateResourceNames (2 bytes) */
	Stream_Write_UINT16(s, 0x0000);     /* Pad (2 bytes) */
	/* Port (4 bytes) */
	Stream_Write_UINT16(s, 0x0003);                     /* ProtocolId (RDP = 3) (2 bytes) */
	Stream_Write_UINT16(s, tsg->Port);                  /* PortNumber (0xD3D = 3389) (2 bytes) */
	Stream_Write_UINT32(s, 0x00000001);                 /* NumResourceNames (4 bytes) */
	Stream_Write_UINT32(s, 0x00020004);                 /* ResourceNamePtr (4 bytes) */
	Stream_Write_UINT32(s, (UINT32)count);              /* MaxCount (4 bytes) */
	Stream_Write_UINT32(s, 0);                          /* Offset (4 bytes) */
	Stream_Write_UINT32(s, (UINT32)count);              /* ActualCount (4 bytes) */
	Stream_Write_UTF16_String(s, tsg->Hostname, count); /* Array */
	return rpc_client_write_call(rpc, s, TsProxyCreateChannelOpnum);
}

static BOOL TsProxyCreateChannelReadResponse(RPC_PDU* pdu, CONTEXT_HANDLE* channelContext,
                                             UINT32* channelId)
{
	BOOL rc = FALSE;
	WLog_DBG(TAG, "TsProxyCreateChannelReadResponse");

	if (!pdu)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 28))
		goto fail;

	/* ChannelContext (20 bytes) */
	Stream_Read_UINT32(pdu->s, channelContext->ContextType); /* ContextType (4 bytes) */
	Stream_Read(pdu->s, channelContext->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	Stream_Read_UINT32(pdu->s, *channelId);                  /* ChannelId (4 bytes) */
	Stream_Seek_UINT32(pdu->s);                              /* ReturnValue (4 bytes) */
	rc = TRUE;
fail:
	return rc;
}

/**
 * HRESULT TsProxyCloseChannel(
 * [in, out] PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE* context
 * );
 */

static BOOL TsProxyCloseChannelWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* context)
{
	wStream* s;
	rdpRpc* rpc;
	WLog_DBG(TAG, "TsProxyCloseChannelWriteRequest");

	if (!tsg || !tsg->rpc || !context)
		return FALSE;

	rpc = tsg->rpc;
	s = Stream_New(NULL, 20);

	if (!s)
		return FALSE;

	/* ChannelContext (20 bytes) */
	Stream_Write_UINT32(s, context->ContextType); /* ContextType (4 bytes) */
	Stream_Write(s, context->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	return rpc_client_write_call(rpc, s, TsProxyCloseChannelOpnum);
}

static BOOL TsProxyCloseChannelReadResponse(RPC_PDU* pdu, CONTEXT_HANDLE* context)
{
	BOOL rc = FALSE;
	WLog_DBG(TAG, "TsProxyCloseChannelReadResponse");

	if (!pdu)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 24))
		goto fail;

	/* ChannelContext (20 bytes) */
	Stream_Read_UINT32(pdu->s, context->ContextType); /* ContextType (4 bytes) */
	Stream_Read(pdu->s, context->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	Stream_Seek_UINT32(pdu->s);                       /* ReturnValue (4 bytes) */
	rc = TRUE;
fail:
	return rc;
}

/**
 * HRESULT TsProxyCloseTunnel(
 * [in, out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* context
 * );
 */

static BOOL TsProxyCloseTunnelWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* context)
{
	wStream* s;
	rdpRpc* rpc;
	WLog_DBG(TAG, "TsProxyCloseTunnelWriteRequest");

	if (!tsg || !tsg->rpc || !context)
		return FALSE;

	rpc = tsg->rpc;
	s = Stream_New(NULL, 20);

	if (!s)
		return FALSE;

	/* TunnelContext (20 bytes) */
	Stream_Write_UINT32(s, context->ContextType); /* ContextType (4 bytes) */
	Stream_Write(s, context->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	return rpc_client_write_call(rpc, s, TsProxyCloseTunnelOpnum);
}

static BOOL TsProxyCloseTunnelReadResponse(RPC_PDU* pdu, CONTEXT_HANDLE* context)
{
	BOOL rc = FALSE;
	WLog_DBG(TAG, "TsProxyCloseTunnelReadResponse");

	if (!pdu || !context)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, pdu->s, 24))
		goto fail;

	/* TunnelContext (20 bytes) */
	Stream_Read_UINT32(pdu->s, context->ContextType); /* ContextType (4 bytes) */
	Stream_Read(pdu->s, context->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	Stream_Seek_UINT32(pdu->s);                       /* ReturnValue (4 bytes) */
	rc = TRUE;
fail:
	return rc;
}

/**
 * OpNum = 8
 *
 * DWORD TsProxySetupReceivePipe(
 * [in, max_is(32767)] byte pRpcMessage[]
 * );
 */

static BOOL TsProxySetupReceivePipeWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* channelContext)
{
	wStream* s;
	rdpRpc* rpc;
	WLog_DBG(TAG, "TsProxySetupReceivePipeWriteRequest");

	if (!tsg || !tsg->rpc || !channelContext)
		return FALSE;

	rpc = tsg->rpc;
	s = Stream_New(NULL, 20);

	if (!s)
		return FALSE;

	/* ChannelContext (20 bytes) */
	Stream_Write_UINT32(s, channelContext->ContextType); /* ContextType (4 bytes) */
	Stream_Write(s, channelContext->ContextUuid, 16);    /* ContextUuid (16 bytes) */
	return rpc_client_write_call(rpc, s, TsProxySetupReceivePipeOpnum);
}

static BOOL tsg_transition_to_state(rdpTsg* tsg, TSG_STATE state)
{
	const char* oldState = tsg_state_to_string(tsg->state);
	const char* newState = tsg_state_to_string(state);

	WLog_DBG(TAG, "%s -> %s", oldState, newState);
	return tsg_set_state(tsg, state);
}

BOOL tsg_proxy_begin(rdpTsg* tsg)
{
	TSG_PACKET tsgPacket = { 0 };
	PTSG_CAPABILITY_NAP tsgCapNap;
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;

	if (!tsg)
		return FALSE;

	packetVersionCaps = &tsg->packetVersionCaps;
	packetVersionCaps->tsgCaps = &tsg->tsgCaps;
	tsgCapNap = &tsg->tsgCaps.tsgPacket.tsgCapNap;
	tsgPacket.packetId = TSG_PACKET_TYPE_VERSIONCAPS;
	tsgPacket.tsgPacket.packetVersionCaps = packetVersionCaps;
	packetVersionCaps->tsgHeader.ComponentId = TS_GATEWAY_TRANSPORT;
	packetVersionCaps->tsgHeader.PacketId = TSG_PACKET_TYPE_VERSIONCAPS;
	packetVersionCaps->numCapabilities = 1;
	packetVersionCaps->majorVersion = 1;
	packetVersionCaps->minorVersion = 1;
	packetVersionCaps->quarantineCapabilities = 0;
	packetVersionCaps->tsgCaps->capabilityType = TSG_CAPABILITY_TYPE_NAP;
	/*
	 * Using reduced capabilities appears to trigger
	 * TSG_PACKET_TYPE_QUARENC_RESPONSE instead of TSG_PACKET_TYPE_CAPS_RESPONSE
	 *
	 * However, reduced capabilities may break connectivity with servers enforcing features, such as
	 * "Only allow connections from Remote Desktop Services clients that support RD Gateway
	 * messaging"
	 */
	tsgCapNap->capabilities = TSG_NAP_CAPABILITY_QUAR_SOH | TSG_NAP_CAPABILITY_IDLE_TIMEOUT |
	                          TSG_MESSAGING_CAP_CONSENT_SIGN | TSG_MESSAGING_CAP_SERVICE_MSG |
	                          TSG_MESSAGING_CAP_REAUTH;

	if (!TsProxyCreateTunnelWriteRequest(tsg, &tsgPacket))
	{
		WLog_ERR(TAG, "TsProxyCreateTunnel failure");
		tsg_transition_to_state(tsg, TSG_STATE_FINAL);
		return FALSE;
	}

	return tsg_transition_to_state(tsg, TSG_STATE_INITIAL);
}

static BOOL tsg_proxy_reauth(rdpTsg* tsg)
{
	TSG_PACKET tsgPacket = { 0 };
	PTSG_PACKET_REAUTH packetReauth;
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;

	if (!tsg)
		return FALSE;

	tsg->reauthSequence = TRUE;
	packetReauth = &tsg->packetReauth;
	packetVersionCaps = &tsg->packetVersionCaps;

	if (!packetReauth || !packetVersionCaps)
		return FALSE;

	tsgPacket.packetId = TSG_PACKET_TYPE_REAUTH;
	tsgPacket.tsgPacket.packetReauth = &tsg->packetReauth;
	packetReauth->tunnelContext = tsg->ReauthTunnelContext;
	packetReauth->packetId = TSG_PACKET_TYPE_VERSIONCAPS;
	packetReauth->tsgInitialPacket.packetVersionCaps = packetVersionCaps;

	if (!TsProxyCreateTunnelWriteRequest(tsg, &tsgPacket))
	{
		WLog_ERR(TAG, "TsProxyCreateTunnel failure");
		tsg_transition_to_state(tsg, TSG_STATE_FINAL);
		return FALSE;
	}

	if (!TsProxyMakeTunnelCallWriteRequest(tsg, &tsg->TunnelContext,
	                                       TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST))
	{
		WLog_ERR(TAG, "TsProxyMakeTunnelCall failure");
		tsg_transition_to_state(tsg, TSG_STATE_FINAL);
		return FALSE;
	}

	return tsg_transition_to_state(tsg, TSG_STATE_INITIAL);
}

BOOL tsg_recv_pdu(rdpTsg* tsg, RPC_PDU* pdu)
{
	BOOL rc = FALSE;
	RpcClientCall* call;
	rdpRpc* rpc;

	if (!tsg || !tsg->rpc || !pdu)
		return FALSE;

	rpc = tsg->rpc;

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
	{
		if (!Stream_SafeSeek(pdu->s, 24))
			return FALSE;
	}

	switch (tsg->state)
	{
		case TSG_STATE_INITIAL:
		{
			CONTEXT_HANDLE* TunnelContext;
			TunnelContext = (tsg->reauthSequence) ? &tsg->NewTunnelContext : &tsg->TunnelContext;

			if (!TsProxyCreateTunnelReadResponse(tsg, pdu, TunnelContext, &tsg->TunnelId))
			{
				WLog_ERR(TAG, "TsProxyCreateTunnelReadResponse failure");
				return FALSE;
			}

			if (!tsg_transition_to_state(tsg, TSG_STATE_CONNECTED))
				return FALSE;

			if (!TsProxyAuthorizeTunnelWriteRequest(tsg, TunnelContext))
			{
				WLog_ERR(TAG, "TsProxyAuthorizeTunnel failure");
				return FALSE;
			}

			rc = TRUE;
		}
		break;

		case TSG_STATE_CONNECTED:
		{
			CONTEXT_HANDLE* TunnelContext;
			TunnelContext = (tsg->reauthSequence) ? &tsg->NewTunnelContext : &tsg->TunnelContext;

			if (!TsProxyAuthorizeTunnelReadResponse(pdu))
			{
				WLog_ERR(TAG, "TsProxyAuthorizeTunnelReadResponse failure");
				return FALSE;
			}

			if (!tsg_transition_to_state(tsg, TSG_STATE_AUTHORIZED))
				return FALSE;

			if (!tsg->reauthSequence)
			{
				if (!TsProxyMakeTunnelCallWriteRequest(tsg, TunnelContext,
				                                       TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST))
				{
					WLog_ERR(TAG, "TsProxyMakeTunnelCall failure");
					return FALSE;
				}
			}

			if (!TsProxyCreateChannelWriteRequest(tsg, TunnelContext))
			{
				WLog_ERR(TAG, "TsProxyCreateChannel failure");
				return FALSE;
			}

			rc = TRUE;
		}
		break;

		case TSG_STATE_AUTHORIZED:
			call = rpc_client_call_find_by_id(rpc->client, pdu->CallId);

			if (!call)
				return FALSE;

			if (call->OpNum == TsProxyMakeTunnelCallOpnum)
			{
				if (!TsProxyMakeTunnelCallReadResponse(tsg, pdu))
				{
					WLog_ERR(TAG, "TsProxyMakeTunnelCallReadResponse failure");
					return FALSE;
				}

				rc = TRUE;
			}
			else if (call->OpNum == TsProxyCreateChannelOpnum)
			{
				CONTEXT_HANDLE ChannelContext;

				if (!TsProxyCreateChannelReadResponse(pdu, &ChannelContext, &tsg->ChannelId))
				{
					WLog_ERR(TAG, "TsProxyCreateChannelReadResponse failure");
					return FALSE;
				}

				if (!tsg->reauthSequence)
					CopyMemory(&tsg->ChannelContext, &ChannelContext, sizeof(CONTEXT_HANDLE));
				else
					CopyMemory(&tsg->NewChannelContext, &ChannelContext, sizeof(CONTEXT_HANDLE));

				if (!tsg_transition_to_state(tsg, TSG_STATE_CHANNEL_CREATED))
					return FALSE;

				if (!tsg->reauthSequence)
				{
					if (!TsProxySetupReceivePipeWriteRequest(tsg, &tsg->ChannelContext))
					{
						WLog_ERR(TAG, "TsProxySetupReceivePipe failure");
						return FALSE;
					}
				}
				else
				{
					if (!TsProxyCloseChannelWriteRequest(tsg, &tsg->NewChannelContext))
					{
						WLog_ERR(TAG, "TsProxyCloseChannelWriteRequest failure");
						return FALSE;
					}

					if (!TsProxyCloseTunnelWriteRequest(tsg, &tsg->NewTunnelContext))
					{
						WLog_ERR(TAG, "TsProxyCloseTunnelWriteRequest failure");
						return FALSE;
					}
				}

				rc = tsg_transition_to_state(tsg, TSG_STATE_PIPE_CREATED);
				tsg->reauthSequence = FALSE;
			}
			else
			{
				WLog_ERR(TAG, "TSG_STATE_AUTHORIZED unexpected OpNum: %" PRIu32 "\n", call->OpNum);
			}

			break;

		case TSG_STATE_CHANNEL_CREATED:
			break;

		case TSG_STATE_PIPE_CREATED:
			call = rpc_client_call_find_by_id(rpc->client, pdu->CallId);

			if (!call)
				return FALSE;

			if (call->OpNum == TsProxyMakeTunnelCallOpnum)
			{
				if (!TsProxyMakeTunnelCallReadResponse(tsg, pdu))
				{
					WLog_ERR(TAG, "TsProxyMakeTunnelCallReadResponse failure");
					return FALSE;
				}

				rc = TRUE;

				if (tsg->ReauthTunnelContext)
					rc = tsg_proxy_reauth(tsg);
			}
			else if (call->OpNum == TsProxyCloseChannelOpnum)
			{
				CONTEXT_HANDLE ChannelContext;

				if (!TsProxyCloseChannelReadResponse(pdu, &ChannelContext))
				{
					WLog_ERR(TAG, "TsProxyCloseChannelReadResponse failure");
					return FALSE;
				}

				rc = TRUE;
			}
			else if (call->OpNum == TsProxyCloseTunnelOpnum)
			{
				CONTEXT_HANDLE TunnelContext;

				if (!TsProxyCloseTunnelReadResponse(pdu, &TunnelContext))
				{
					WLog_ERR(TAG, "TsProxyCloseTunnelReadResponse failure");
					return FALSE;
				}

				rc = TRUE;
			}

			break;

		case TSG_STATE_TUNNEL_CLOSE_PENDING:
		{
			CONTEXT_HANDLE ChannelContext;

			if (!TsProxyCloseChannelReadResponse(pdu, &ChannelContext))
			{
				WLog_ERR(TAG, "TsProxyCloseChannelReadResponse failure");
				return FALSE;
			}

			if (!tsg_transition_to_state(tsg, TSG_STATE_CHANNEL_CLOSE_PENDING))
				return FALSE;

			if (!TsProxyCloseChannelWriteRequest(tsg, NULL))
			{
				WLog_ERR(TAG, "TsProxyCloseChannelWriteRequest failure");
				return FALSE;
			}

			if (!TsProxyMakeTunnelCallWriteRequest(tsg, &tsg->TunnelContext,
			                                       TSG_TUNNEL_CANCEL_ASYNC_MSG_REQUEST))
			{
				WLog_ERR(TAG, "TsProxyMakeTunnelCall failure");
				return FALSE;
			}

			rc = TRUE;
		}
		break;

		case TSG_STATE_CHANNEL_CLOSE_PENDING:
		{
			CONTEXT_HANDLE TunnelContext;

			if (!TsProxyCloseTunnelReadResponse(pdu, &TunnelContext))
			{
				WLog_ERR(TAG, "TsProxyCloseTunnelReadResponse failure");
				return FALSE;
			}

			rc = tsg_transition_to_state(tsg, TSG_STATE_FINAL);
		}
		break;

		case TSG_STATE_FINAL:
			break;
	}

	return rc;
}

BOOL tsg_check_event_handles(rdpTsg* tsg)
{
	if (rpc_client_in_channel_recv(tsg->rpc) < 0)
		return FALSE;

	if (rpc_client_out_channel_recv(tsg->rpc) < 0)
		return FALSE;

	return TRUE;
}

DWORD tsg_get_event_handles(rdpTsg* tsg, HANDLE* events, DWORD count)
{
	UINT32 nCount = 0;
	rdpRpc* rpc = tsg->rpc;
	RpcVirtualConnection* connection = rpc->VirtualConnection;

	if (events && (nCount < count))
	{
		events[nCount] = rpc->client->PipeEvent;
		nCount++;
	}
	else
		return 0;

	if (connection->DefaultInChannel && connection->DefaultInChannel->common.tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(connection->DefaultInChannel->common.tls->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	if (connection->NonDefaultInChannel && connection->NonDefaultInChannel->common.tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(connection->NonDefaultInChannel->common.tls->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	if (connection->DefaultOutChannel && connection->DefaultOutChannel->common.tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(connection->DefaultOutChannel->common.tls->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	if (connection->NonDefaultOutChannel && connection->NonDefaultOutChannel->common.tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(connection->NonDefaultOutChannel->common.tls->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	return nCount;
}

static BOOL tsg_set_hostname(rdpTsg* tsg, const char* hostname)
{
	free(tsg->Hostname);
	tsg->Hostname = NULL;
	ConvertToUnicode(CP_UTF8, 0, hostname, -1, &tsg->Hostname, 0);
	return TRUE;
}

static BOOL tsg_set_machine_name(rdpTsg* tsg, const char* machineName)
{
	free(tsg->MachineName);
	tsg->MachineName = NULL;
	ConvertToUnicode(CP_UTF8, 0, machineName, -1, &tsg->MachineName, 0);
	return TRUE;
}

BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port, DWORD timeout)
{
	UINT64 looptimeout = timeout * 1000ULL;
	DWORD nCount;
	HANDLE events[MAXIMUM_WAIT_OBJECTS] = { 0 };
	rdpRpc* rpc;
	rdpContext* context;
	rdpSettings* settings;
	rdpTransport* transport;

	WINPR_ASSERT(tsg);

	rpc = tsg->rpc;
	WINPR_ASSERT(rpc);

	transport = rpc->transport;
	context = transport_get_context(transport);
	WINPR_ASSERT(context);

	settings = context->settings;

	tsg->Port = port;
	tsg->transport = transport;

	if (!settings->GatewayPort)
		settings->GatewayPort = 443;

	if (!tsg_set_hostname(tsg, hostname))
		return FALSE;

	if (!tsg_set_machine_name(tsg, settings->ComputerName))
		return FALSE;

	if (!rpc_connect(rpc, timeout))
	{
		WLog_ERR(TAG, "rpc_connect error!");
		return FALSE;
	}

	nCount = tsg_get_event_handles(tsg, events, ARRAYSIZE(events));

	if (nCount == 0)
		return FALSE;

	while (tsg->state != TSG_STATE_PIPE_CREATED)
	{
		const DWORD polltimeout = 250;
		DWORD status = WaitForMultipleObjects(nCount, events, FALSE, polltimeout);
		if (status == WAIT_TIMEOUT)
		{
			if (timeout > 0)
			{
				if (looptimeout < polltimeout)
					return FALSE;
				looptimeout -= polltimeout;
			}
		}
		else
			looptimeout = timeout * 1000ULL;

		if (!tsg_check_event_handles(tsg))
		{
			WLog_ERR(TAG, "tsg_check failure");
			transport_set_layer(transport, TRANSPORT_LAYER_CLOSED);
			return FALSE;
		}
	}

	WLog_INFO(TAG, "TS Gateway Connection Success");
	tsg->bio = BIO_new(BIO_s_tsg());

	if (!tsg->bio)
		return FALSE;

	BIO_set_data(tsg->bio, (void*)tsg);
	return TRUE;
}

BOOL tsg_disconnect(rdpTsg* tsg)
{
	/**
	 *                        Gateway Shutdown Phase
	 *
	 *     Client                                              Server
	 *        |                                                   |
	 *        |-------------TsProxyCloseChannel Request---------->|
	 *        |                                                   |
	 *        |<-------TsProxySetupReceivePipe Final Response-----|
	 *        |<-----------TsProxyCloseChannel Response-----------|
	 *        |                                                   |
	 *        |----TsProxyMakeTunnelCall Request (cancel async)-->|
	 *        |                                                   |
	 *        |<---TsProxyMakeTunnelCall Response (call async)----|
	 *        |<---TsProxyMakeTunnelCall Response (cancel async)--|
	 *        |                                                   |
	 *        |--------------TsProxyCloseTunnel Request---------->|
	 *        |<-------------TsProxyCloseTunnel Response----------|
	 *        |                                                   |
	 */
	if (!tsg)
		return FALSE;

	if (tsg->state != TSG_STATE_TUNNEL_CLOSE_PENDING)
	{
		if (!TsProxyCloseChannelWriteRequest(tsg, &tsg->ChannelContext))
			return FALSE;

		return tsg_transition_to_state(tsg, TSG_STATE_CHANNEL_CLOSE_PENDING);
	}

	return TRUE;
}

/**
 * @brief
 *
 * @param[in] tsg
 * @param[in] data
 * @param[in] length
 * @return < 0 on error; 0 if not enough data is available (non blocking mode); > 0 bytes to read
 */

static int tsg_read(rdpTsg* tsg, BYTE* data, size_t length)
{
	rdpRpc* rpc;
	int status = 0;

	if (!tsg || !data)
		return -1;

	rpc = tsg->rpc;

	if (transport_get_layer(rpc->transport) == TRANSPORT_LAYER_CLOSED)
	{
		WLog_ERR(TAG, "tsg_read error: connection lost");
		return -1;
	}

	do
	{
		status = rpc_client_receive_pipe_read(rpc->client, data, length);

		if (status < 0)
			return -1;

		if (!status && !transport_get_blocking(rpc->transport))
			return 0;

		if (transport_get_layer(rpc->transport) == TRANSPORT_LAYER_CLOSED)
		{
			WLog_ERR(TAG, "tsg_read error: connection lost");
			return -1;
		}

		if (status > 0)
			break;

		if (transport_get_blocking(rpc->transport))
		{
			while (WaitForSingleObject(rpc->client->PipeEvent, 0) != WAIT_OBJECT_0)
			{
				if (!tsg_check_event_handles(tsg))
					return -1;

				WaitForSingleObject(rpc->client->PipeEvent, 100);
			}
		}
	} while (transport_get_blocking(rpc->transport));

	return status;
}

static int tsg_write(rdpTsg* tsg, const BYTE* data, UINT32 length)
{
	int status;

	if (!tsg || !data || !tsg->rpc || !tsg->rpc->transport)
		return -1;

	if (transport_get_layer(tsg->rpc->transport) == TRANSPORT_LAYER_CLOSED)
	{
		WLog_ERR(TAG, "error, connection lost");
		return -1;
	}

	status = TsProxySendToServer((handle_t)tsg, data, 1, &length);

	if (status < 0)
		return -1;

	return (int)length;
}

rdpTsg* tsg_new(rdpTransport* transport)
{
	rdpTsg* tsg;
	tsg = (rdpTsg*)calloc(1, sizeof(rdpTsg));

	if (!tsg)
		return NULL;

	tsg->transport = transport;
	tsg->rpc = rpc_new(tsg->transport);

	if (!tsg->rpc)
		goto out_free;

	return tsg;
out_free:
	free(tsg);
	return NULL;
}

void tsg_free(rdpTsg* tsg)
{
	if (tsg)
	{
		rpc_free(tsg->rpc);
		free(tsg->Hostname);
		free(tsg->MachineName);
		free(tsg);
	}
}

static int transport_bio_tsg_write(BIO* bio, const char* buf, int num)
{
	int status;
	rdpTsg* tsg = (rdpTsg*)BIO_get_data(bio);
	BIO_clear_flags(bio, BIO_FLAGS_WRITE);

	if (num < 0)
		return -1;
	status = tsg_write(tsg, (const BYTE*)buf, (UINT32)num);

	if (status < 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
		return -1;
	}
	else if (status == 0)
	{
		BIO_set_flags(bio, BIO_FLAGS_WRITE);
		WSASetLastError(WSAEWOULDBLOCK);
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
	rdpTsg* tsg = (rdpTsg*)BIO_get_data(bio);

	if (!tsg || (size < 0))
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
		return -1;
	}

	BIO_clear_flags(bio, BIO_FLAGS_READ);
	status = tsg_read(tsg, (BYTE*)buf, (size_t)size);

	if (status < 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
		return -1;
	}
	else if (status == 0)
	{
		BIO_set_flags(bio, BIO_FLAGS_READ);
		WSASetLastError(WSAEWOULDBLOCK);
	}
	else
	{
		BIO_set_flags(bio, BIO_FLAGS_READ);
	}

	return status > 0 ? status : -1;
}

static int transport_bio_tsg_puts(BIO* bio, const char* str)
{
	WINPR_UNUSED(bio);
	WINPR_UNUSED(str);
	return 1;
}

static int transport_bio_tsg_gets(BIO* bio, char* str, int size)
{
	WINPR_UNUSED(bio);
	WINPR_UNUSED(str);
	WINPR_UNUSED(size);
	return 1;
}

static long transport_bio_tsg_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	long status = -1;
	rdpTsg* tsg = (rdpTsg*)BIO_get_data(bio);
	RpcVirtualConnection* connection = tsg->rpc->VirtualConnection;
	RpcInChannel* inChannel = connection->DefaultInChannel;
	RpcOutChannel* outChannel = connection->DefaultOutChannel;

	switch (cmd)
	{
		case BIO_CTRL_FLUSH:
			(void)BIO_flush(inChannel->common.tls->bio);
			(void)BIO_flush(outChannel->common.tls->bio);
			status = 1;
			break;

		case BIO_C_GET_EVENT:
			if (arg2)
			{
				*((HANDLE*)arg2) = tsg->rpc->client->PipeEvent;
				status = 1;
			}

			break;

		case BIO_C_SET_NONBLOCK:
			status = 1;
			break;

		case BIO_C_READ_BLOCKED:
		{
			BIO* cbio = outChannel->common.bio;
			status = BIO_read_blocked(cbio);
		}
		break;

		case BIO_C_WRITE_BLOCKED:
		{
			BIO* cbio = inChannel->common.bio;
			status = BIO_write_blocked(cbio);
		}
		break;

		case BIO_C_WAIT_READ:
		{
			int timeout = (int)arg1;
			BIO* cbio = outChannel->common.bio;

			if (BIO_read_blocked(cbio))
				return BIO_wait_read(cbio, timeout);
			else if (BIO_write_blocked(cbio))
				return BIO_wait_write(cbio, timeout);
			else
				status = 1;
		}
		break;

		case BIO_C_WAIT_WRITE:
		{
			int timeout = (int)arg1;
			BIO* cbio = inChannel->common.bio;

			if (BIO_write_blocked(cbio))
				status = BIO_wait_write(cbio, timeout);
			else if (BIO_read_blocked(cbio))
				status = BIO_wait_read(cbio, timeout);
			else
				status = 1;
		}
		break;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
		case BIO_CTRL_GET_KTLS_SEND:
			status = 0;
			break;
		case BIO_CTRL_GET_KTLS_RECV:
			status = 0;
			break;
#endif
		default:
			break;
	}

	return status;
}

static int transport_bio_tsg_new(BIO* bio)
{
	WINPR_ASSERT(bio);
	BIO_set_init(bio, 1);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	return 1;
}

static int transport_bio_tsg_free(BIO* bio)
{
	WINPR_ASSERT(bio);
	WINPR_UNUSED(bio);
	return 1;
}

BIO_METHOD* BIO_s_tsg(void)
{
	static BIO_METHOD* bio_methods = NULL;

	if (bio_methods == NULL)
	{
		if (!(bio_methods = BIO_meth_new(BIO_TYPE_TSG, "TSGateway")))
			return NULL;

		BIO_meth_set_write(bio_methods, transport_bio_tsg_write);
		BIO_meth_set_read(bio_methods, transport_bio_tsg_read);
		BIO_meth_set_puts(bio_methods, transport_bio_tsg_puts);
		BIO_meth_set_gets(bio_methods, transport_bio_tsg_gets);
		BIO_meth_set_ctrl(bio_methods, transport_bio_tsg_ctrl);
		BIO_meth_set_create(bio_methods, transport_bio_tsg_new);
		BIO_meth_set_destroy(bio_methods, transport_bio_tsg_free);
	}

	return bio_methods;
}

TSG_STATE tsg_get_state(rdpTsg* tsg)
{
	if (!tsg)
		return TSG_STATE_INITIAL;

	return tsg->state;
}

BIO* tsg_get_bio(rdpTsg* tsg)
{
	if (!tsg)
		return NULL;

	return tsg->bio;
}

BOOL tsg_set_state(rdpTsg* tsg, TSG_STATE state)
{
	if (!tsg)
		return FALSE;

	tsg->state = state;
	return TRUE;
}
