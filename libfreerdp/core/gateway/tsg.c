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

#include "../settings.h"

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/log.h>

#include "rpc_bind.h"
#include "rpc_client.h"
#include "tsg.h"
#include "../utils.h"
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
} TSENDPOINTINFO;

typedef struct
{
	UINT16 ComponentId;
	UINT16 PacketId;
} TSG_PACKET_HEADER;

typedef struct
{
	UINT32 capabilities;
} TSG_CAPABILITY_NAP;

typedef union
{
	TSG_CAPABILITY_NAP tsgCapNap;
} TSG_CAPABILITIES_UNION;

typedef struct
{
	UINT32 capabilityType;
	TSG_CAPABILITIES_UNION tsgPacket;
} TSG_PACKET_CAPABILITIES;

typedef struct
{
	TSG_PACKET_HEADER tsgHeader;
	TSG_PACKET_CAPABILITIES tsgCaps;
	UINT32 numCapabilities;
	UINT16 majorVersion;
	UINT16 minorVersion;
	UINT16 quarantineCapabilities;
} TSG_PACKET_VERSIONCAPS;

typedef struct
{
	UINT32 flags;
} TSG_PACKET_QUARCONFIGREQUEST;

typedef struct
{
	UINT32 flags;
	WCHAR* machineName;
	UINT32 nameLength;
	BYTE* data;
	UINT32 dataLen;
} TSG_PACKET_QUARREQUEST;

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
} TSG_REDIRECTION_FLAGS;

typedef struct
{
	UINT32 flags;
	UINT32 reserved;
	BYTE* responseData;
	UINT32 responseDataLen;
	TSG_REDIRECTION_FLAGS redirectionFlags;
} TSG_PACKET_RESPONSE;

typedef struct
{
	UINT32 flags;
	UINT32 certChainLen;
	WCHAR* certChainData;
	GUID nonce;
	TSG_PACKET_VERSIONCAPS versionCaps;
} TSG_PACKET_QUARENC_RESPONSE;

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
} TSG_PACKET_REAUTH_MESSAGE;

typedef struct
{
	UINT32 msgID;
	UINT32 msgType;
	INT32 isMsgPresent;
} TSG_PACKET_MSG_RESPONSE;

typedef struct
{
	TSG_PACKET_QUARENC_RESPONSE pktQuarEncResponse;
	TSG_PACKET_MSG_RESPONSE pktConsentMessage;
} TSG_PACKET_CAPS_RESPONSE;

typedef struct
{
	UINT32 maxMessagesPerBatch;
} TSG_PACKET_MSG_REQUEST;

typedef struct
{
	TSG_PACKET_VERSIONCAPS tsgVersionCaps;
	UINT32 cookieLen;
	BYTE* cookie;
} TSG_PACKET_AUTH;

typedef union
{
	TSG_PACKET_VERSIONCAPS packetVersionCaps;
	TSG_PACKET_AUTH packetAuth;
} TSG_INITIAL_PACKET_TYPE_UNION;

typedef struct
{
	UINT64 tunnelContext;
	UINT32 packetId;
	TSG_INITIAL_PACKET_TYPE_UNION tsgInitialPacket;
} TSG_PACKET_REAUTH;

typedef union
{
	TSG_PACKET_HEADER packetHeader;
	TSG_PACKET_VERSIONCAPS packetVersionCaps;
	TSG_PACKET_QUARCONFIGREQUEST packetQuarConfigRequest;
	TSG_PACKET_QUARREQUEST packetQuarRequest;
	TSG_PACKET_RESPONSE packetResponse;
	TSG_PACKET_QUARENC_RESPONSE packetQuarEncResponse;
	TSG_PACKET_CAPS_RESPONSE packetCapsResponse;
	TSG_PACKET_MSG_REQUEST packetMsgRequest;
	TSG_PACKET_MSG_RESPONSE packetMsgResponse;
	TSG_PACKET_AUTH packetAuth;
	TSG_PACKET_REAUTH packetReauth;
} TSG_PACKET_TYPE_UNION;

typedef struct
{
	UINT32 packetId;
	TSG_PACKET_TYPE_UNION tsgPacket;
} TSG_PACKET;

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
	wLog* log;
};

static BOOL tsg_stream_align(wLog* log, wStream* s, size_t align);

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

static const char* tsg_component_id_to_string(UINT16 ComponentId, char* buffer, size_t bytelen)
{
	const char* str = NULL;

#define ENTRY(x)  \
	case x:       \
		str = #x; \
		break
	switch (ComponentId)
	{
		ENTRY(TS_GATEWAY_TRANSPORT);
		default:
			str = "TS_UNKNOWN";
			break;
	}
#undef ENTRY

	(void)_snprintf(buffer, bytelen, "%s [0x%04" PRIx16 "]", str, ComponentId);
	return buffer;
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

static BOOL TsProxyReadTunnelContext(wLog* log, wStream* s, CONTEXT_HANDLE* tunnelContext)
{
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 20))
		return FALSE;

	WINPR_ASSERT(tunnelContext);
	Stream_Read_UINT32(s, tunnelContext->ContextType); /* ContextType (4 bytes) */
	Stream_Read(s, &tunnelContext->ContextUuid,
	            sizeof(tunnelContext->ContextUuid)); /* ContextUuid (16 bytes) */
	return TRUE;
}

static BOOL TsProxyWriteTunnelContext(wLog* log, wStream* s, const CONTEXT_HANDLE* tunnelContext)
{
	if (!Stream_EnsureRemainingCapacity(s, 20))
		return FALSE;

	Stream_Write_UINT32(s, tunnelContext->ContextType); /* ContextType (4 bytes) */
	Stream_Write(s, &tunnelContext->ContextUuid,
	             sizeof(tunnelContext->ContextUuid)); /* ContextUuid (16 bytes) */
	return TRUE;
}

static BOOL tsg_ndr_pointer_write(wLog* log, wStream* s, UINT32* index, DWORD length)
{
	WINPR_ASSERT(index);
	const UINT32 ndrPtr = 0x20000 + (*index) * 4;

	if (!s)
		return FALSE;
	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;

	if (length > 0)
	{
		Stream_Write_UINT32(s, ndrPtr); /* mszGroupsNdrPtr (4 bytes) */
		(*index) = (*index) + 1;
	}
	else
		Stream_Write_UINT32(s, 0);
	return TRUE;
}

static BOOL tsg_ndr_pointer_read(wLog* log, wStream* s, UINT32* index, UINT32* ptrval,
                                 BOOL required)
{
	WINPR_ASSERT(index);
	const UINT32 ndrPtr = 0x20000 + (*index) * 4;

	if (!s)
		return FALSE;
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return FALSE;

	DWORD val = 0;
	Stream_Read_UINT32(s, val);
	if (ptrval)
		*ptrval = val;

	if (val != 0)
	{
		if (val != ndrPtr)
		{
			WLog_Print(log, WLOG_WARN, "Read NDR pointer 0x%04" PRIx32 " but expected 0x%04" PRIx32,
			           val, ndrPtr);
			if ((val & 0xFFFF0000) != (ndrPtr & 0xFFFF0000))
				return FALSE;
		}
		(*index)++;
	}
	else if (required)
	{
		WLog_Print(log, WLOG_ERROR, "NDR pointer == 0, but the field is required");
		return FALSE;
	}

	return TRUE;
}

static BOOL tsg_ndr_write_string(wLog* log, wStream* s, const WCHAR* str, UINT32 length)
{
	if (!Stream_EnsureRemainingCapacity(s, 12 + length))
		return FALSE;

	Stream_Write_UINT32(s, length);            /* MaxCount (4 bytes) */
	Stream_Write_UINT32(s, 0);                 /* Offset (4 bytes) */
	Stream_Write_UINT32(s, length);            /* ActualCount (4 bytes) */
	Stream_Write_UTF16_String(s, str, length); /* Array */
	return TRUE;
}

static BOOL tsg_ndr_read_string(wLog* log, wStream* s, WCHAR** str, UINT32 lengthInBytes)
{
	UINT32 MaxCount = 0;
	UINT32 Offset = 0;
	UINT32 ActualCount = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return FALSE;

	Stream_Read_UINT32(s, MaxCount);    /* MaxCount (4 bytes) */
	Stream_Read_UINT32(s, Offset);      /* Offset (4 bytes) */
	Stream_Read_UINT32(s, ActualCount); /* ActualCount (4 bytes) */
	if (ActualCount > MaxCount)
	{
		WLog_Print(log, WLOG_ERROR,
		           "failed to read string, ActualCount (%" PRIu32 ") > MaxCount (%" PRIu32 ")",
		           ActualCount, MaxCount);
		return FALSE;
	}
	if (Offset != 0)
	{
		WLog_Print(log, WLOG_ERROR, "Unsupported Offset (%" PRIu32 "), expected 0", Offset);
		return FALSE;
	}
	if (ActualCount > lengthInBytes / sizeof(WCHAR))
	{
		WLog_Print(log, WLOG_ERROR,
		           "failed to read string, ActualCount (%" PRIu32
		           ") * sizeof(WCHAR) > lengthInBytes (%" PRIu32 ")",
		           ActualCount, lengthInBytes);
		return FALSE;
	}
	if (str)
		*str = Stream_PointerAs(s, WCHAR);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, MaxCount))
		return FALSE;
	Stream_Seek(s, MaxCount);
	return TRUE;
}

