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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/ndr.h>
#include <winpr/error.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/log.h>

#include "rpc_bind.h"
#include "rpc_client.h"
#include "tsg.h"
#include "../../crypto/opensslcompat.h"

#define TAG FREERDP_TAG("core.gateway.tsg")

typedef WCHAR* RESOURCENAME;

typedef struct _tsendpointinfo
{
	RESOURCENAME* resourceName;
	UINT32 numResourceNames;
	RESOURCENAME* alternateResourceNames;
	UINT16 numAlternateResourceNames;
	UINT32 Port;
} TSENDPOINTINFO, *PTSENDPOINTINFO;

typedef struct _TSG_PACKET_HEADER
{
	UINT16 ComponentId;
	UINT16 PacketId;
} TSG_PACKET_HEADER, *PTSG_PACKET_HEADER;

typedef struct _TSG_CAPABILITY_NAP
{
	UINT32 capabilities;
} TSG_CAPABILITY_NAP, *PTSG_CAPABILITY_NAP;

typedef union {
	TSG_CAPABILITY_NAP tsgCapNap;
} TSG_CAPABILITIES_UNION, *PTSG_CAPABILITIES_UNION;

typedef struct _TSG_PACKET_CAPABILITIES
{
	UINT32 capabilityType;
	TSG_CAPABILITIES_UNION tsgPacket;
} TSG_PACKET_CAPABILITIES, *PTSG_PACKET_CAPABILITIES;

typedef struct _TSG_PACKET_VERSIONCAPS
{
	TSG_PACKET_HEADER tsgHeader;
	PTSG_PACKET_CAPABILITIES tsgCaps;
	UINT32 numCapabilities;
	UINT16 majorVersion;
	UINT16 minorVersion;
	UINT16 quarantineCapabilities;
} TSG_PACKET_VERSIONCAPS, *PTSG_PACKET_VERSIONCAPS;

typedef struct _TSG_PACKET_QUARCONFIGREQUEST
{
	UINT32 flags;
} TSG_PACKET_QUARCONFIGREQUEST, *PTSG_PACKET_QUARCONFIGREQUEST;

typedef struct _TSG_PACKET_QUARREQUEST
{
	UINT32 flags;
	WCHAR* machineName;
	UINT32 nameLength;
	BYTE* data;
	UINT32 dataLen;
} TSG_PACKET_QUARREQUEST, *PTSG_PACKET_QUARREQUEST;

typedef struct _TSG_REDIRECTION_FLAGS
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

typedef struct _TSG_PACKET_RESPONSE
{
	UINT32 flags;
	UINT32 reserved;
	BYTE* responseData;
	UINT32 responseDataLen;
	TSG_REDIRECTION_FLAGS redirectionFlags;
} TSG_PACKET_RESPONSE, *PTSG_PACKET_RESPONSE;

typedef struct _TSG_PACKET_QUARENC_RESPONSE
{
	UINT32 flags;
	UINT32 certChainLen;
	WCHAR* certChainData;
	GUID nonce;
	PTSG_PACKET_VERSIONCAPS versionCaps;
} TSG_PACKET_QUARENC_RESPONSE, *PTSG_PACKET_QUARENC_RESPONSE;

typedef struct TSG_PACKET_STRING_MESSAGE
{
	INT32 isDisplayMandatory;
	INT32 isConsentMandatory;
	UINT32 msgBytes;
	WCHAR* msgBuffer;
} TSG_PACKET_STRING_MESSAGE;

typedef struct TSG_PACKET_REAUTH_MESSAGE
{
	UINT64 tunnelContext;
} TSG_PACKET_REAUTH_MESSAGE, *PTSG_PACKET_REAUTH_MESSAGE;

typedef struct _TSG_PACKET_MSG_RESPONSE
{
	UINT32 msgID;
	UINT32 msgType;
	INT32 isMsgPresent;
} TSG_PACKET_MSG_RESPONSE, *PTSG_PACKET_MSG_RESPONSE;

typedef struct TSG_PACKET_CAPS_RESPONSE
{
	TSG_PACKET_QUARENC_RESPONSE pktQuarEncResponse;
	TSG_PACKET_MSG_RESPONSE pktConsentMessage;
} TSG_PACKET_CAPS_RESPONSE, *PTSG_PACKET_CAPS_RESPONSE;