static BOOL tsg_ndr_read_packet_header(wLog* log, wStream* s, TSG_PACKET_HEADER* header)
{
	const UINT32 ComponentId = TS_GATEWAY_TRANSPORT;

	WINPR_ASSERT(header);
	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, 2, sizeof(UINT16)))
		return FALSE;
	Stream_Read_UINT16(s, header->ComponentId);
	Stream_Read_UINT16(s, header->PacketId);

	if (ComponentId != header->ComponentId)
	{
		char buffer[64] = { 0 };
		char buffer2[64] = { 0 };
		WLog_Print(log, WLOG_ERROR, "Unexpected ComponentId: %s, Expected %s",
		           tsg_component_id_to_string(header->ComponentId, buffer, sizeof(buffer)),
		           tsg_component_id_to_string(ComponentId, buffer2, sizeof(buffer2)));
		return FALSE;
	}

	return TRUE;
}

static BOOL tsg_ndr_write_packet_header(wLog* log, wStream* s, const TSG_PACKET_HEADER* header)
{
	WINPR_ASSERT(header);
	if (!Stream_EnsureRemainingCapacity(s, 2 * sizeof(UINT16)))
		return FALSE;
	Stream_Write_UINT16(s, header->ComponentId);
	Stream_Write_UINT16(s, header->PacketId);
	return TRUE;
}

static BOOL tsg_ndr_read_nap(wLog* log, wStream* s, TSG_CAPABILITY_NAP* nap)
{
	WINPR_ASSERT(nap);

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, 1, sizeof(UINT32)))
		return FALSE;
	Stream_Read_UINT32(s, nap->capabilities);
	return TRUE;
}

static BOOL tsg_ndr_write_nap(wLog* log, wStream* s, const TSG_CAPABILITY_NAP* nap)
{
	WINPR_ASSERT(nap);

	if (!Stream_EnsureRemainingCapacity(s, 1 * sizeof(UINT32)))
		return FALSE;
	Stream_Write_UINT32(s, nap->capabilities);
	return TRUE;
}

static BOOL tsg_ndr_read_tsg_caps(wLog* log, wStream* s, TSG_PACKET_CAPABILITIES* caps)
{
	UINT32 capabilityType = 0;
	UINT32 count = 0;
	WINPR_ASSERT(caps);

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, 3, sizeof(UINT32)))
		return FALSE;
	Stream_Read_UINT32(s, count);
	Stream_Read_UINT32(s, capabilityType);
	Stream_Read_UINT32(s, caps->capabilityType);
	if (capabilityType != caps->capabilityType)
	{
		WLog_Print(log, WLOG_ERROR, "Inconsistent data, capabilityType %s != %s",
		           tsg_packet_id_to_string(capabilityType),
		           tsg_packet_id_to_string(caps->capabilityType));
		return FALSE;
	}
	switch (caps->capabilityType)
	{
		case TSG_CAPABILITY_TYPE_NAP:
			return tsg_ndr_read_nap(log, s, &caps->tsgPacket.tsgCapNap);
		default:
			WLog_Print(log, WLOG_ERROR,
			           "unknown TSG_PACKET_CAPABILITIES::capabilityType 0x%04" PRIx32,
			           caps->capabilityType);
			return FALSE;
	}
}

static BOOL tsg_ndr_write_tsg_caps(wLog* log, wStream* s, const TSG_PACKET_CAPABILITIES* caps)
{
	WINPR_ASSERT(caps);

	if (!Stream_EnsureRemainingCapacity(s, 2 * sizeof(UINT32)))
		return FALSE;
	Stream_Write_UINT32(s, caps->capabilityType);
	Stream_Write_UINT32(s, caps->capabilityType);

	switch (caps->capabilityType)
	{
		case TSG_CAPABILITY_TYPE_NAP:
			return tsg_ndr_write_nap(log, s, &caps->tsgPacket.tsgCapNap);
		default:
			WLog_Print(log, WLOG_ERROR,
			           "unknown TSG_PACKET_CAPABILITIES::capabilityType 0x%04" PRIx32,
			           caps->capabilityType);
			return FALSE;
	}
}

static BOOL tsg_ndr_read_version_caps(wLog* log, wStream* s, UINT32* index,
                                      TSG_PACKET_VERSIONCAPS* caps)
{
	WINPR_ASSERT(caps);
	if (!tsg_ndr_read_packet_header(log, s, &caps->tsgHeader))
		return FALSE;

	UINT32 TSGCapsPtr = 0;
	if (!tsg_ndr_pointer_read(log, s, index, &TSGCapsPtr, TRUE))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 10))
		return FALSE;
	Stream_Read_UINT32(s, caps->numCapabilities);
	Stream_Read_UINT16(s, caps->majorVersion);
	Stream_Read_UINT16(s, caps->minorVersion);
	Stream_Read_UINT16(s, caps->quarantineCapabilities);
	/* 4-byte alignment */
	if (!tsg_stream_align(log, s, 4))
		return FALSE;

	return tsg_ndr_read_tsg_caps(log, s, &caps->tsgCaps);
}

static BOOL tsg_ndr_write_version_caps(wLog* log, wStream* s, UINT32* index,
                                       const TSG_PACKET_VERSIONCAPS* caps)
{
	WINPR_ASSERT(caps);
	if (!tsg_ndr_write_packet_header(log, s, &caps->tsgHeader))
		return FALSE;

	if (!tsg_ndr_pointer_write(log, s, index, 1)) /* TsgCapsPtr (4 bytes) */
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 10))
		return FALSE;
	Stream_Write_UINT32(s, caps->numCapabilities);
	Stream_Write_UINT16(s, caps->majorVersion);
	Stream_Write_UINT16(s, caps->minorVersion);
	Stream_Write_UINT16(s, caps->quarantineCapabilities);

	/* 4-byte alignment (30 + 2) */
	Stream_Write_UINT16(s, 0x0000);                /* pad (2 bytes) */
	Stream_Write_UINT32(s, caps->numCapabilities); /* MaxCount (4 bytes) */
	return tsg_ndr_write_tsg_caps(log, s, &caps->tsgCaps);
}

static BOOL tsg_ndr_read_quarenc_response(wLog* log, wStream* s, UINT32* index,
                                          TSG_PACKET_QUARENC_RESPONSE* quarenc)
{
	WINPR_ASSERT(quarenc);
	UINT32 CertChainDataPtr = 0;
	UINT32 VersionCapsPtr = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return FALSE;
	Stream_Read_UINT32(s, quarenc->flags);
	Stream_Read_UINT32(s, quarenc->certChainLen);

	if (!tsg_ndr_pointer_read(log, s, index, &CertChainDataPtr, quarenc->certChainLen != 0))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, 1, sizeof(quarenc->nonce)))
		return FALSE;
	Stream_Read(s, &quarenc->nonce, sizeof(quarenc->nonce));

	if (!tsg_ndr_pointer_read(log, s, index, &VersionCapsPtr, TRUE))
		return FALSE;

	return TRUE;
}

static BOOL tsg_ndr_read_quarenc_data(wLog* log, wStream* s, UINT32* index,
                                      TSG_PACKET_QUARENC_RESPONSE* quarenc)
{
	WINPR_ASSERT(quarenc);

	if (quarenc->certChainLen > 0)
	{
		/* [MS-TSGU] 2.2.9.2.1.6  TSG_PACKET_QUARENC_RESPONSE::certChainLen number of WCHAR */
		if (!tsg_ndr_read_string(log, s, &quarenc->certChainData,
		                         quarenc->certChainLen * sizeof(WCHAR)))
			return FALSE;
		/* 4-byte alignment */
		if (!tsg_stream_align(log, s, 4))
			return FALSE;
	}

	return tsg_ndr_read_version_caps(log, s, index, &quarenc->versionCaps);
}

static BOOL tsg_ndr_write_auth(wLog* log, wStream* s, UINT32* index, const TSG_PACKET_AUTH* auth)
{
	WINPR_ASSERT(auth);

	if (!tsg_ndr_write_version_caps(log, s, index, &auth->tsgVersionCaps))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;

	Stream_Write_UINT32(s, auth->cookieLen);
	if (!tsg_ndr_pointer_write(log, s, index, auth->cookieLen))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, auth->cookieLen))
		return FALSE;
	Stream_Write(s, auth->cookie, auth->cookieLen);
	return TRUE;
}

static BOOL tsg_ndr_write_reauth(wLog* log, wStream* s, UINT32* index,
                                 const TSG_PACKET_REAUTH* auth)
{
	WINPR_ASSERT(auth);

	if (!Stream_EnsureRemainingCapacity(s, 12))
		return FALSE;

	Stream_Write_UINT64(s, auth->tunnelContext); /* TunnelContext (8 bytes) */
	Stream_Write_UINT32(s, auth->packetId);      /* PacketId (4 bytes) */

	switch (auth->packetId)
	{
		case TSG_PACKET_TYPE_VERSIONCAPS:
			return tsg_ndr_write_version_caps(log, s, index,
			                                  &auth->tsgInitialPacket.packetVersionCaps);
		case TSG_PACKET_TYPE_AUTH:
			return tsg_ndr_write_auth(log, s, index, &auth->tsgInitialPacket.packetAuth);
		default:
			WLog_Print(log, WLOG_ERROR, "unexpected packetId %s",
			           tsg_packet_id_to_string(auth->packetId));
			return FALSE;
	}
}

static BOOL tsg_ndr_read_packet_response(wLog* log, wStream* s, UINT32* index,
                                         TSG_PACKET_RESPONSE* response)
{
	UINT32 ResponseDataPtr = 0;
	UINT32 MaxSizeValue = 0;
	UINT32 MaxOffsetValue = 0;
	UINT32 idleTimeout = 0;

	WINPR_ASSERT(response);

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, 2, sizeof(UINT32)))
		return FALSE;
	Stream_Read_UINT32(s, response->flags); /* Flags (4 bytes) */
	Stream_Seek_UINT32(s);                  /* Reserved (4 bytes) */

	if (response->flags != TSG_PACKET_TYPE_QUARREQUEST)
	{
		WLog_Print(log, WLOG_ERROR,
		           "Unexpected Packet Response Flags: 0x%08" PRIX32
		           ", Expected TSG_PACKET_TYPE_QUARREQUEST",
		           response->flags);
		return FALSE;
	}

	if (!tsg_ndr_pointer_read(log, s, index, &ResponseDataPtr, TRUE))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, 10, sizeof(UINT32)))
		return FALSE;

	Stream_Read_UINT32(s, response->responseDataLen); /* ResponseDataLength (4 bytes) */
	Stream_Read_INT32(
	    s, response->redirectionFlags.enableAllRedirections); /* EnableAllRedirections (4 bytes) */
	Stream_Read_INT32(
	    s,
	    response->redirectionFlags.disableAllRedirections); /* DisableAllRedirections (4 bytes) */
	Stream_Read_INT32(s, response->redirectionFlags
	                         .driveRedirectionDisabled); /* DriveRedirectionDisabled (4 bytes) */
	Stream_Read_INT32(s,
	                  response->redirectionFlags
	                      .printerRedirectionDisabled); /* PrinterRedirectionDisabled (4 bytes) */
	Stream_Read_INT32(
	    s,
	    response->redirectionFlags.portRedirectionDisabled); /* PortRedirectionDisabled (4 bytes) */
	Stream_Read_INT32(s, response->redirectionFlags.reserved); /* Reserved (4 bytes) */
	Stream_Read_INT32(
	    s, response->redirectionFlags
	           .clipboardRedirectionDisabled); /* ClipboardRedirectionDisabled (4 bytes) */
	Stream_Read_INT32(
	    s,
	    response->redirectionFlags.pnpRedirectionDisabled); /* PnpRedirectionDisabled (4 bytes) */

	Stream_Read_UINT32(s, MaxSizeValue);   /* (4 bytes) */
	Stream_Read_UINT32(s, MaxOffsetValue); /* (4 bytes) */

	if (MaxSizeValue != response->responseDataLen)
	{
		WLog_Print(log, WLOG_ERROR, "Unexpected size value: %" PRIu32 ", expected: %" PRIu32 "",
		           MaxSizeValue, response->responseDataLen);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, MaxSizeValue))
		return FALSE;

	if (MaxSizeValue == 4)
		Stream_Read_UINT32(s, idleTimeout);
	else
		Stream_Seek(s, MaxSizeValue); /* ResponseData */
	return TRUE;
}

WINPR_ATTR_FORMAT_ARG(3, 4)
static BOOL tsg_print(char** buffer, size_t* len, WINPR_FORMAT_ARG const char* fmt, ...)
{
	int rc = 0;
	va_list ap = { 0 };
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
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(length);
	WINPR_ASSERT(caps);

	if (!tsg_print(buffer, length, "capabilities { "))
		return FALSE;

	for (UINT32 x = 0; x < numCaps; x++)
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

	if (!tsg_packet_capabilities_to_string(buffer, length, &caps->tsgCaps, caps->numCapabilities))
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
		name = ConvertWCharNToUtf8Alloc(caps->machineName, caps->nameLength, NULL);
		if (!name)
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

	(void)_snprintf(
	    buffer, size,
	    "enableAllRedirections=%s,  disableAllRedirections=%s, driveRedirectionDisabled=%s, "
	    "printerRedirectionDisabled=%s, portRedirectionDisabled=%s, reserved=%s, "
	    "clipboardRedirectionDisabled=%s, pnpRedirectionDisabled=%s",
	    tsg_bool_to_string(flags->enableAllRedirections),
	    tsg_bool_to_string(flags->disableAllRedirections),
	    tsg_bool_to_string(flags->driveRedirectionDisabled),
	    tsg_bool_to_string(flags->printerRedirectionDisabled),
	    tsg_bool_to_string(flags->portRedirectionDisabled), tsg_bool_to_string(flags->reserved),
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
	RPC_CSTR uuid = NULL;
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
		strdata = ConvertWCharNToUtf8Alloc(caps->certChainData, caps->certChainLen, NULL);
		if (!strdata)
			return FALSE;
	}

	tsg_packet_versioncaps_to_string(&ptbuffer, &size, &caps->versionCaps);
	UuidToStringA(&caps->nonce, &uuid);
	if (strdata || (caps->certChainLen == 0))
		rc =
		    tsg_print(buffer, length,
		              " flags=0x%08" PRIx32 ", certChain[%" PRIu32 "]=%s, nonce=%s, versionCaps=%s",
		              caps->flags, caps->certChainLen, strdata, uuid, tbuffer);
	free(strdata);
	RpcStringFreeA(&uuid);
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
			                                      &caps->tsgInitialPacket.packetVersionCaps);
			break;
		case TSG_PACKET_TYPE_AUTH:
			rc = tsg_packet_auth_to_string(buffer, length, &caps->tsgInitialPacket.packetAuth);
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
			if (!tsg_packet_header_to_string(&buffer, &len, &packet->tsgPacket.packetHeader))
				goto fail;
			break;
		case TSG_PACKET_TYPE_VERSIONCAPS:
			if (!tsg_packet_versioncaps_to_string(&buffer, &len,
			                                      &packet->tsgPacket.packetVersionCaps))
				goto fail;
			break;
		case TSG_PACKET_TYPE_QUARCONFIGREQUEST:
			if (!tsg_packet_quarconfigrequest_to_string(&buffer, &len,
			                                            &packet->tsgPacket.packetQuarConfigRequest))
				goto fail;
			break;
		case TSG_PACKET_TYPE_QUARREQUEST:
			if (!tsg_packet_quarrequest_to_string(&buffer, &len,
			                                      &packet->tsgPacket.packetQuarRequest))
				goto fail;
			break;
		case TSG_PACKET_TYPE_RESPONSE:
			if (!tsg_packet_response_to_string(&buffer, &len, &packet->tsgPacket.packetResponse))
				goto fail;
			break;
		case TSG_PACKET_TYPE_QUARENC_RESPONSE:
			if (!tsg_packet_quarenc_response_to_string(&buffer, &len,
			                                           &packet->tsgPacket.packetQuarEncResponse))
				goto fail;
			break;
		case TSG_PACKET_TYPE_CAPS_RESPONSE:
			if (!tsg_packet_caps_response_to_string(&buffer, &len,
			                                        &packet->tsgPacket.packetCapsResponse))
				goto fail;
			break;
		case TSG_PACKET_TYPE_MSGREQUEST_PACKET:
			if (!tsg_packet_message_request_to_string(&buffer, &len,
			                                          &packet->tsgPacket.packetMsgRequest))
				goto fail;
			break;
		case TSG_PACKET_TYPE_MESSAGE_PACKET:
			if (!tsg_packet_message_response_to_string(&buffer, &len,
			                                           &packet->tsgPacket.packetMsgResponse))
				goto fail;
			break;
		case TSG_PACKET_TYPE_AUTH:
			if (!tsg_packet_auth_to_string(&buffer, &len, &packet->tsgPacket.packetAuth))
				goto fail;
			break;
		case TSG_PACKET_TYPE_REAUTH:
			if (!tsg_packet_reauth_to_string(&buffer, &len, &packet->tsgPacket.packetReauth))
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