typedef struct TSG_PACKET_MSG_REQUEST
{
	UINT32 maxMessagesPerBatch;
} TSG_PACKET_MSG_REQUEST, *PTSG_PACKET_MSG_REQUEST;

typedef struct _TSG_PACKET_AUTH
{
	TSG_PACKET_VERSIONCAPS tsgVersionCaps;
	UINT32 cookieLen;
	BYTE* cookie;
} TSG_PACKET_AUTH, *PTSG_PACKET_AUTH;

typedef union {
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;
	PTSG_PACKET_AUTH packetAuth;
} TSG_INITIAL_PACKET_TYPE_UNION, *PTSG_INITIAL_PACKET_TYPE_UNION;

typedef struct TSG_PACKET_REAUTH
{
	UINT64 tunnelContext;
	UINT32 packetId;
	TSG_INITIAL_PACKET_TYPE_UNION tsgInitialPacket;
} TSG_PACKET_REAUTH, *PTSG_PACKET_REAUTH;

typedef union {
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

typedef struct _TSG_PACKET
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
	rdpSettings* settings;
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
	int length;
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

	length = 28 + totalDataBytes;
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

	return length;
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

static BOOL TsProxyCreateTunnelWriteRequest(rdpTsg* tsg, PTSG_PACKET tsgPacket)
{
	BOOL rc = FALSE;
	BOOL write = TRUE;
	UINT16 opnum = 0;
	wStream* s;
	rdpRpc* rpc;

	if (!tsg || !tsg->rpc)
		return FALSE;

	rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxyCreateTunnelWriteRequest");
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
	UINT32 MsgBytes;
	PTSG_PACKET_CAPABILITIES tsgCaps = NULL;
	PTSG_PACKET_VERSIONCAPS versionCaps = NULL;
	PTSG_PACKET_CAPS_RESPONSE packetCapsResponse = NULL;
	PTSG_PACKET_QUARENC_RESPONSE packetQuarEncResponse = NULL;
	WLog_DBG(TAG, "TsProxyCreateTunnelReadResponse");

	if (!pdu)
		return FALSE;

	packet = (PTSG_PACKET)calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	if (Stream_GetRemainingLength(pdu->s) < 12)
		goto fail;

	Stream_Seek_UINT32(pdu->s);                   /* PacketPtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packet->packetId); /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);      /* SwitchValue (4 bytes) */

	if ((packet->packetId == TSG_PACKET_TYPE_CAPS_RESPONSE) &&
	    (SwitchValue == TSG_PACKET_TYPE_CAPS_RESPONSE))
	{
		packetCapsResponse = (PTSG_PACKET_CAPS_RESPONSE)calloc(1, sizeof(TSG_PACKET_CAPS_RESPONSE));

		if (!packetCapsResponse)
			goto fail;

		packet->tsgPacket.packetCapsResponse = packetCapsResponse;

		if (Stream_GetRemainingLength(pdu->s) < 32)
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
			if (Stream_GetRemainingLength(pdu->s) < 16)
				goto fail;

			Stream_Seek_UINT32(pdu->s);                     /* MsgId (4 bytes) */
			Stream_Seek_UINT32(pdu->s);                     /* MsgType (4 bytes) */
			Stream_Read_UINT32(pdu->s, IsMessagePresent);   /* IsMessagePresent (4 bytes) */
			Stream_Read_UINT32(pdu->s, MessageSwitchValue); /* MessageSwitchValue (4 bytes) */
		}

		if (packetCapsResponse->pktQuarEncResponse.certChainLen > 0)
		{
			if (Stream_GetRemainingLength(pdu->s) < 16)
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
			if (Stream_GetRemainingLength(pdu->s) < 4)
				goto fail;

			Stream_Read_UINT32(pdu->s, Pointer); /* Ptr (4 bytes) */
		}

		versionCaps = (PTSG_PACKET_VERSIONCAPS)calloc(1, sizeof(TSG_PACKET_VERSIONCAPS));

		if (!versionCaps)
			goto fail;

		packetCapsResponse->pktQuarEncResponse.versionCaps = versionCaps;

		if (Stream_GetRemainingLength(pdu->s) < 18)
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

		if (Stream_GetRemainingLength(pdu->s) < 16)
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
				if (Stream_GetRemainingLength(pdu->s) < 16)
					goto fail;

				Stream_Seek_UINT32(pdu->s); /* IsDisplayMandatory (4 bytes) */
				Stream_Seek_UINT32(pdu->s); /* IsConsent Mandatory (4 bytes) */
				Stream_Read_UINT32(pdu->s, MsgBytes);
				Stream_Read_UINT32(pdu->s, Pointer);

				if (Pointer)
				{
					if (Stream_GetRemainingLength(pdu->s) < 12)
						goto fail;

					Stream_Seek_UINT32(pdu->s); /* MaxCount (4 bytes) */
					Stream_Seek_UINT32(pdu->s); /* Offset (4 bytes) */
					Stream_Seek_UINT32(pdu->s); /* Length (4 bytes) */
				}

				if (MsgBytes > TSG_MESSAGING_MAX_MESSAGE_LENGTH)
				{
					WLog_ERR(TAG, "Out of Spec Message Length %" PRIu32 "", MsgBytes);
					goto fail;
				}

				if (!Stream_SafeSeek(pdu->s, MsgBytes))
					goto fail;

				break;

			case TSG_ASYNC_MESSAGE_REAUTH:
			{
				if (!tsg_stream_align(pdu->s, 8))
					goto fail;

				if (Stream_GetRemainingLength(pdu->s) < 8)
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
		if (Stream_GetRemainingLength(pdu->s) < 24)
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

		if (Stream_GetRemainingLength(pdu->s) < 32)
			goto fail;

		Stream_Seek_UINT32(pdu->s); /* PacketQuarResponsePtr (4 bytes) */
		Stream_Read_UINT32(pdu->s, packetQuarEncResponse->flags); /* Flags (4 bytes) */
		Stream_Read_UINT32(pdu->s,
		                   packetQuarEncResponse->certChainLen); /* CertChainLength (4 bytes) */
		Stream_Seek_UINT32(pdu->s);                              /* CertChainDataPtr (4 bytes) */
		Stream_Read(pdu->s, &packetQuarEncResponse->nonce, 16);  /* Nonce (16 bytes) */

		if (packetQuarEncResponse->certChainLen > 0)
		{
			if (Stream_GetRemainingLength(pdu->s) < 16)
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
			if (Stream_GetRemainingLength(pdu->s) < 4)
				goto fail;

			Stream_Read_UINT32(pdu->s, Pointer); /* Ptr (4 bytes): 0x00020008 */
		}

		versionCaps = (PTSG_PACKET_VERSIONCAPS)calloc(1, sizeof(TSG_PACKET_VERSIONCAPS));

		if (!versionCaps)
			goto fail;

		packetQuarEncResponse->versionCaps = versionCaps;

		if (Stream_GetRemainingLength(pdu->s) < 18)
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
		Stream_Read_UINT16(pdu->s, versionCaps->majorVersion);    /* MinorVersion (2 bytes) */
		Stream_Read_UINT16(
		    pdu->s, versionCaps->quarantineCapabilities); /* QuarantineCapabilities (2 bytes) */

		/* 4-byte alignment */
		if (!tsg_stream_align(pdu->s, 4))
			goto fail;

		if (Stream_GetRemainingLength(pdu->s) < 36)
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
	UINT32 pad;
	wStream* s;
	size_t count;
	UINT32 offset;
	rdpRpc* rpc;

	if (!tsg || !tsg->rpc || !tunnelContext || !tsg->MachineName)
		return FALSE;

	count = _wcslen(tsg->MachineName) + 1;
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
	Stream_Write_UINT32(s, count);                       /* NameLength (4 bytes) */
	Stream_Write_UINT32(s, 0x00020008);                  /* DataPtr (4 bytes) */
	Stream_Write_UINT32(s, 0);                           /* DataLength (4 bytes) */
	/* MachineName */
	Stream_Write_UINT32(s, count);                         /* MaxCount (4 bytes) */
	Stream_Write_UINT32(s, 0);                             /* Offset (4 bytes) */
	Stream_Write_UINT32(s, count);                         /* ActualCount (4 bytes) */
	Stream_Write_UTF16_String(s, tsg->MachineName, count); /* Array */
	/* 4-byte alignment */
	offset = Stream_GetPosition(s);
	pad = rpc_offset_align(&offset, 4);
	Stream_Zero(s, pad);
	Stream_Write_UINT32(s, 0x00000000); /* MaxCount (4 bytes) */
	Stream_SealLength(s);
	return rpc_client_write_call(rpc, s, TsProxyAuthorizeTunnelOpnum);
}

static BOOL TsProxyAuthorizeTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BOOL rc = FALSE;
	UINT32 Pointer;
	UINT32 SizeValue;
	UINT32 SwitchValue;
	UINT32 idleTimeout;
	PTSG_PACKET packet = NULL;
	PTSG_PACKET_RESPONSE packetResponse = NULL;
	WLog_DBG(TAG, "TsProxyAuthorizeTunnelReadResponse");

	if (!pdu)
		return FALSE;

	packet = (PTSG_PACKET)calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	if (Stream_GetRemainingLength(pdu->s) < 68)
		goto fail;

	Stream_Seek_UINT32(pdu->s);                   /* PacketPtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packet->packetId); /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);      /* SwitchValue (4 bytes) */

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
	Stream_Read_UINT32(pdu->s, packetResponse->redirectionFlags
	                               .enableAllRedirections); /* EnableAllRedirections (4 bytes) */
	Stream_Read_UINT32(pdu->s, packetResponse->redirectionFlags
	                               .disableAllRedirections); /* DisableAllRedirections (4 bytes) */
	Stream_Read_UINT32(pdu->s,
	                   packetResponse->redirectionFlags
	                       .driveRedirectionDisabled); /* DriveRedirectionDisabled (4 bytes) */
	Stream_Read_UINT32(pdu->s,
	                   packetResponse->redirectionFlags
	                       .printerRedirectionDisabled); /* PrinterRedirectionDisabled (4 bytes) */
	Stream_Read_UINT32(pdu->s,
	                   packetResponse->redirectionFlags
	                       .portRedirectionDisabled); /* PortRedirectionDisabled (4 bytes) */
	Stream_Read_UINT32(pdu->s, packetResponse->redirectionFlags.reserved); /* Reserved (4 bytes) */
	Stream_Read_UINT32(
	    pdu->s, packetResponse->redirectionFlags
	                .clipboardRedirectionDisabled); /* ClipboardRedirectionDisabled (4 bytes) */
	Stream_Read_UINT32(pdu->s, packetResponse->redirectionFlags
	                               .pnpRedirectionDisabled); /* PnpRedirectionDisabled (4 bytes) */
	Stream_Read_UINT32(pdu->s, SizeValue);                   /* (4 bytes) */

	if (SizeValue != packetResponse->responseDataLen)
	{
		WLog_ERR(TAG, "Unexpected size value: %" PRIu32 ", expected: %" PRIu32 "", SizeValue,
		         packetResponse->responseDataLen);
		goto fail;
	}

	if (Stream_GetRemainingLength(pdu->s) < SizeValue)
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

	if (Stream_GetRemainingLength(s) < 32)
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

	if (Stream_GetRemainingLength(s) < msg->msgBytes)
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
	char* messageText = NULL;
	TSG_PACKET_MSG_RESPONSE packetMsgResponse = { 0 };
	TSG_PACKET_STRING_MESSAGE packetStringMessage = { 0 };
	TSG_PACKET_REAUTH_MESSAGE packetReauthMessage = { 0 };
	WLog_DBG(TAG, "TsProxyMakeTunnelCallReadResponse");

	/* This is an asynchronous response */

	if (!pdu)
		return FALSE;

	if (Stream_GetRemainingLength(pdu->s) < 28)
		goto fail;

	Stream_Seek_UINT32(pdu->s);                  /* PacketPtr (4 bytes) */
	Stream_Read_UINT32(pdu->s, packet.packetId); /* PacketId (4 bytes) */
	Stream_Read_UINT32(pdu->s, SwitchValue);     /* SwitchValue (4 bytes) */

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

			if (tsg->rpc && tsg->rpc->context && tsg->rpc->context->instance)
			{
				rc = IFCALLRESULT(TRUE, tsg->rpc->context->instance->PresentGatewayMessage,
				                  tsg->rpc->context->instance, SwitchValue,
				                  packetStringMessage.isDisplayMandatory != 0,
				                  packetStringMessage.isConsentMandatory != 0,
				                  packetStringMessage.msgBytes, packetStringMessage.msgBuffer);
			}

			break;

		case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
			if (!TsProxyReadPacketSTringMessage(tsg, pdu->s, &packetStringMessage))
				goto fail;

			ConvertFromUnicode(CP_UTF8, 0, packetStringMessage.msgBuffer,
			                   packetStringMessage.msgBytes / 2, &messageText, 0, NULL, NULL);

			WLog_INFO(TAG, "Service Message: %s", messageText);
			free(messageText);

			if (tsg->rpc && tsg->rpc->context && tsg->rpc->context->instance)
			{
				rc = IFCALLRESULT(TRUE, tsg->rpc->context->instance->PresentGatewayMessage,
				                  tsg->rpc->context->instance, SwitchValue,
				                  packetStringMessage.isDisplayMandatory != 0,
				                  packetStringMessage.isConsentMandatory != 0,
				                  packetStringMessage.msgBytes, packetStringMessage.msgBuffer);
			}
			break;

		case TSG_ASYNC_MESSAGE_REAUTH:
			if (Stream_GetRemainingLength(pdu->s) < 20)
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
	Stream_Write_UINT32(s, count);                      /* MaxCount (4 bytes) */
	Stream_Write_UINT32(s, 0);                          /* Offset (4 bytes) */
	Stream_Write_UINT32(s, count);                      /* ActualCount (4 bytes) */
	Stream_Write_UTF16_String(s, tsg->Hostname, count); /* Array */
	return rpc_client_write_call(rpc, s, TsProxyCreateChannelOpnum);
}

static BOOL TsProxyCreateChannelReadResponse(rdpTsg* tsg, RPC_PDU* pdu,
                                             CONTEXT_HANDLE* channelContext, UINT32* channelId)
{
	BOOL rc = FALSE;
	WLog_DBG(TAG, "TsProxyCreateChannelReadResponse");

	if (!pdu)
		return FALSE;

	if (Stream_GetRemainingLength(pdu->s) < 28)
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

static BOOL TsProxyCloseChannelReadResponse(rdpTsg* tsg, RPC_PDU* pdu, CONTEXT_HANDLE* context)
{
	BOOL rc = FALSE;
	WLog_DBG(TAG, "TsProxyCloseChannelReadResponse");

	if (!pdu)
		return FALSE;

	if (Stream_GetRemainingLength(pdu->s) < 24)
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

static BOOL TsProxyCloseTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu, CONTEXT_HANDLE* context)
{
	BOOL rc = FALSE;
	WLog_DBG(TAG, "TsProxyCloseTunnelReadResponse");

	if (!pdu || !context)
		return FALSE;

	if (Stream_GetRemainingLength(pdu->s) < 24)
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
	const char* str = "TSG_STATE_UNKNOWN";

	switch (state)
	{
		case TSG_STATE_INITIAL:
			str = "TSG_STATE_INITIAL";
			break;

		case TSG_STATE_CONNECTED:
			str = "TSG_STATE_CONNECTED";
			break;

		case TSG_STATE_AUTHORIZED:
			str = "TSG_STATE_AUTHORIZED";
			break;

		case TSG_STATE_CHANNEL_CREATED:
			str = "TSG_STATE_CHANNEL_CREATED";
			break;

		case TSG_STATE_PIPE_CREATED:
			str = "TSG_STATE_PIPE_CREATED";
			break;

		case TSG_STATE_TUNNEL_CLOSE_PENDING:
			str = "TSG_STATE_TUNNEL_CLOSE_PENDING";
			break;

		case TSG_STATE_CHANNEL_CLOSE_PENDING:
			str = "TSG_STATE_CHANNEL_CLOSE_PENDING";
			break;

		case TSG_STATE_FINAL:
			str = "TSG_STATE_FINAL";
			break;
	}

	WLog_DBG(TAG, "%s", str);
	return tsg_set_state(tsg, state);
}

BOOL tsg_proxy_begin(rdpTsg* tsg)
{
	TSG_PACKET tsgPacket;
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
	TSG_PACKET tsgPacket;
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
	Stream_SealLength(pdu->s);
	Stream_SetPosition(pdu->s, 0);

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

			if (!TsProxyAuthorizeTunnelReadResponse(tsg, pdu))
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

				if (!TsProxyCreateChannelReadResponse(tsg, pdu, &ChannelContext, &tsg->ChannelId))
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

				if (!TsProxyCloseChannelReadResponse(tsg, pdu, &ChannelContext))
				{
					WLog_ERR(TAG, "TsProxyCloseChannelReadResponse failure");
					return FALSE;
				}

				rc = TRUE;
			}
			else if (call->OpNum == TsProxyCloseTunnelOpnum)
			{
				CONTEXT_HANDLE TunnelContext;

				if (!TsProxyCloseTunnelReadResponse(tsg, pdu, &TunnelContext))
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

			if (!TsProxyCloseChannelReadResponse(tsg, pdu, &ChannelContext))
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

			if (!TsProxyCloseTunnelReadResponse(tsg, pdu, &TunnelContext))
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

BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port, int timeout)
{
	DWORD nCount;
	HANDLE events[64];
	rdpRpc* rpc = tsg->rpc;
	rdpSettings* settings = rpc->settings;
	rdpTransport* transport = rpc->transport;
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

	nCount = tsg_get_event_handles(tsg, events, 64);

	if (nCount == 0)
		return FALSE;

	while (tsg->state != TSG_STATE_PIPE_CREATED)
	{
		WaitForMultipleObjects(nCount, events, FALSE, 250);

		if (!tsg_check_event_handles(tsg))
		{
			WLog_ERR(TAG, "tsg_check failure");
			transport->layer = TRANSPORT_LAYER_CLOSED;
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

static int tsg_read(rdpTsg* tsg, BYTE* data, UINT32 length)
{
	rdpRpc* rpc;
	int status = 0;

	if (!tsg || !data)
		return -1;

	rpc = tsg->rpc;

	if (rpc->transport->layer == TRANSPORT_LAYER_CLOSED)
	{
		WLog_ERR(TAG, "tsg_read error: connection lost");
		return -1;
	}

	do
	{
		status = rpc_client_receive_pipe_read(rpc->client, data, (size_t)length);

		if (status < 0)
			return -1;

		if (!status && !rpc->transport->blocking)
			return 0;

		if (rpc->transport->layer == TRANSPORT_LAYER_CLOSED)
		{
			WLog_ERR(TAG, "tsg_read error: connection lost");
			return -1;
		}

		if (status > 0)
			break;

		if (rpc->transport->blocking)
		{
			while (WaitForSingleObject(rpc->client->PipeEvent, 0) != WAIT_OBJECT_0)
			{
				if (!tsg_check_event_handles(tsg))
					return -1;

				WaitForSingleObject(rpc->client->PipeEvent, 100);
			}
		}
	} while (rpc->transport->blocking);

	return status;
}

static int tsg_write(rdpTsg* tsg, const BYTE* data, UINT32 length)
{
	int status;

	if (!tsg || !data || !tsg->rpc || !tsg->rpc->transport)
		return -1;

	if (tsg->rpc->transport->layer == TRANSPORT_LAYER_CLOSED)
	{
		WLog_ERR(TAG, "error, connection lost");
		return -1;
	}

	status = TsProxySendToServer((handle_t)tsg, data, 1, &length);

	if (status < 0)
		return -1;

	return length;
}

rdpTsg* tsg_new(rdpTransport* transport)
{
	rdpTsg* tsg;
	tsg = (rdpTsg*)calloc(1, sizeof(rdpTsg));

	if (!tsg)
		return NULL;

	tsg->transport = transport;
	tsg->settings = transport->settings;
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
	status = tsg_write(tsg, (BYTE*)buf, num);

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
	status = tsg_read(tsg, (BYTE*)buf, size);

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
	return 1;
}

static int transport_bio_tsg_gets(BIO* bio, char* str, int size)
{
	return 1;
}

static long transport_bio_tsg_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	int status = -1;
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
			BIO* bio = outChannel->common.bio;
			status = BIO_read_blocked(bio);
		}
		break;

		case BIO_C_WRITE_BLOCKED:
		{
			BIO* bio = inChannel->common.bio;
			status = BIO_write_blocked(bio);
		}
		break;

		case BIO_C_WAIT_READ:
		{
			int timeout = (int)arg1;
			BIO* bio = outChannel->common.bio;

			if (BIO_read_blocked(bio))
				return BIO_wait_read(bio, timeout);
			else if (BIO_write_blocked(bio))
				return BIO_wait_write(bio, timeout);
			else
				status = 1;
		}
		break;

		case BIO_C_WAIT_WRITE:
		{
			int timeout = (int)arg1;
			BIO* bio = inChannel->common.bio;

			if (BIO_write_blocked(bio))
				status = BIO_wait_write(bio, timeout);
			else if (BIO_read_blocked(bio))
				status = BIO_wait_read(bio, timeout);
			else
				status = 1;
		}
		break;

		default:
			break;
	}

	return status;
}

static int transport_bio_tsg_new(BIO* bio)
{
	BIO_set_init(bio, 1);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	return 1;
}

static int transport_bio_tsg_free(BIO* bio)
{
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