static BOOL tsg_stream_align(wLog* log, wStream* s, size_t align)
{
	size_t pos = 0;
	size_t offset = 0;

	if (!s)
		return FALSE;

	pos = Stream_GetPosition(s);

	if ((pos % align) != 0)
		offset = align - pos % align;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, offset))
		return FALSE;
	Stream_Seek(s, offset);
	return TRUE;
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
                               const UINT32* lengths)
{
	wStream* s = NULL;
	rdpTsg* tsg = NULL;
	size_t length = 0;
	const byte* buffer1 = NULL;
	const byte* buffer2 = NULL;
	const byte* buffer3 = NULL;
	UINT32 buffer1Length = 0;
	UINT32 buffer2Length = 0;
	UINT32 buffer3Length = 0;
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
		WLog_Print(tsg->log, WLOG_ERROR, "Stream_New failed!");
		return -1;
	}

	/* PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE_NR (20 bytes) */
	if (!TsProxyWriteTunnelContext(tsg->log, s, &tsg->ChannelContext))
		goto fail;
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
fail:
	Stream_Free(s, TRUE);
	return -1;
}

/**
 * OpNum = 1
 *
 * HRESULT TsProxyCreateTunnel(
 * [in, ref] TSG_PACKET* tsgPacket,
 * [out, ref] TSG_PACKET** tsgPacketResponse,
 * [out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* tunnelContext,
 * [out] unsigned long* tunnelId
 * );
 */

static BOOL TsProxyCreateTunnelWriteRequest(rdpTsg* tsg, const TSG_PACKET* tsgPacket)
{
	BOOL rc = FALSE;
	BOOL write = TRUE;
	UINT16 opnum = 0;
	wStream* s = NULL;
	rdpRpc* rpc = NULL;

	if (!tsg || !tsg->rpc)
		return FALSE;

	rpc = tsg->rpc;
	WLog_Print(tsg->log, WLOG_DEBUG, "%s", tsg_packet_to_string(tsgPacket));
	s = Stream_New(NULL, 108);

	if (!s)
		return FALSE;

	switch (tsgPacket->packetId)
	{
		case TSG_PACKET_TYPE_VERSIONCAPS:
		{
			UINT32 index = 0;
			const TSG_PACKET_VERSIONCAPS* packetVersionCaps =
			    &tsgPacket->tsgPacket.packetVersionCaps;

			Stream_Write_UINT32(s, tsgPacket->packetId); /* PacketId (4 bytes) */
			Stream_Write_UINT32(s, tsgPacket->packetId); /* SwitchValue (4 bytes) */
			if (!tsg_ndr_pointer_write(tsg->log, s, &index, 1)) /* PacketVersionCapsPtr (4 bytes) */
				goto fail;

			if (!tsg_ndr_write_version_caps(tsg->log, s, &index, packetVersionCaps))
				goto fail;
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
			const TSG_PACKET_REAUTH* packetReauth = &tsgPacket->tsgPacket.packetReauth;
			UINT32 index = 0;
			Stream_Write_UINT32(s, tsgPacket->packetId);        /* PacketId (4 bytes) */
			Stream_Write_UINT32(s, tsgPacket->packetId);        /* SwitchValue (4 bytes) */
			if (!tsg_ndr_pointer_write(tsg->log, s, &index, 1)) /* PacketReauthPtr (4 bytes) */
				goto fail;
			if (!tsg_ndr_write_reauth(tsg->log, s, &index, packetReauth))
				goto fail;
			opnum = TsProxyCreateTunnelOpnum;
		}
		break;

		default:
			WLog_Print(tsg->log, WLOG_WARN, "unexpected packetId %s",
			           tsg_packet_id_to_string(tsgPacket->packetId));
			write = FALSE;
			break;
	}

	rc = TRUE;

	if (write)
		return rpc_client_write_call(rpc, s, opnum);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL tsg_ndr_read_consent_message(wLog* log, rdpContext* context, wStream* s, UINT32* index)
{
	TSG_PACKET_STRING_MESSAGE packetStringMessage = { 0 };
	UINT32 Pointer = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(index);

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return FALSE;

	Stream_Read_INT32(s, packetStringMessage.isDisplayMandatory);
	Stream_Read_INT32(s, packetStringMessage.isConsentMandatory);
	Stream_Read_UINT32(s, packetStringMessage.msgBytes);

	if (!tsg_ndr_pointer_read(log, s, index, &Pointer, FALSE))
		return FALSE;

	if (Pointer)
	{
		if (packetStringMessage.msgBytes > TSG_MESSAGING_MAX_MESSAGE_LENGTH)
		{
			WLog_Print(log, WLOG_ERROR, "Out of Spec Message Length %" PRIu32 "",
			           packetStringMessage.msgBytes);
			return FALSE;
		}
		if (!tsg_ndr_read_string(log, s, &packetStringMessage.msgBuffer,
		                         packetStringMessage.msgBytes))
			return FALSE;

		if (context->instance)
		{
			return IFCALLRESULT(TRUE, context->instance->PresentGatewayMessage, context->instance,
			                    TSG_ASYNC_MESSAGE_CONSENT_MESSAGE
			                        ? GATEWAY_MESSAGE_CONSENT
			                        : TSG_ASYNC_MESSAGE_SERVICE_MESSAGE,
			                    packetStringMessage.isDisplayMandatory != 0,
			                    packetStringMessage.isConsentMandatory != 0,
			                    packetStringMessage.msgBytes, packetStringMessage.msgBuffer);
		}
	}
	return TRUE;
}

static BOOL tsg_ndr_read_tunnel_context(wLog* log, wStream* s, CONTEXT_HANDLE* tunnelContext,
                                        UINT32* tunnelId)
{

	if (!tsg_stream_align(log, s, 4))
		return FALSE;

	/* TunnelContext (20 bytes) */
	if (!TsProxyReadTunnelContext(log, s, tunnelContext))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return FALSE;

	WINPR_ASSERT(tunnelId);
	Stream_Read_UINT32(s, *tunnelId); /* TunnelId (4 bytes) */

	UINT32 ReturnValue = 0;
	Stream_Read_UINT32(s, ReturnValue); /* ReturnValue (4 bytes) */
	if (ReturnValue != NO_ERROR)
		WLog_WARN(TAG, "ReturnValue=%s", NtStatus2Tag(ReturnValue));
	return TRUE;
}

static BOOL tsg_ndr_read_caps_response(wLog* log, rdpContext* context, wStream* s, UINT32* index,
                                       UINT32 PacketPtr, TSG_PACKET_CAPS_RESPONSE* caps,
                                       CONTEXT_HANDLE* tunnelContext, UINT32* tunnelId)
{
	UINT32 PacketQuarResponsePtr = 0;
	UINT32 MessageSwitchValue = 0;
	UINT32 MsgId = 0;
	UINT32 MsgType = 0;
	UINT32 IsMessagePresent = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(index);
	WINPR_ASSERT(caps);

	if (!tsg_ndr_pointer_read(log, s, index, &PacketQuarResponsePtr, TRUE))
		goto fail;

	if (!tsg_ndr_read_quarenc_response(log, s, index, &caps->pktQuarEncResponse))
		goto fail;

	if (PacketPtr)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 16))
			goto fail;

		Stream_Read_UINT32(s, MsgId);              /* MsgId (4 bytes) */
		Stream_Read_UINT32(s, MsgType);            /* MsgType (4 bytes) */
		Stream_Read_UINT32(s, IsMessagePresent);   /* IsMessagePresent (4 bytes) */
		Stream_Read_UINT32(s, MessageSwitchValue); /* MessageSwitchValue (4 bytes) */
	}

	{
		UINT32 MsgPtr = 0;
		if (!tsg_ndr_pointer_read(log, s, index, &MsgPtr, TRUE))
			return FALSE;
	}
	if (!tsg_ndr_read_quarenc_data(log, s, index, &caps->pktQuarEncResponse))
		goto fail;

	switch (MessageSwitchValue)
	{
		case TSG_ASYNC_MESSAGE_CONSENT_MESSAGE:
		case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
		{
			if (!tsg_ndr_read_consent_message(log, context, s, index))
				goto fail;
		}
		break;

		case TSG_ASYNC_MESSAGE_REAUTH:
		{
			if (!tsg_stream_align(log, s, 8))
				goto fail;

			if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
				goto fail;

			Stream_Seek_UINT64(s); /* TunnelContext (8 bytes) */
		}
		break;

		default:
			WLog_Print(log, WLOG_ERROR, "Unexpected Message Type: 0x%" PRIX32 "",
			           MessageSwitchValue);
			goto fail;
	}

	return tsg_ndr_read_tunnel_context(log, s, tunnelContext, tunnelId);
fail:
	return FALSE;
}

static BOOL TsProxyCreateTunnelReadResponse(rdpTsg* tsg, const RPC_PDU* pdu,
                                            CONTEXT_HANDLE* tunnelContext, UINT32* tunnelId)
{
	BOOL rc = FALSE;
	UINT32 index = 0;
	TSG_PACKET packet = { 0 };
	UINT32 SwitchValue = 0;
	rdpContext* context = NULL;
	UINT32 PacketPtr = 0;

	WINPR_ASSERT(tsg);
	WINPR_ASSERT(tsg->rpc);
	WINPR_ASSERT(tsg->rpc->transport);

	context = transport_get_context(tsg->rpc->transport);
	WINPR_ASSERT(context);

	if (!pdu)
		return FALSE;

	if (!tsg_ndr_pointer_read(tsg->log, pdu->s, &index, &PacketPtr, TRUE))
		goto fail;

	if (!Stream_CheckAndLogRequiredLengthWLog(tsg->log, pdu->s, 8))
		goto fail;
	Stream_Read_UINT32(pdu->s, packet.packetId); /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);     /* SwitchValue (4 bytes) */

	WLog_Print(tsg->log, WLOG_DEBUG, "%s", tsg_packet_id_to_string(packet.packetId));

	if ((packet.packetId == TSG_PACKET_TYPE_CAPS_RESPONSE) &&
	    (SwitchValue == TSG_PACKET_TYPE_CAPS_RESPONSE))
	{
		if (!tsg_ndr_read_caps_response(tsg->log, context, pdu->s, &index, PacketPtr,
		                                &packet.tsgPacket.packetCapsResponse, tunnelContext,
		                                tunnelId))
			goto fail;
	}
	else if ((packet.packetId == TSG_PACKET_TYPE_QUARENC_RESPONSE) &&
	         (SwitchValue == TSG_PACKET_TYPE_QUARENC_RESPONSE))
	{
		UINT32 PacketQuarResponsePtr = 0;

		if (!tsg_ndr_pointer_read(tsg->log, pdu->s, &index, &PacketQuarResponsePtr, TRUE))
			goto fail;

		if (!tsg_ndr_read_quarenc_response(tsg->log, pdu->s, &index,
		                                   &packet.tsgPacket.packetQuarEncResponse))
			goto fail;

		if (!tsg_ndr_read_quarenc_data(tsg->log, pdu->s, &index,
		                               &packet.tsgPacket.packetQuarEncResponse))
			goto fail;

		if (!tsg_ndr_read_tunnel_context(tsg->log, pdu->s, tunnelContext, tunnelId))
			goto fail;
	}
	else
	{
		WLog_Print(tsg->log, WLOG_ERROR,
		           "Unexpected PacketId: 0x%08" PRIX32 ", Expected TSG_PACKET_TYPE_CAPS_RESPONSE "
		           "or TSG_PACKET_TYPE_QUARENC_RESPONSE",
		           packet.packetId);
		goto fail;
	}

	rc = TRUE;
fail:
	return rc;
}

/**
 * OpNum = 2
 *
 * HRESULT TsProxyAuthorizeTunnel(
 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
 * [in, ref] TSG_PACKET* tsgPacket,
 * [out, ref] TSG_PACKET** tsgPacketResponse
 * );
 *
 */

static BOOL TsProxyAuthorizeTunnelWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* tunnelContext)
{
	size_t pad = 0;
	wStream* s = NULL;
	size_t count = 0;
	size_t offset = 0;
	rdpRpc* rpc = NULL;

	if (!tsg || !tsg->rpc || !tunnelContext || !tsg->MachineName)
		return FALSE;

	count = _wcslen(tsg->MachineName) + 1;
	if (count > UINT32_MAX)
		return FALSE;

	rpc = tsg->rpc;
	WLog_Print(tsg->log, WLOG_DEBUG, "TsProxyAuthorizeTunnelWriteRequest");
	s = Stream_New(NULL, 1024 + count * 2);

	if (!s)
		return FALSE;

	if (!TsProxyWriteTunnelContext(tsg->log, s, tunnelContext))
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	/* 4-byte alignment */
	UINT32 index = 0;
	Stream_Write_UINT32(s, TSG_PACKET_TYPE_QUARREQUEST); /* PacketId (4 bytes) */
	Stream_Write_UINT32(s, TSG_PACKET_TYPE_QUARREQUEST); /* SwitchValue (4 bytes) */
	if (!tsg_ndr_pointer_write(tsg->log, s, &index, 1))  /* PacketQuarRequestPtr (4 bytes) */
		goto fail;
	Stream_Write_UINT32(s, 0x00000000);                  /* Flags (4 bytes) */
	if (!tsg_ndr_pointer_write(tsg->log, s, &index, 1))  /* MachineNamePtr (4 bytes) */
		goto fail;
	Stream_Write_UINT32(s, (UINT32)count);               /* NameLength (4 bytes) */
	if (!tsg_ndr_pointer_write(tsg->log, s, &index, 1))  /* DataPtr (4 bytes) */
		goto fail;
	Stream_Write_UINT32(s, 0);                           /* DataLength (4 bytes) */
	/* MachineName */
	if (!tsg_ndr_write_string(tsg->log, s, tsg->MachineName, count))
		goto fail;
	/* 4-byte alignment */
	offset = Stream_GetPosition(s);
	pad = rpc_offset_align(&offset, 4);
	Stream_Zero(s, pad);
	Stream_Write_UINT32(s, 0x00000000); /* MaxCount (4 bytes) */
	Stream_SealLength(s);
	return rpc_client_write_call(rpc, s, TsProxyAuthorizeTunnelOpnum);
fail:
	Stream_Free(s, TRUE);
	return FALSE;
}

static UINT32 tsg_redir_to_flags(const TSG_REDIRECTION_FLAGS* redirect)
{
	UINT32 flags = 0;
	if (redirect->enableAllRedirections)
		flags |= HTTP_TUNNEL_REDIR_ENABLE_ALL;
	if (redirect->disableAllRedirections)
		flags |= HTTP_TUNNEL_REDIR_DISABLE_ALL;

	if (redirect->driveRedirectionDisabled)
		flags |= HTTP_TUNNEL_REDIR_DISABLE_DRIVE;
	if (redirect->printerRedirectionDisabled)
		flags |= HTTP_TUNNEL_REDIR_DISABLE_PRINTER;
	if (redirect->portRedirectionDisabled)
		flags |= HTTP_TUNNEL_REDIR_DISABLE_PORT;
	if (redirect->clipboardRedirectionDisabled)
		flags |= HTTP_TUNNEL_REDIR_DISABLE_CLIPBOARD;
	if (redirect->pnpRedirectionDisabled)
		flags |= HTTP_TUNNEL_REDIR_DISABLE_PNP;
	return flags;
}

static BOOL tsg_redirect_apply(rdpTsg* tsg, const TSG_REDIRECTION_FLAGS* redirect)
{
	WINPR_ASSERT(tsg);
	WINPR_ASSERT(redirect);

	rdpTransport* transport = tsg->transport;
	WINPR_ASSERT(transport);

	rdpContext* context = transport_get_context(transport);
	UINT32 redirFlags = tsg_redir_to_flags(redirect);
	return utils_apply_gateway_policy(tsg->log, context, redirFlags, "TSG");
}

static BOOL TsProxyAuthorizeTunnelReadResponse(rdpTsg* tsg, const RPC_PDU* pdu)
{
	BOOL rc = FALSE;
	UINT32 SwitchValue = 0;
	UINT32 index = 0;
	TSG_PACKET packet = { 0 };
	UINT32 PacketPtr = 0;
	UINT32 PacketResponsePtr = 0;

	WINPR_ASSERT(tsg);
	WINPR_ASSERT(pdu);

	wLog* log = tsg->log;
	WINPR_ASSERT(log);

	if (!tsg_ndr_pointer_read(log, pdu->s, &index, &PacketPtr, TRUE))
		goto fail;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, pdu->s, 8))
		goto fail;
	Stream_Read_UINT32(pdu->s, packet.packetId);  /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);      /* SwitchValue (4 bytes) */

	WLog_Print(log, WLOG_DEBUG, "%s", tsg_packet_id_to_string(packet.packetId));

	if (packet.packetId == E_PROXY_NAP_ACCESSDENIED)
	{
		WLog_Print(log, WLOG_ERROR, "status: E_PROXY_NAP_ACCESSDENIED (0x%08X)",
		           E_PROXY_NAP_ACCESSDENIED);
		WLog_Print(log, WLOG_ERROR,
		           "Ensure that the Gateway Connection Authorization Policy is correct");
		goto fail;
	}

	if ((packet.packetId != TSG_PACKET_TYPE_RESPONSE) || (SwitchValue != TSG_PACKET_TYPE_RESPONSE))
	{
		WLog_Print(log, WLOG_ERROR,
		           "Unexpected PacketId: 0x%08" PRIX32 ", Expected TSG_PACKET_TYPE_RESPONSE",
		           packet.packetId);
		goto fail;
	}

	if (!tsg_ndr_pointer_read(log, pdu->s, &index, &PacketResponsePtr, TRUE))
		goto fail;

	if (!tsg_ndr_read_packet_response(log, pdu->s, &index, &packet.tsgPacket.packetResponse))
		goto fail;

	rc = TRUE;

	if (packet.tsgPacket.packetResponse.flags & TSG_PACKET_TYPE_QUARREQUEST)
		rc = tsg_redirect_apply(tsg, &packet.tsgPacket.packetResponse.redirectionFlags);
fail:
	return rc;
}

/**
 * OpNum = 3
 *
 * HRESULT TsProxyMakeTunnelCall(
 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
 * [in] unsigned long procId,
 * [in, ref] TSG_PACKET* tsgPacket,
 * [out, ref] TSG_PACKET** tsgPacketResponse
 * );
 */

static BOOL TsProxyMakeTunnelCallWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* tunnelContext,
                                              UINT32 procId)
{
	wStream* s = NULL;
	rdpRpc* rpc = NULL;

	if (!tsg || !tsg->rpc || !tunnelContext)
		return FALSE;

	rpc = tsg->rpc;
	WLog_Print(tsg->log, WLOG_DEBUG, "TsProxyMakeTunnelCallWriteRequest");
	s = Stream_New(NULL, 40);

	if (!s)
		return FALSE;

	/* TunnelContext (20 bytes) */
	UINT32 index = 0;
	if (!TsProxyWriteTunnelContext(tsg->log, s, tunnelContext))
		goto fail;
	Stream_Write_UINT32(s, procId);                     /* ProcId (4 bytes) */
	/* 4-byte alignment */
	Stream_Write_UINT32(s, TSG_PACKET_TYPE_MSGREQUEST_PACKET); /* PacketId (4 bytes) */
	Stream_Write_UINT32(s, TSG_PACKET_TYPE_MSGREQUEST_PACKET); /* SwitchValue (4 bytes) */
	if (!tsg_ndr_pointer_write(tsg->log, s, &index, 1))        /* PacketMsgRequestPtr (4 bytes) */
		goto fail;
	Stream_Write_UINT32(s, 0x00000001);                        /* MaxMessagesPerBatch (4 bytes) */
	return rpc_client_write_call(rpc, s, TsProxyMakeTunnelCallOpnum);
fail:
	Stream_Free(s, TRUE);
	return FALSE;
}

static BOOL TsProxyReadPacketSTringMessage(rdpTsg* tsg, wStream* s, TSG_PACKET_STRING_MESSAGE* msg)
{
	UINT32 ConsentMessagePtr = 0;
	UINT32 MsgPtr = 0;
	UINT32 index = 0;

	if (!tsg || !s || !msg)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(tsg->log, s, 32))
		return FALSE;

	if (!tsg_ndr_pointer_read(tsg->log, s, &index, &ConsentMessagePtr, TRUE))
		return FALSE;

	Stream_Read_INT32(s, msg->isDisplayMandatory); /* IsDisplayMandatory (4 bytes) */
	Stream_Read_INT32(s, msg->isConsentMandatory); /* IsConsentMandatory (4 bytes) */
	Stream_Read_UINT32(s, msg->msgBytes);          /* MsgBytes (4 bytes) */

	if (!tsg_ndr_pointer_read(tsg->log, s, &index, &MsgPtr, TRUE))
		return FALSE;

	return tsg_ndr_read_string(tsg->log, s, &msg->msgBuffer, msg->msgBytes);
}

static BOOL TsProxyMakeTunnelCallReadResponse(rdpTsg* tsg, const RPC_PDU* pdu)
{
	BOOL rc = FALSE;
	UINT32 index = 0;
	UINT32 SwitchValue = 0;
	TSG_PACKET packet;
	rdpContext* context = NULL;
	char* messageText = NULL;
	TSG_PACKET_MSG_RESPONSE packetMsgResponse = { 0 };
	TSG_PACKET_STRING_MESSAGE packetStringMessage = { 0 };
	TSG_PACKET_REAUTH_MESSAGE packetReauthMessage = { 0 };
	UINT32 PacketPtr = 0;
	UINT32 PacketMsgResponsePtr = 0;

	WINPR_ASSERT(tsg);
	WINPR_ASSERT(tsg->rpc);

	context = transport_get_context(tsg->rpc->transport);
	WINPR_ASSERT(context);

	/* This is an asynchronous response */

	if (!pdu)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(tsg->log, pdu->s, 28))
		goto fail;

	if (!tsg_ndr_pointer_read(tsg->log, pdu->s, &index, &PacketPtr, TRUE))
		goto fail;

	Stream_Read_UINT32(pdu->s, packet.packetId); /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);     /* SwitchValue (4 bytes) */

	WLog_Print(tsg->log, WLOG_DEBUG, "%s", tsg_packet_id_to_string(packet.packetId));

	if ((packet.packetId != TSG_PACKET_TYPE_MESSAGE_PACKET) ||
	    (SwitchValue != TSG_PACKET_TYPE_MESSAGE_PACKET))
	{
		WLog_Print(tsg->log, WLOG_ERROR,
		           "Unexpected PacketId: 0x%08" PRIX32 ", Expected TSG_PACKET_TYPE_MESSAGE_PACKET",
		           packet.packetId);
		goto fail;
	}

	if (!tsg_ndr_pointer_read(tsg->log, pdu->s, &index, &PacketMsgResponsePtr, TRUE))
		goto fail;

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

			messageText = ConvertWCharNToUtf8Alloc(
			    packetStringMessage.msgBuffer, packetStringMessage.msgBytes / sizeof(WCHAR), NULL);
			WLog_Print(tsg->log, WLOG_INFO, "Consent Message: %s", messageText);
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

			messageText = ConvertWCharNToUtf8Alloc(
			    packetStringMessage.msgBuffer, packetStringMessage.msgBytes / sizeof(WCHAR), NULL);
			WLog_Print(tsg->log, WLOG_INFO, "Service Message: %s", messageText);
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
		{
			UINT32 ReauthMessagePtr = 0;
			if (!Stream_CheckAndLogRequiredLengthWLog(tsg->log, pdu->s, 20))
				goto fail;

			if (!tsg_ndr_pointer_read(tsg->log, pdu->s, &index, &ReauthMessagePtr, TRUE))
				goto fail;
			Stream_Seek_UINT32(pdu->s);          /* alignment pad (4 bytes) */
			Stream_Read_UINT64(pdu->s,
			                   packetReauthMessage.tunnelContext); /* TunnelContext (8 bytes) */
			Stream_Seek_UINT32(pdu->s);                            /* ReturnValue (4 bytes) */
			tsg->ReauthTunnelContext = packetReauthMessage.tunnelContext;
		}
		break;

		default:
			WLog_Print(tsg->log, WLOG_ERROR, "unexpected message type: %" PRIu32 "", SwitchValue);
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
	WINPR_ASSERT(tsg);
	WINPR_ASSERT(tunnelContext);

	WLog_Print(tsg->log, WLOG_DEBUG, "TsProxyCreateChannelWriteRequest");

	if (!tsg->rpc || !tsg->Hostname)
		return FALSE;

	rdpRpc* rpc = tsg->rpc;
	const size_t count = _wcslen(tsg->Hostname) + 1;
	if (count > UINT32_MAX)
		return FALSE;

	wStream* s = Stream_New(NULL, 60 + count * 2);
	if (!s)
		return FALSE;

	/* TunnelContext (20 bytes) */
	if (!TsProxyWriteTunnelContext(tsg->log, s, tunnelContext))
		goto fail;

	/* TSENDPOINTINFO */
	UINT32 index = 0;
	if (!tsg_ndr_pointer_write(tsg->log, s, &index, 1))
		goto fail;
	Stream_Write_UINT32(s, 0x00000001); /* NumResourceNames (4 bytes) */
	if (!tsg_ndr_pointer_write(tsg->log, s, &index, 0))
		goto fail;
	Stream_Write_UINT16(s, 0x0000);     /* NumAlternateResourceNames (2 bytes) */
	Stream_Write_UINT16(s, 0x0000);     /* Pad (2 bytes) */
	/* Port (4 bytes) */
	Stream_Write_UINT16(s, 0x0003);                     /* ProtocolId (RDP = 3) (2 bytes) */
	Stream_Write_UINT16(s, tsg->Port);                  /* PortNumber (0xD3D = 3389) (2 bytes) */
	Stream_Write_UINT32(s, 0x00000001);                 /* NumResourceNames (4 bytes) */
	if (!tsg_ndr_pointer_write(tsg->log, s, &index, 1))
		goto fail;
	if (!tsg_ndr_write_string(tsg->log, s, tsg->Hostname, count))
		goto fail;
	return rpc_client_write_call(rpc, s, TsProxyCreateChannelOpnum);

fail:
	Stream_Free(s, TRUE);
	return FALSE;
}

static BOOL TsProxyCreateChannelReadResponse(wLog* log, const RPC_PDU* pdu,
                                             CONTEXT_HANDLE* channelContext, UINT32* channelId)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(log);
	WINPR_ASSERT(pdu);
	WINPR_ASSERT(channelId);

	WLog_Print(log, WLOG_DEBUG, "TsProxyCreateChannelReadResponse");

	if (!Stream_CheckAndLogRequiredLengthWLog(log, pdu->s, 28))
		goto fail;

	/* ChannelContext (20 bytes) */
	if (!TsProxyReadTunnelContext(log, pdu->s, channelContext))
		goto fail;
	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, pdu->s, 2, sizeof(UINT32)))
		goto fail;
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
	WINPR_ASSERT(tsg);
	WINPR_ASSERT(context);

	WLog_Print(tsg->log, WLOG_DEBUG, "TsProxyCloseChannelWriteRequest");

	rdpRpc* rpc = tsg->rpc;
	WINPR_ASSERT(rpc);

	wStream* s = Stream_New(NULL, 20);

	if (!s)
		return FALSE;

	/* ChannelContext (20 bytes) */
	if (!TsProxyWriteTunnelContext(tsg->log, s, context))
		goto fail;
	return rpc_client_write_call(rpc, s, TsProxyCloseChannelOpnum);
fail:
	Stream_Free(s, TRUE);
	return FALSE;
}

static BOOL TsProxyCloseChannelReadResponse(wLog* log, const RPC_PDU* pdu, CONTEXT_HANDLE* context)
{
	BOOL rc = FALSE;
	WLog_Print(log, WLOG_DEBUG, "TsProxyCloseChannelReadResponse");

	if (!pdu)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, pdu->s, 24))
		goto fail;

	/* ChannelContext (20 bytes) */
	if (!TsProxyReadTunnelContext(log, pdu->s, context))
		goto fail;

	{
		const size_t len = sizeof(UINT32);
		if (!Stream_CheckAndLogRequiredLengthWLog(log, pdu->s, len))
			goto fail;
		Stream_Seek(pdu->s, len); /* ReturnValue (4 bytes) */
		rc = TRUE;
	}
fail:
	return rc;
}

/**
 * HRESULT TsProxyCloseTunnel(
 * [in, out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* context
 * );
 */

static BOOL TsProxyCloseTunnelWriteRequest(rdpTsg* tsg, const CONTEXT_HANDLE* context)
{
	WINPR_ASSERT(tsg);
	WINPR_ASSERT(context);

	WLog_Print(tsg->log, WLOG_DEBUG, "TsProxyCloseTunnelWriteRequest");

	rdpRpc* rpc = tsg->rpc;
	WINPR_ASSERT(rpc);

	wStream* s = Stream_New(NULL, 20);

	if (!s)
		return FALSE;

	/* TunnelContext (20 bytes) */
	if (!TsProxyWriteTunnelContext(tsg->log, s, context))
		goto fail;
	return rpc_client_write_call(rpc, s, TsProxyCloseTunnelOpnum);
fail:
	Stream_Free(s, TRUE);
	return FALSE;
}

static BOOL TsProxyCloseTunnelReadResponse(wLog* log, const RPC_PDU* pdu, CONTEXT_HANDLE* context)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(log);
	WINPR_ASSERT(pdu);
	WINPR_ASSERT(context);

	WLog_Print(log, WLOG_DEBUG, "TsProxyCloseTunnelReadResponse");

	if (!Stream_CheckAndLogRequiredLengthWLog(log, pdu->s, 24))
		goto fail;

	/* TunnelContext (20 bytes) */
	if (!TsProxyReadTunnelContext(log, pdu->s, context))
		goto fail;
	{
		const size_t len = sizeof(UINT32);
		if (!Stream_CheckAndLogRequiredLengthWLog(log, pdu->s, len))
			goto fail;
		Stream_Seek(pdu->s, len); /* ReturnValue (4 bytes) */
		rc = TRUE;
	}
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

static BOOL TsProxySetupReceivePipeWriteRequest(rdpTsg* tsg, const CONTEXT_HANDLE* channelContext)
{
	wStream* s = NULL;
	rdpRpc* rpc = NULL;
	WLog_Print(tsg->log, WLOG_DEBUG, "TsProxySetupReceivePipeWriteRequest");

	WINPR_ASSERT(tsg);
	WINPR_ASSERT(tsg->rpc);

	if (!channelContext)
		return FALSE;

	rpc = tsg->rpc;
	s = Stream_New(NULL, 20);

	if (!s)
		return FALSE;

	/* ChannelContext (20 bytes) */
	if (!TsProxyWriteTunnelContext(tsg->log, s, channelContext))
		goto fail;
	return rpc_client_write_call(rpc, s, TsProxySetupReceivePipeOpnum);
fail:
	Stream_Free(s, TRUE);
	return FALSE;
}

static BOOL tsg_transition_to_state(rdpTsg* tsg, TSG_STATE state)
{
	WINPR_ASSERT(tsg);
	const char* oldState = tsg_state_to_string(tsg->state);
	const char* newState = tsg_state_to_string(state);

	WLog_Print(tsg->log, WLOG_DEBUG, "%s -> %s", oldState, newState);
	return tsg_set_state(tsg, state);
}

static BOOL tsg_initialize_version_caps(TSG_PACKET_VERSIONCAPS* packetVersionCaps)
{
	WINPR_ASSERT(packetVersionCaps);

	packetVersionCaps->tsgHeader.ComponentId = TS_GATEWAY_TRANSPORT;
	packetVersionCaps->tsgHeader.PacketId = TSG_PACKET_TYPE_VERSIONCAPS;
	packetVersionCaps->numCapabilities = 1;
	packetVersionCaps->majorVersion = 1;
	packetVersionCaps->minorVersion = 1;
	packetVersionCaps->quarantineCapabilities = 0;
	packetVersionCaps->tsgCaps.capabilityType = TSG_CAPABILITY_TYPE_NAP;
	/*
	 * Using reduced capabilities appears to trigger
	 * TSG_PACKET_TYPE_QUARENC_RESPONSE instead of TSG_PACKET_TYPE_CAPS_RESPONSE
	 *
	 * However, reduced capabilities may break connectivity with servers enforcing features, such as
	 * "Only allow connections from Remote Desktop Services clients that support RD Gateway
	 * messaging"
	 */

	packetVersionCaps->tsgCaps.tsgPacket.tsgCapNap.capabilities =
	    TSG_NAP_CAPABILITY_QUAR_SOH | TSG_NAP_CAPABILITY_IDLE_TIMEOUT |
	    TSG_MESSAGING_CAP_CONSENT_SIGN | TSG_MESSAGING_CAP_SERVICE_MSG | TSG_MESSAGING_CAP_REAUTH;
	return TRUE;
}

BOOL tsg_proxy_begin(rdpTsg* tsg)
{
	TSG_PACKET tsgPacket = { 0 };

	WINPR_ASSERT(tsg);

	tsgPacket.packetId = TSG_PACKET_TYPE_VERSIONCAPS;
	if (!tsg_initialize_version_caps(&tsgPacket.tsgPacket.packetVersionCaps) ||
	    !TsProxyCreateTunnelWriteRequest(tsg, &tsgPacket))
	{
		WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCreateTunnel failure");
		tsg_transition_to_state(tsg, TSG_STATE_FINAL);
		return FALSE;
	}

	return tsg_transition_to_state(tsg, TSG_STATE_INITIAL);
}

static BOOL tsg_proxy_reauth(rdpTsg* tsg)
{
	TSG_PACKET tsgPacket = { 0 };

	WINPR_ASSERT(tsg);

	tsg->reauthSequence = TRUE;
	TSG_PACKET_REAUTH* packetReauth = &tsgPacket.tsgPacket.packetReauth;

	tsgPacket.packetId = TSG_PACKET_TYPE_REAUTH;
	packetReauth->tunnelContext = tsg->ReauthTunnelContext;
	packetReauth->packetId = TSG_PACKET_TYPE_VERSIONCAPS;

	if (!tsg_initialize_version_caps(&packetReauth->tsgInitialPacket.packetVersionCaps))
		return FALSE;

	if (!TsProxyCreateTunnelWriteRequest(tsg, &tsgPacket))
	{
		WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCreateTunnel failure");
		tsg_transition_to_state(tsg, TSG_STATE_FINAL);
		return FALSE;
	}

	if (!TsProxyMakeTunnelCallWriteRequest(tsg, &tsg->TunnelContext,
	                                       TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST))
	{
		WLog_Print(tsg->log, WLOG_ERROR, "TsProxyMakeTunnelCall failure");
		tsg_transition_to_state(tsg, TSG_STATE_FINAL);
		return FALSE;
	}

	return tsg_transition_to_state(tsg, TSG_STATE_INITIAL);
}

BOOL tsg_recv_pdu(rdpTsg* tsg, const RPC_PDU* pdu)
{
	BOOL rc = FALSE;
	RpcClientCall* call = NULL;
	rdpRpc* rpc = NULL;

	WINPR_ASSERT(tsg);
	WINPR_ASSERT(pdu);
	WINPR_ASSERT(tsg->rpc);

	rpc = tsg->rpc;

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
	{
		const size_t len = 24;
		if (!Stream_CheckAndLogRequiredLengthWLog(tsg->log, pdu->s, len))
			return FALSE;
		Stream_Seek(pdu->s, len);
	}

	switch (tsg->state)
	{
		case TSG_STATE_INITIAL:
		{
			CONTEXT_HANDLE* TunnelContext = NULL;
			TunnelContext = (tsg->reauthSequence) ? &tsg->NewTunnelContext : &tsg->TunnelContext;

			if (!TsProxyCreateTunnelReadResponse(tsg, pdu, TunnelContext, &tsg->TunnelId))
			{
				WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCreateTunnelReadResponse failure");
				return FALSE;
			}

			if (!tsg_transition_to_state(tsg, TSG_STATE_CONNECTED))
				return FALSE;

			if (!TsProxyAuthorizeTunnelWriteRequest(tsg, TunnelContext))
			{
				WLog_Print(tsg->log, WLOG_ERROR, "TsProxyAuthorizeTunnel failure");
				return FALSE;
			}

			rc = TRUE;
		}
		break;

		case TSG_STATE_CONNECTED:
		{
			CONTEXT_HANDLE* TunnelContext =
			    (tsg->reauthSequence) ? &tsg->NewTunnelContext : &tsg->TunnelContext;

			if (!TsProxyAuthorizeTunnelReadResponse(tsg, pdu))
			{
				WLog_Print(tsg->log, WLOG_ERROR, "TsProxyAuthorizeTunnelReadResponse failure");
				return FALSE;
			}

			if (!tsg_transition_to_state(tsg, TSG_STATE_AUTHORIZED))
				return FALSE;

			if (!tsg->reauthSequence)
			{
				if (!TsProxyMakeTunnelCallWriteRequest(tsg, TunnelContext,
				                                       TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST))
				{
					WLog_Print(tsg->log, WLOG_ERROR, "TsProxyMakeTunnelCall failure");
					return FALSE;
				}
			}

			if (!TsProxyCreateChannelWriteRequest(tsg, TunnelContext))
			{
				WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCreateChannel failure");
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
					WLog_Print(tsg->log, WLOG_ERROR, "TsProxyMakeTunnelCallReadResponse failure");
					return FALSE;
				}

				rc = TRUE;
			}
			else if (call->OpNum == TsProxyCreateChannelOpnum)
			{
				CONTEXT_HANDLE ChannelContext;

				if (!TsProxyCreateChannelReadResponse(tsg->log, pdu, &ChannelContext,
				                                      &tsg->ChannelId))
				{
					WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCreateChannelReadResponse failure");
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
						WLog_Print(tsg->log, WLOG_ERROR, "TsProxySetupReceivePipe failure");
						return FALSE;
					}
				}
				else
				{
					if (!TsProxyCloseChannelWriteRequest(tsg, &tsg->NewChannelContext))
					{
						WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCloseChannelWriteRequest failure");
						return FALSE;
					}

					if (!TsProxyCloseTunnelWriteRequest(tsg, &tsg->NewTunnelContext))
					{
						WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCloseTunnelWriteRequest failure");
						return FALSE;
					}
				}

				rc = tsg_transition_to_state(tsg, TSG_STATE_PIPE_CREATED);
				tsg->reauthSequence = FALSE;
			}
			else
			{
				WLog_Print(tsg->log, WLOG_ERROR,
				           "TSG_STATE_AUTHORIZED unexpected OpNum: %" PRIu32 "\n", call->OpNum);
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
					WLog_Print(tsg->log, WLOG_ERROR, "TsProxyMakeTunnelCallReadResponse failure");
					return FALSE;
				}

				rc = TRUE;

				if (tsg->ReauthTunnelContext)
					rc = tsg_proxy_reauth(tsg);
			}
			else if (call->OpNum == TsProxyCloseChannelOpnum)
			{
				CONTEXT_HANDLE ChannelContext;

				if (!TsProxyCloseChannelReadResponse(tsg->log, pdu, &ChannelContext))
				{
					WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCloseChannelReadResponse failure");
					return FALSE;
				}

				rc = TRUE;
			}
			else if (call->OpNum == TsProxyCloseTunnelOpnum)
			{
				CONTEXT_HANDLE TunnelContext;

				if (!TsProxyCloseTunnelReadResponse(tsg->log, pdu, &TunnelContext))
				{
					WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCloseTunnelReadResponse failure");
					return FALSE;
				}

				rc = TRUE;
			}

			break;

		case TSG_STATE_TUNNEL_CLOSE_PENDING:
		{
			CONTEXT_HANDLE ChannelContext;

			if (!TsProxyCloseChannelReadResponse(tsg->log, pdu, &ChannelContext))
			{
				WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCloseChannelReadResponse failure");
				return FALSE;
			}

			if (!tsg_transition_to_state(tsg, TSG_STATE_CHANNEL_CLOSE_PENDING))
				return FALSE;

			if (!TsProxyCloseChannelWriteRequest(tsg, NULL))
			{
				WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCloseChannelWriteRequest failure");
				return FALSE;
			}

			if (!TsProxyMakeTunnelCallWriteRequest(tsg, &tsg->TunnelContext,
			                                       TSG_TUNNEL_CANCEL_ASYNC_MSG_REQUEST))
			{
				WLog_Print(tsg->log, WLOG_ERROR, "TsProxyMakeTunnelCall failure");
				return FALSE;
			}

			rc = TRUE;
		}
		break;

		case TSG_STATE_CHANNEL_CLOSE_PENDING:
		{
			CONTEXT_HANDLE TunnelContext;

			if (!TsProxyCloseTunnelReadResponse(tsg->log, pdu, &TunnelContext))
			{
				WLog_Print(tsg->log, WLOG_ERROR, "TsProxyCloseTunnelReadResponse failure");
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
	WINPR_ASSERT(tsg);
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
	WINPR_ASSERT(tsg);
	free(tsg->Hostname);
	tsg->Hostname = ConvertUtf8ToWCharAlloc(hostname, NULL);
	return tsg->Hostname != NULL;
}

static BOOL tsg_set_machine_name(rdpTsg* tsg, const char* machineName)
{
	WINPR_ASSERT(tsg);
	free(tsg->MachineName);
	tsg->MachineName = ConvertUtf8ToWCharAlloc(machineName, NULL);
	return tsg->MachineName != NULL;
}

BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port, DWORD timeout)
{
	UINT64 looptimeout = timeout * 1000ULL;
	DWORD nCount = 0;
	HANDLE events[MAXIMUM_WAIT_OBJECTS] = { 0 };
	rdpRpc* rpc = NULL;
	rdpContext* context = NULL;
	rdpSettings* settings = NULL;
	rdpTransport* transport = NULL;

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
		WLog_Print(tsg->log, WLOG_ERROR, "rpc_connect error!");
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
			WLog_Print(tsg->log, WLOG_ERROR, "tsg_check failure");
			transport_set_layer(transport, TRANSPORT_LAYER_CLOSED);
			return FALSE;
		}
	}

	WLog_Print(tsg->log, WLOG_INFO, "TS Gateway Connection Success");
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
 * @brief Read data from TSG
 *
 * @param[in] tsg The TSG instance to read from
 * @param[in] data A pointer to the data buffer
 * @param[in] length length of data
 *
 * @return < 0 on error; 0 if not enough data is available (non blocking mode); > 0 bytes to read
 */

static int tsg_read(rdpTsg* tsg, BYTE* data, size_t length)
{
	rdpRpc* rpc = NULL;
	int status = 0;

	if (!tsg || !data)
		return -1;

	rpc = tsg->rpc;

	if (transport_get_layer(rpc->transport) == TRANSPORT_LAYER_CLOSED)
	{
		WLog_Print(tsg->log, WLOG_ERROR, "tsg_read error: connection lost");
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
			WLog_Print(tsg->log, WLOG_ERROR, "tsg_read error: connection lost");
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

				(void)WaitForSingleObject(rpc->client->PipeEvent, 100);
			}
		}
	} while (transport_get_blocking(rpc->transport));

	return status;
}

static int tsg_write(rdpTsg* tsg, const BYTE* data, UINT32 length)
{
	int status = 0;

	if (!tsg || !data || !tsg->rpc || !tsg->rpc->transport)
		return -1;

	if (transport_get_layer(tsg->rpc->transport) == TRANSPORT_LAYER_CLOSED)
	{
		WLog_Print(tsg->log, WLOG_ERROR, "error, connection lost");
		return -1;
	}

	status = TsProxySendToServer((handle_t)tsg, data, 1, &length);

	if (status < 0)
		return -1;

	return (int)length;
}

rdpTsg* tsg_new(rdpTransport* transport)
{
	rdpTsg* tsg = (rdpTsg*)calloc(1, sizeof(rdpTsg));

	if (!tsg)
		return NULL;
	tsg->log = WLog_Get(TAG);
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
	int status = 0;
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
	int status = 0;
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

// NOLINTNEXTLINE(readability-non-const-parameter)
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
