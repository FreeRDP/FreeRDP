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

static DWORD TsProxySendToServer(handle_t IDL_handle, byte pRpcMessage[], UINT32 count,
                                 UINT32* lengths)
{
	wStream* s;
	int status;
	rdpTsg* tsg;
	BYTE* buffer;
	UINT32 length;
	byte* buffer1 = NULL;
	byte* buffer2 = NULL;
	byte* buffer3 = NULL;
	UINT32 buffer1Length;
	UINT32 buffer2Length;
	UINT32 buffer3Length;
	UINT32 numBuffers = 0;
	UINT32 totalDataBytes = 0;
	tsg = (rdpTsg*) IDL_handle;
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
	buffer = (BYTE*) calloc(1, length);

	if (!buffer)
		return -1;

	s = Stream_New(buffer, length);

	if (!s)
	{
		free(buffer);
		WLog_ERR(TAG, "Stream_New failed!");
		return -1;
	}

	/* PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE_NR (20 bytes) */
	Stream_Write(s, &tsg->ChannelContext.ContextType, 4); /* ContextType (4 bytes) */
	Stream_Write(s, tsg->ChannelContext.ContextUuid, 16); /* ContextUuid (16 bytes) */
	Stream_Write_UINT32_BE(s, totalDataBytes); /* totalDataBytes (4 bytes) */
	Stream_Write_UINT32_BE(s, numBuffers); /* numBuffers (4 bytes) */

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

	Stream_SealLength(s);
	status = rpc_client_write_call(tsg->rpc, Stream_Buffer(s), Stream_Length(s),
	                               TsProxySendToServerOpnum);
	Stream_Free(s, TRUE);

	if (status <= 0)
	{
		WLog_ERR(TAG, "rpc_write failed!");
		return -1;
	}

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
	int status;
	UINT32 length;
	UINT32 offset = 0;
	BYTE* buffer = NULL;
	rdpRpc* rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxyCreateTunnelWriteRequest");

	if (tsgPacket->packetId == TSG_PACKET_TYPE_VERSIONCAPS)
	{
		PTSG_PACKET_VERSIONCAPS packetVersionCaps = tsgPacket->tsgPacket.packetVersionCaps;
		PTSG_CAPABILITY_NAP tsgCapNap = &packetVersionCaps->tsgCaps->tsgPacket.tsgCapNap;
		length = 108;
		buffer = (BYTE*) malloc(length);

		if (!buffer)
			return FALSE;

		*((UINT32*) &buffer[0]) = tsgPacket->packetId; /* PacketId (4 bytes) */
		*((UINT32*) &buffer[4]) = tsgPacket->packetId; /* SwitchValue (4 bytes) */
		*((UINT32*) &buffer[8]) = 0x00020000; /* PacketVersionCapsPtr (4 bytes) */
		*((UINT16*) &buffer[12]) = packetVersionCaps->tsgHeader.ComponentId; /* ComponentId (2 bytes) */
		*((UINT16*) &buffer[14]) = packetVersionCaps->tsgHeader.PacketId; /* PacketId (2 bytes) */
		*((UINT32*) &buffer[16]) = 0x00020004; /* TsgCapsPtr (4 bytes) */
		*((UINT32*) &buffer[20]) = packetVersionCaps->numCapabilities; /* NumCapabilities (4 bytes) */
		*((UINT16*) &buffer[24]) = packetVersionCaps->majorVersion; /* MajorVersion (2 bytes) */
		*((UINT16*) &buffer[26]) = packetVersionCaps->minorVersion; /* MinorVersion (2 bytes) */
		*((UINT16*) &buffer[28]) =
		    packetVersionCaps->quarantineCapabilities; /* QuarantineCapabilities (2 bytes) */
		/* 4-byte alignment (30 + 2) */
		*((UINT16*) &buffer[30]) = 0x0000; /* pad (2 bytes) */
		*((UINT32*) &buffer[32]) = packetVersionCaps->numCapabilities; /* MaxCount (4 bytes) */
		*((UINT32*) &buffer[36]) =
		    packetVersionCaps->tsgCaps->capabilityType; /* CapabilityType (4 bytes) */
		*((UINT32*) &buffer[40]) = packetVersionCaps->tsgCaps->capabilityType; /* SwitchValue (4 bytes) */
		*((UINT32*) &buffer[44]) = tsgCapNap->capabilities; /* capabilities (4 bytes) */
		offset = 48;
		/**
		 * The following 60-byte structure is apparently undocumented,
		 * but parts of it can be matched to known C706 data structures.
		 */
		/*
		 * 8-byte constant (8A E3 13 71 02 F4 36 71) also observed here:
		 * http://lists.samba.org/archive/cifs-protocol/2010-July/001543.html
		 */
		buffer[offset + 0] = 0x8A;
		buffer[offset + 1] = 0xE3;
		buffer[offset + 2] = 0x13;
		buffer[offset + 3] = 0x71;
		buffer[offset + 4] = 0x02;
		buffer[offset + 5] = 0xF4;
		buffer[offset + 6] = 0x36;
		buffer[offset + 7] = 0x71;
		*((UINT32*) &buffer[offset + 8]) = 0x00040001; /* 1.4 (version?) */
		*((UINT32*) &buffer[offset + 12]) = 0x00000001; /* 1 (element count?) */
		/* p_cont_list_t */
		buffer[offset + 16] = 2; /* ncontext_elem */
		buffer[offset + 17] = 0x40; /* reserved1 */
		*((UINT16*) &buffer[offset + 18]) = 0x0028; /* reserved2 */
		/* p_syntax_id_t */
		CopyMemory(&buffer[offset + 20], &TSGU_UUID, sizeof(p_uuid_t));
		*((UINT32*) &buffer[offset + 36]) = TSGU_SYNTAX_IF_VERSION;
		/* p_syntax_id_t */
		CopyMemory(&buffer[offset + 40], &NDR_UUID, sizeof(p_uuid_t));
		*((UINT32*) &buffer[offset + 56]) = NDR_SYNTAX_IF_VERSION;
		status = rpc_client_write_call(rpc, buffer, length, TsProxyCreateTunnelOpnum);
		free(buffer);

		if (status <= 0)
			return FALSE;
	}
	else if (tsgPacket->packetId == TSG_PACKET_TYPE_REAUTH)
	{
		PTSG_PACKET_REAUTH packetReauth = tsgPacket->tsgPacket.packetReauth;
		PTSG_PACKET_VERSIONCAPS packetVersionCaps = packetReauth->tsgInitialPacket.packetVersionCaps;
		PTSG_CAPABILITY_NAP tsgCapNap = &packetVersionCaps->tsgCaps->tsgPacket.tsgCapNap;
		length = 72;
		buffer = (BYTE*) malloc(length);

		if (!buffer)
			return FALSE;

		*((UINT32*) &buffer[0]) = tsgPacket->packetId; /* PacketId (4 bytes) */
		*((UINT32*) &buffer[4]) = tsgPacket->packetId; /* SwitchValue (4 bytes) */
		*((UINT32*) &buffer[8]) = 0x00020000; /* PacketReauthPtr (4 bytes) */
		*((UINT32*) &buffer[12]) = 0; /* ??? (4 bytes) */
		*((UINT64*) &buffer[16]) = packetReauth->tunnelContext; /* TunnelContext (8 bytes) */
		offset = 24;
		*((UINT32*) &buffer[offset + 0]) = TSG_PACKET_TYPE_VERSIONCAPS; /* PacketId (4 bytes) */
		*((UINT32*) &buffer[offset + 4]) = TSG_PACKET_TYPE_VERSIONCAPS; /* SwitchValue (4 bytes) */
		*((UINT32*) &buffer[offset + 8]) = 0x00020004; /* PacketVersionCapsPtr (4 bytes) */
		*((UINT16*) &buffer[offset + 12]) =
		    packetVersionCaps->tsgHeader.ComponentId; /* ComponentId (2 bytes) */
		*((UINT16*) &buffer[offset + 14]) = packetVersionCaps->tsgHeader.PacketId; /* PacketId (2 bytes) */
		*((UINT32*) &buffer[offset + 16]) = 0x00020008; /* TsgCapsPtr (4 bytes) */
		*((UINT32*) &buffer[offset + 20]) =
		    packetVersionCaps->numCapabilities; /* NumCapabilities (4 bytes) */
		*((UINT16*) &buffer[offset + 24]) = packetVersionCaps->majorVersion; /* MajorVersion (2 bytes) */
		*((UINT16*) &buffer[offset + 26]) = packetVersionCaps->minorVersion; /* MinorVersion (2 bytes) */
		*((UINT16*) &buffer[offset + 28]) =
		    packetVersionCaps->quarantineCapabilities; /* QuarantineCapabilities (2 bytes) */
		/* 4-byte alignment (30 + 2) */
		*((UINT16*) &buffer[offset + 30]) = 0x0000; /* pad (2 bytes) */
		*((UINT32*) &buffer[offset + 32]) = packetVersionCaps->numCapabilities; /* MaxCount (4 bytes) */
		*((UINT32*) &buffer[offset + 36]) =
		    packetVersionCaps->tsgCaps->capabilityType; /* CapabilityType (4 bytes) */
		*((UINT32*) &buffer[offset + 40]) =
		    packetVersionCaps->tsgCaps->capabilityType; /* SwitchValue (4 bytes) */
		*((UINT32*) &buffer[offset + 44]) = tsgCapNap->capabilities; /* capabilities (4 bytes) */
		status = rpc_client_write_call(rpc, buffer, length, TsProxyCreateTunnelOpnum);
		free(buffer);

		if (status <= 0)
			return FALSE;
	}

	return TRUE;
}

static BOOL TsProxyCreateTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu,
        CONTEXT_HANDLE* tunnelContext,
        UINT32* tunnelId)
{
	BYTE* buffer;
	UINT32 count;
	UINT32 length;
	UINT32 offset;
	UINT32 Pointer;
	PTSG_PACKET packet;
	UINT32 SwitchValue;
	UINT32 MessageSwitchValue = 0;
	UINT32 IsMessagePresent;
	UINT32 MsgBytes;
	PTSG_PACKET_CAPABILITIES tsgCaps;
	PTSG_PACKET_VERSIONCAPS versionCaps;
	PTSG_PACKET_CAPS_RESPONSE packetCapsResponse;
	PTSG_PACKET_QUARENC_RESPONSE packetQuarEncResponse;
	WLog_DBG(TAG, "TsProxyCreateTunnelReadResponse");

	if (!pdu)
		return FALSE;

	length = Stream_Length(pdu->s);
	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	packet = (PTSG_PACKET) calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	offset = 4; /* PacketPtr (4 bytes) */
	packet->packetId = *((UINT32*) &buffer[offset]); /* PacketId (4 bytes) */
	SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue (4 bytes) */

	if ((packet->packetId == TSG_PACKET_TYPE_CAPS_RESPONSE) &&
	    (SwitchValue == TSG_PACKET_TYPE_CAPS_RESPONSE))
	{
		packetCapsResponse = (PTSG_PACKET_CAPS_RESPONSE) calloc(1, sizeof(TSG_PACKET_CAPS_RESPONSE));

		if (!packetCapsResponse)
		{
			free(packet);
			return FALSE;
		}

		packet->tsgPacket.packetCapsResponse = packetCapsResponse;
		/* PacketQuarResponsePtr (4 bytes) */
		packetCapsResponse->pktQuarEncResponse.flags = *((UINT32*) &buffer[offset +
		               12]); /* Flags (4 bytes) */
		packetCapsResponse->pktQuarEncResponse.certChainLen = *((UINT32*) &buffer[offset +
		               16]); /* CertChainLength (4 bytes) */
		/* CertChainDataPtr (4 bytes) */
		CopyMemory(&packetCapsResponse->pktQuarEncResponse.nonce, &buffer[offset + 24],
		           16); /* Nonce (16 bytes) */
		offset += 40;
		Pointer = *((UINT32*) &buffer[offset]); /* VersionCapsPtr (4 bytes) */
		offset += 4;

		if ((Pointer == 0x0002000C) || (Pointer == 0x00020008))
		{
			offset += 4; /* MsgId (4 bytes) */
			offset += 4; /* MsgType (4 bytes) */
			IsMessagePresent = *((UINT32*) &buffer[offset]); /* IsMessagePresent (4 bytes) */
			offset += 4;
			MessageSwitchValue = *((UINT32*) &buffer[offset]); /* MessageSwitchValue (4 bytes) */
			offset += 4;
		}

		if (packetCapsResponse->pktQuarEncResponse.certChainLen > 0)
		{
			Pointer = *((UINT32*) &buffer[offset]); /* MsgPtr (4 bytes): 0x00020014 */
			offset += 4;
			offset += 4; /* MaxCount (4 bytes) */
			offset += 4; /* Offset (4 bytes) */
			count = *((UINT32*) &buffer[offset]); /* ActualCount (4 bytes) */
			offset += 4;
			/*
			 * CertChainData is a wide character string, and the count is
			 * given in characters excluding the null terminator, therefore:
			 * size = (count * 2)
			 */
			offset += (count * 2); /* CertChainData */
			/* 4-byte alignment */
			rpc_offset_align(&offset, 4);
		}
		else
		{
			Pointer = *((UINT32*) &buffer[offset]); /* Ptr (4 bytes) */
			offset += 4;
		}

		versionCaps = (PTSG_PACKET_VERSIONCAPS) calloc(1, sizeof(TSG_PACKET_VERSIONCAPS));

		if (!versionCaps)
		{
			free(packetCapsResponse);
			free(packet);
			return FALSE;
		}

		packetCapsResponse->pktQuarEncResponse.versionCaps = versionCaps;
		versionCaps->tsgHeader.ComponentId = *((UINT16*) &buffer[offset]); /* ComponentId (2 bytes) */
		versionCaps->tsgHeader.PacketId = *((UINT16*) &buffer[offset + 2]); /* PacketId (2 bytes) */
		offset += 4;

		if (versionCaps->tsgHeader.ComponentId != TS_GATEWAY_TRANSPORT)
		{
			WLog_ERR(TAG, "Unexpected ComponentId: 0x%04"PRIX16", Expected TS_GATEWAY_TRANSPORT",
			         versionCaps->tsgHeader.ComponentId);
			free(packetCapsResponse);
			free(versionCaps);
			free(packet);
			return FALSE;
		}

		Pointer = *((UINT32*) &buffer[offset]); /* TsgCapsPtr (4 bytes) */
		versionCaps->numCapabilities = *((UINT32*) &buffer[offset + 4]); /* NumCapabilities (4 bytes) */
		versionCaps->majorVersion = *((UINT16*) &buffer[offset + 8]); /* MajorVersion (2 bytes) */
		versionCaps->minorVersion = *((UINT16*) &buffer[offset + 10]); /* MinorVersion (2 bytes) */
		versionCaps->quarantineCapabilities = *((UINT16*) &buffer[offset +
		                                               12]); /* QuarantineCapabilities (2 bytes) */
		offset += 14;
		/* 4-byte alignment */
		rpc_offset_align(&offset, 4);
		tsgCaps = (PTSG_PACKET_CAPABILITIES) calloc(1, sizeof(TSG_PACKET_CAPABILITIES));

		if (!tsgCaps)
		{
			free(packetCapsResponse);
			free(versionCaps);
			free(packet);
			return FALSE;
		}

		versionCaps->tsgCaps = tsgCaps;
		offset += 4; /* MaxCount (4 bytes) */
		tsgCaps->capabilityType = *((UINT32*) &buffer[offset]); /* CapabilityType (4 bytes) */
		SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue (4 bytes) */
		offset += 8;

		if ((SwitchValue != TSG_CAPABILITY_TYPE_NAP) ||
		    (tsgCaps->capabilityType != TSG_CAPABILITY_TYPE_NAP))
		{
			WLog_ERR(TAG, "Unexpected CapabilityType: 0x%08"PRIX32", Expected TSG_CAPABILITY_TYPE_NAP",
			         tsgCaps->capabilityType);
			free(tsgCaps);
			free(versionCaps);
			free(packetCapsResponse);
			free(packet);
			return FALSE;
		}

		tsgCaps->tsgPacket.tsgCapNap.capabilities = *((UINT32*)
		        &buffer[offset]); /* Capabilities (4 bytes) */
		offset += 4;

		switch (MessageSwitchValue)
		{
			case TSG_ASYNC_MESSAGE_CONSENT_MESSAGE:
			case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
				offset += 4; /* IsDisplayMandatory (4 bytes) */
				offset += 4; /* IsConsent Mandatory (4 bytes) */
				MsgBytes = *((UINT32*) &buffer[offset]);
				offset += 4;
				Pointer = *((UINT32*) &buffer[offset]);
				offset += 4;

				if (Pointer)
				{
					offset += 4; /* MaxCount (4 bytes) */
					offset += 4; /* Offset (4 bytes) */
					offset += 4; /* Length (4 bytes) */
				}

				if (MsgBytes > TSG_MESSAGING_MAX_MESSAGE_LENGTH)
				{
					WLog_ERR(TAG, "Out of Spec Message Length %"PRIu32"", MsgBytes);
					free(tsgCaps);
					free(versionCaps);
					free(packetCapsResponse);
					free(packet);
					return FALSE;
				}

				offset += MsgBytes;
				break;

			case TSG_ASYNC_MESSAGE_REAUTH:
				rpc_offset_align(&offset, 8);
				offset += 8; /* TunnelContext (8 bytes) */
				break;

			default:
				WLog_ERR(TAG, "Unexpected Message Type: 0x%"PRIX32"", MessageSwitchValue);
				free(tsgCaps);
				free(versionCaps);
				free(packetCapsResponse);
				free(packet);
				return FALSE;
		}

		rpc_offset_align(&offset, 4);
		/* TunnelContext (20 bytes) */
		CopyMemory(&tunnelContext->ContextType, &buffer[offset], 4); /* ContextType (4 bytes) */
		CopyMemory(&tunnelContext->ContextUuid, &buffer[offset + 4], 16); /* ContextUuid (16 bytes) */
		offset += 20;
		*tunnelId = *((UINT32*) &buffer[offset]); /* TunnelId (4 bytes) */
		/* ReturnValue (4 bytes) */
		free(tsgCaps);
		free(versionCaps);
		free(packetCapsResponse);
	}
	else if ((packet->packetId == TSG_PACKET_TYPE_QUARENC_RESPONSE) &&
	         (SwitchValue == TSG_PACKET_TYPE_QUARENC_RESPONSE))
	{
		packetQuarEncResponse = (PTSG_PACKET_QUARENC_RESPONSE) calloc(1,
		                        sizeof(TSG_PACKET_QUARENC_RESPONSE));

		if (!packetQuarEncResponse)
		{
			free(packet);
			return FALSE;
		}

		packet->tsgPacket.packetQuarEncResponse = packetQuarEncResponse;
		/* PacketQuarResponsePtr (4 bytes) */
		packetQuarEncResponse->flags = *((UINT32*) &buffer[offset + 12]); /* Flags (4 bytes) */
		packetQuarEncResponse->certChainLen = *((UINT32*) &buffer[offset +
		                                               16]); /* CertChainLength (4 bytes) */
		/* CertChainDataPtr (4 bytes) */
		CopyMemory(&packetQuarEncResponse->nonce, &buffer[offset + 24], 16); /* Nonce (16 bytes) */
		offset += 40;

		if (packetQuarEncResponse->certChainLen > 0)
		{
			Pointer = *((UINT32*) &buffer[offset]); /* Ptr (4 bytes): 0x0002000C */
			offset += 4;
			offset += 4; /* MaxCount (4 bytes) */
			offset += 4; /* Offset (4 bytes) */
			count = *((UINT32*) &buffer[offset]); /* ActualCount (4 bytes) */
			offset += 4;
			/*
			 * CertChainData is a wide character string, and the count is
			 * given in characters excluding the null terminator, therefore:
			 * size = (count * 2)
			 */
			offset += (count * 2); /* CertChainData */
			/* 4-byte alignment */
			rpc_offset_align(&offset, 4);
		}
		else
		{
			Pointer = *((UINT32*) &buffer[offset]); /* Ptr (4 bytes): 0x00020008 */
			offset += 4;
		}

		versionCaps = (PTSG_PACKET_VERSIONCAPS) calloc(1, sizeof(TSG_PACKET_VERSIONCAPS));

		if (!versionCaps)
		{
			free(packetQuarEncResponse);
			free(packet);
			return FALSE;
		}

		packetQuarEncResponse->versionCaps = versionCaps;
		versionCaps->tsgHeader.ComponentId = *((UINT16*) &buffer[offset]); /* ComponentId (2 bytes) */
		versionCaps->tsgHeader.PacketId = *((UINT16*) &buffer[offset + 2]); /* PacketId (2 bytes) */
		offset += 4;

		if (versionCaps->tsgHeader.ComponentId != TS_GATEWAY_TRANSPORT)
		{
			WLog_ERR(TAG, "Unexpected ComponentId: 0x%04"PRIX16", Expected TS_GATEWAY_TRANSPORT",
			         versionCaps->tsgHeader.ComponentId);
			free(versionCaps);
			free(packetQuarEncResponse);
			free(packet);
			return FALSE;
		}

		Pointer = *((UINT32*) &buffer[offset]); /* TsgCapsPtr (4 bytes) */
		versionCaps->numCapabilities = *((UINT32*) &buffer[offset + 4]); /* NumCapabilities (4 bytes) */
		versionCaps->majorVersion = *((UINT16*) &buffer[offset + 8]); /* MajorVersion (2 bytes) */
		versionCaps->majorVersion = *((UINT16*) &buffer[offset + 10]); /* MinorVersion (2 bytes) */
		versionCaps->quarantineCapabilities = *((UINT16*) &buffer[offset +
		                                               12]); /* QuarantineCapabilities (2 bytes) */
		offset += 14;
		/* 4-byte alignment */
		rpc_offset_align(&offset, 4);
		/* Not sure exactly what this is */
		offset += 4; /* 0x00000001 (4 bytes) */
		offset += 4; /* 0x00000001 (4 bytes) */
		offset += 4; /* 0x00000001 (4 bytes) */
		offset += 4; /* 0x00000002 (4 bytes) */
		/* TunnelContext (20 bytes) */
		CopyMemory(&tunnelContext->ContextType, &buffer[offset], 4); /* ContextType (4 bytes) */
		CopyMemory(&tunnelContext->ContextUuid, &buffer[offset + 4], 16); /* ContextUuid (16 bytes) */
		offset += 20;
		free(versionCaps);
		free(packetQuarEncResponse);
	}
	else
	{
		WLog_ERR(TAG, "Unexpected PacketId: 0x%08"PRIX32", Expected TSG_PACKET_TYPE_CAPS_RESPONSE "
		         "or TSG_PACKET_TYPE_QUARENC_RESPONSE", packet->packetId);
		free(packet);
		return FALSE;
	}

	free(packet);
	return TRUE;
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
	int status;
	BYTE* buffer;
	UINT32 count;
	UINT32 length;
	UINT32 offset;
	rdpRpc* rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxyAuthorizeTunnelWriteRequest");
	count = _wcslen(tsg->MachineName) + 1;
	offset = 64 + (count * 2);
	rpc_offset_align(&offset, 4);
	offset += 4;
	length = offset;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext (20 bytes) */
	CopyMemory(&buffer[0], &tunnelContext->ContextType, 4); /* ContextType (4 bytes) */
	CopyMemory(&buffer[4], &tunnelContext->ContextUuid, 16); /* ContextUuid (16 bytes) */
	/* 4-byte alignment */
	*((UINT32*) &buffer[20]) = TSG_PACKET_TYPE_QUARREQUEST; /* PacketId (4 bytes) */
	*((UINT32*) &buffer[24]) = TSG_PACKET_TYPE_QUARREQUEST; /* SwitchValue (4 bytes) */
	*((UINT32*) &buffer[28]) = 0x00020000; /* PacketQuarRequestPtr (4 bytes) */
	*((UINT32*) &buffer[32]) = 0x00000000; /* Flags (4 bytes) */
	*((UINT32*) &buffer[36]) = 0x00020004; /* MachineNamePtr (4 bytes) */
	*((UINT32*) &buffer[40]) = count; /* NameLength (4 bytes) */
	*((UINT32*) &buffer[44]) = 0x00020008; /* DataPtr (4 bytes) */
	*((UINT32*) &buffer[48]) = 0; /* DataLength (4 bytes) */
	/* MachineName */
	*((UINT32*) &buffer[52]) = count; /* MaxCount (4 bytes) */
	*((UINT32*) &buffer[56]) = 0; /* Offset (4 bytes) */
	*((UINT32*) &buffer[60]) = count; /* ActualCount (4 bytes) */
	CopyMemory(&buffer[64], tsg->MachineName, count * 2); /* Array */
	offset = 64 + (count * 2);
	/* 4-byte alignment */
	pad = rpc_offset_align(&offset, 4);
	ZeroMemory(&buffer[offset - pad], pad);
	*((UINT32*) &buffer[offset]) = 0x00000000; /* MaxCount (4 bytes) */
	offset += 4;
	status = rpc_client_write_call(rpc, buffer, length, TsProxyAuthorizeTunnelOpnum);
	free(buffer);

	if (status <= 0)
		return FALSE;

	return TRUE;
}

static BOOL TsProxyAuthorizeTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;
	UINT32 Pointer;
	UINT32 SizeValue;
	UINT32 SwitchValue;
	UINT32 idleTimeout;
	PTSG_PACKET packet;
	PTSG_PACKET_RESPONSE packetResponse;
	WLog_DBG(TAG, "TsProxyAuthorizeTunnelReadResponse");

	if (!pdu)
		return FALSE;

	length = Stream_Length(pdu->s);
	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	packet = (PTSG_PACKET) calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	offset = 4; /* PacketPtr (4 bytes) */
	packet->packetId = *((UINT32*) &buffer[offset]); /* PacketId (4 bytes) */
	SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue (4 bytes) */

	if (packet->packetId == E_PROXY_NAP_ACCESSDENIED)
	{
		WLog_ERR(TAG, "status: E_PROXY_NAP_ACCESSDENIED (0x%08X)", E_PROXY_NAP_ACCESSDENIED);
		WLog_ERR(TAG, "Ensure that the Gateway Connection Authorization Policy is correct");
		free(packet);
		return FALSE;
	}

	if ((packet->packetId != TSG_PACKET_TYPE_RESPONSE) || (SwitchValue != TSG_PACKET_TYPE_RESPONSE))
	{
		WLog_ERR(TAG, "Unexpected PacketId: 0x%08"PRIX32", Expected TSG_PACKET_TYPE_RESPONSE",
		         packet->packetId);
		free(packet);
		return FALSE;
	}

	packetResponse = (PTSG_PACKET_RESPONSE) calloc(1, sizeof(TSG_PACKET_RESPONSE));

	if (!packetResponse)
	{
		free(packet);
		return FALSE;
	}

	packet->tsgPacket.packetResponse = packetResponse;
	Pointer = *((UINT32*) &buffer[offset + 8]); /* PacketResponsePtr (4 bytes) */
	packetResponse->flags = *((UINT32*) &buffer[offset + 12]); /* Flags (4 bytes) */

	if (packetResponse->flags != TSG_PACKET_TYPE_QUARREQUEST)
	{
		WLog_ERR(TAG,
		         "Unexpected Packet Response Flags: 0x%08"PRIX32", Expected TSG_PACKET_TYPE_QUARREQUEST",
		         packetResponse->flags);
		free(packet);
		free(packetResponse);
		return FALSE;
	}

	/* Reserved (4 bytes) */
	Pointer = *((UINT32*) &buffer[offset + 20]); /* ResponseDataPtr (4 bytes) */
	packetResponse->responseDataLen = *((UINT32*) &buffer[offset +
	                                           24]); /* ResponseDataLength (4 bytes) */
	packetResponse->redirectionFlags.enableAllRedirections = *((UINT32*) &buffer[offset +
	               28]); /* EnableAllRedirections (4 bytes) */
	packetResponse->redirectionFlags.disableAllRedirections = *((UINT32*) &buffer[offset +
	               32]); /* DisableAllRedirections (4 bytes) */
	packetResponse->redirectionFlags.driveRedirectionDisabled = *((UINT32*) &buffer[offset +
	               36]); /* DriveRedirectionDisabled (4 bytes) */
	packetResponse->redirectionFlags.printerRedirectionDisabled = *((UINT32*) &buffer[offset +
	               40]); /* PrinterRedirectionDisabled (4 bytes) */
	packetResponse->redirectionFlags.portRedirectionDisabled = *((UINT32*) &buffer[offset +
	               44]); /* PortRedirectionDisabled (4 bytes) */
	packetResponse->redirectionFlags.reserved = *((UINT32*) &buffer[offset +
	               48]); /* Reserved (4 bytes) */
	packetResponse->redirectionFlags.clipboardRedirectionDisabled = *((UINT32*) &buffer[offset +
	               52]); /* ClipboardRedirectionDisabled (4 bytes) */
	packetResponse->redirectionFlags.pnpRedirectionDisabled = *((UINT32*) &buffer[offset +
	               56]); /* PnpRedirectionDisabled (4 bytes) */
	offset += 60;
	SizeValue = *((UINT32*) &buffer[offset]); /* (4 bytes) */
	offset += 4;

	if (SizeValue != packetResponse->responseDataLen)
	{
		WLog_ERR(TAG, "Unexpected size value: %"PRIu32", expected: %"PRIu32"",
		         SizeValue, packetResponse->responseDataLen);
		free(packetResponse);
		free(packet);
		return FALSE;
	}

	if (SizeValue == 4)
	{
		idleTimeout = *((UINT32*) &buffer[offset]);
		offset += 4;
	}
	else
	{
		offset += SizeValue; /* ResponseData */
	}

	free(packetResponse);
	free(packet);
	return TRUE;
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
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxyMakeTunnelCallWriteRequest");
	length = 40;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext (20 bytes) */
	CopyMemory(&buffer[0], &tunnelContext->ContextType, 4); /* ContextType (4 bytes) */
	CopyMemory(&buffer[4], &tunnelContext->ContextUuid, 16); /* ContextUuid (16 bytes) */
	*((UINT32*) &buffer[20]) = procId; /* ProcId (4 bytes) */
	/* 4-byte alignment */
	*((UINT32*) &buffer[24]) = TSG_PACKET_TYPE_MSGREQUEST_PACKET; /* PacketId (4 bytes) */
	*((UINT32*) &buffer[28]) = TSG_PACKET_TYPE_MSGREQUEST_PACKET; /* SwitchValue (4 bytes) */
	*((UINT32*) &buffer[32]) = 0x00020000; /* PacketMsgRequestPtr (4 bytes) */
	*((UINT32*) &buffer[36]) = 0x00000001; /* MaxMessagesPerBatch (4 bytes) */
	status = rpc_client_write_call(rpc, buffer, length, TsProxyMakeTunnelCallOpnum);
	free(buffer);

	if (status <= 0)
		return FALSE;

	return TRUE;
}

static BOOL TsProxyMakeTunnelCallReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;
	UINT32 Pointer;
	UINT32 MaxCount;
	UINT32 ActualCount;
	UINT32 SwitchValue;
	PTSG_PACKET packet;
	BOOL status = TRUE;
	char* messageText = NULL;
	PTSG_PACKET_MSG_RESPONSE packetMsgResponse = NULL;
	PTSG_PACKET_STRING_MESSAGE packetStringMessage = NULL;
	PTSG_PACKET_REAUTH_MESSAGE packetReauthMessage = NULL;
	WLog_DBG(TAG, "TsProxyMakeTunnelCallReadResponse");

	/* This is an asynchronous response */

	if (!pdu)
		return FALSE;

	length = Stream_Length(pdu->s);
	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	packet = (PTSG_PACKET) calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	offset = 4; /* PacketPtr (4 bytes) */
	packet->packetId = *((UINT32*) &buffer[offset]); /* PacketId (4 bytes) */
	SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue (4 bytes) */

	if ((packet->packetId != TSG_PACKET_TYPE_MESSAGE_PACKET) ||
	    (SwitchValue != TSG_PACKET_TYPE_MESSAGE_PACKET))
	{
		WLog_ERR(TAG, "Unexpected PacketId: 0x%08"PRIX32", Expected TSG_PACKET_TYPE_MESSAGE_PACKET",
		         packet->packetId);
		free(packet);
		return FALSE;
	}

	packetMsgResponse = (PTSG_PACKET_MSG_RESPONSE) calloc(1, sizeof(TSG_PACKET_MSG_RESPONSE));

	if (!packetMsgResponse)
	{
		free(packet);
		return FALSE;
	}

	packet->tsgPacket.packetMsgResponse = packetMsgResponse;
	Pointer = *((UINT32*) &buffer[offset + 8]); /* PacketMsgResponsePtr (4 bytes) */
	packetMsgResponse->msgID = *((UINT32*) &buffer[offset + 12]); /* MsgId (4 bytes) */
	packetMsgResponse->msgType = *((UINT32*) &buffer[offset + 16]); /* MsgType (4 bytes) */
	packetMsgResponse->isMsgPresent = *((INT32*) &buffer[offset + 20]); /* IsMsgPresent (4 bytes) */
	SwitchValue = *((UINT32*) &buffer[offset + 24]); /* SwitchValue (4 bytes) */

	switch (SwitchValue)
	{
		case TSG_ASYNC_MESSAGE_CONSENT_MESSAGE:
			packetStringMessage = (PTSG_PACKET_STRING_MESSAGE) calloc(1, sizeof(TSG_PACKET_STRING_MESSAGE));

			if (!packetStringMessage)
			{
				status = FALSE;
				goto out;
			}

			packetMsgResponse->messagePacket.consentMessage = packetStringMessage;
			Pointer = *((UINT32*) &buffer[offset + 28]); /* ConsentMessagePtr (4 bytes) */
			packetStringMessage->isDisplayMandatory = *((INT32*) &buffer[offset +
			               32]); /* IsDisplayMandatory (4 bytes) */
			packetStringMessage->isConsentMandatory = *((INT32*) &buffer[offset +
			               36]); /* IsConsentMandatory (4 bytes) */
			packetStringMessage->msgBytes = *((UINT32*) &buffer[offset + 40]); /* MsgBytes (4 bytes) */
			Pointer = *((UINT32*) &buffer[offset + 44]); /* MsgPtr (4 bytes) */
			MaxCount = *((UINT32*) &buffer[offset + 48]); /* MaxCount (4 bytes) */
			/* Offset (4 bytes) */
			ActualCount = *((UINT32*) &buffer[offset + 56]); /* ActualCount (4 bytes) */
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) &buffer[offset + 60], ActualCount, &messageText, 0, NULL,
			                   NULL);
			WLog_INFO(TAG, "Consent Message: %s", messageText);
			free(messageText);
			break;

		case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
			packetStringMessage = (PTSG_PACKET_STRING_MESSAGE) calloc(1, sizeof(TSG_PACKET_STRING_MESSAGE));

			if (!packetStringMessage)
			{
				status = FALSE;
				goto out;
			}

			packetMsgResponse->messagePacket.serviceMessage = packetStringMessage;
			Pointer = *((UINT32*) &buffer[offset + 28]); /* ServiceMessagePtr (4 bytes) */
			packetStringMessage->isDisplayMandatory = *((INT32*) &buffer[offset +
			               32]); /* IsDisplayMandatory (4 bytes) */
			packetStringMessage->isConsentMandatory = *((INT32*) &buffer[offset +
			               36]); /* IsConsentMandatory (4 bytes) */
			packetStringMessage->msgBytes = *((UINT32*) &buffer[offset + 40]); /* MsgBytes (4 bytes) */
			Pointer = *((UINT32*) &buffer[offset + 44]); /* MsgPtr (4 bytes) */
			MaxCount = *((UINT32*) &buffer[offset + 48]); /* MaxCount (4 bytes) */
			/* Offset (4 bytes) */
			ActualCount = *((UINT32*) &buffer[offset + 56]); /* ActualCount (4 bytes) */
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) &buffer[offset + 60], ActualCount, &messageText, 0, NULL,
			                   NULL);
			WLog_INFO(TAG, "Service Message: %s", messageText);
			free(messageText);
			break;

		case TSG_ASYNC_MESSAGE_REAUTH:
			packetReauthMessage = (PTSG_PACKET_REAUTH_MESSAGE) calloc(1, sizeof(TSG_PACKET_REAUTH_MESSAGE));

			if (!packetReauthMessage)
			{
				status = FALSE;
				goto out;
			}

			packetMsgResponse->messagePacket.reauthMessage = packetReauthMessage;
			Pointer = *((UINT32*) &buffer[offset + 28]); /* ReauthMessagePtr (4 bytes) */
			/* alignment pad (4 bytes) */
			packetReauthMessage->tunnelContext = *((UINT64*) &buffer[offset +
			                                              36]); /* TunnelContext (8 bytes) */
			/* ReturnValue (4 bytes) */
			tsg->ReauthTunnelContext = packetReauthMessage->tunnelContext;
			break;

		default:
			WLog_ERR(TAG, "unexpected message type: %"PRIu32"", SwitchValue);
			status = FALSE;
			break;
	}

out:

	if (packet)
	{
		if (packet->tsgPacket.packetMsgResponse)
		{
			free(packet->tsgPacket.packetMsgResponse->messagePacket.reauthMessage);
			free(packet->tsgPacket.packetMsgResponse);
		}

		free(packet);
	}

	return status;
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
	int status;
	UINT32 count;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;
	count = _wcslen(tsg->Hostname) + 1;
	WLog_DBG(TAG, "TsProxyCreateChannelWriteRequest");
	length = 60 + (count * 2);
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext (20 bytes) */
	CopyMemory(&buffer[0], &tunnelContext->ContextType, 4); /* ContextType (4 bytes) */
	CopyMemory(&buffer[4], &tunnelContext->ContextUuid, 16); /* ContextUuid (16 bytes) */
	/* TSENDPOINTINFO */
	*((UINT32*) &buffer[20]) = 0x00020000; /* ResourceNamePtr (4 bytes) */
	*((UINT32*) &buffer[24]) = 0x00000001; /* NumResourceNames (4 bytes) */
	*((UINT32*) &buffer[28]) = 0x00000000; /* AlternateResourceNamesPtr (4 bytes) */
	*((UINT16*) &buffer[32]) = 0x0000; /* NumAlternateResourceNames (2 bytes) */
	*((UINT16*) &buffer[34]) = 0x0000; /* Pad (2 bytes) */
	/* Port (4 bytes) */
	*((UINT16*) &buffer[36]) = 0x0003; /* ProtocolId (RDP = 3) (2 bytes) */
	*((UINT16*) &buffer[38]) = tsg->Port; /* PortNumber (0xD3D = 3389) (2 bytes) */
	*((UINT32*) &buffer[40]) = 0x00000001; /* NumResourceNames (4 bytes) */
	*((UINT32*) &buffer[44]) = 0x00020004; /* ResourceNamePtr (4 bytes) */
	*((UINT32*) &buffer[48]) = count; /* MaxCount (4 bytes) */
	*((UINT32*) &buffer[52]) = 0; /* Offset (4 bytes) */
	*((UINT32*) &buffer[56]) = count; /* ActualCount (4 bytes) */
	CopyMemory(&buffer[60], tsg->Hostname, count * 2); /* Array */
	status = rpc_client_write_call(rpc, buffer, length, TsProxyCreateChannelOpnum);
	free(buffer);

	if (status <= 0)
		return FALSE;

	return TRUE;
}

static BOOL TsProxyCreateChannelReadResponse(rdpTsg* tsg, RPC_PDU* pdu,
        CONTEXT_HANDLE* channelContext,
        UINT32* channelId)
{
	BYTE* buffer;
	UINT32 offset;
	WLog_DBG(TAG, "TsProxyCreateChannelReadResponse");

	if (!pdu)
		return FALSE;

	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	offset = 0;
	/* ChannelContext (20 bytes) */
	CopyMemory(&channelContext->ContextType, &buffer[offset], 4); /* ContextType (4 bytes) */
	CopyMemory(&channelContext->ContextUuid, &buffer[offset + 4], 16); /* ContextUuid (16 bytes) */
	offset += 20;
	*channelId = *((UINT32*) &buffer[offset]); /* ChannelId (4 bytes) */
	/* ReturnValue (4 bytes) */
	return TRUE;
}

/**
 * HRESULT TsProxyCloseChannel(
 * [in, out] PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE* context
 * );
 */

static BOOL TsProxyCloseChannelWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* context)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxyCloseChannelWriteRequest");

	if (!context)
		return FALSE;

	length = 20;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* ChannelContext (20 bytes) */
	CopyMemory(&buffer[0], &context->ContextType, 4); /* ContextType (4 bytes) */
	CopyMemory(&buffer[4], &context->ContextUuid, 16); /* ContextUuid (16 bytes) */
	status = rpc_client_write_call(rpc, buffer, length, TsProxyCloseChannelOpnum);
	free(buffer);

	if (status <= 0)
		return FALSE;

	return TRUE;
}

static BOOL TsProxyCloseChannelReadResponse(rdpTsg* tsg, RPC_PDU* pdu, CONTEXT_HANDLE* context)
{
	BYTE* buffer;
	WLog_DBG(TAG, "TsProxyCloseChannelReadResponse");

	if (!pdu)
		return FALSE;

	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	/* ChannelContext (20 bytes) */
	CopyMemory(&context->ContextType, &buffer[0], 4); /* ContextType (4 bytes) */
	CopyMemory(&context->ContextUuid, &buffer[4], 16); /* ContextUuid (16 bytes) */
	/* ReturnValue (4 bytes) */
	return TRUE;
}

/**
 * HRESULT TsProxyCloseTunnel(
 * [in, out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* context
 * );
 */

static BOOL TsProxyCloseTunnelWriteRequest(rdpTsg* tsg, CONTEXT_HANDLE* context)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxyCloseTunnelWriteRequest");
	length = 20;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext (20 bytes) */
	CopyMemory(&buffer[0], &context->ContextType, 4); /* ContextType (4 bytes) */
	CopyMemory(&buffer[4], &context->ContextUuid, 16); /* ContextUuid (16 bytes) */
	status = rpc_client_write_call(rpc, buffer, length, TsProxyCloseTunnelOpnum);
	free(buffer);

	if (status <= 0)
		return FALSE;

	return TRUE;
}

static BOOL TsProxyCloseTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu, CONTEXT_HANDLE* context)
{
	BYTE* buffer;
	WLog_DBG(TAG, "TsProxyCloseTunnelReadResponse");

	if (!pdu)
		return FALSE;

	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	/* TunnelContext (20 bytes) */
	CopyMemory(&context->ContextType, &buffer[0], 4); /* ContextType (4 bytes) */
	CopyMemory(&context->ContextUuid, &buffer[4], 16); /* ContextUuid (16 bytes) */
	/* ReturnValue (4 bytes) */
	return TRUE;
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
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;
	WLog_DBG(TAG, "TsProxySetupReceivePipeWriteRequest");
	length = 20;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* ChannelContext (20 bytes) */
	CopyMemory(&buffer[0], &channelContext->ContextType, 4); /* ContextType (4 bytes) */
	CopyMemory(&buffer[4], &channelContext->ContextUuid, 16); /* ContextUuid (16 bytes) */
	status = rpc_client_write_call(rpc, buffer, length, TsProxySetupReceivePipeOpnum);
	free(buffer);

	if (status <= 0)
		return FALSE;

	return TRUE;
}


static int tsg_transition_to_state(rdpTsg* tsg, TSG_STATE state)
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

	tsg->state = state;
	WLog_DBG(TAG, "%s", str);
	return 1;
}

int tsg_proxy_begin(rdpTsg* tsg)
{
	TSG_PACKET tsgPacket;
	PTSG_CAPABILITY_NAP tsgCapNap;
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;
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
	 * "Only allow connections from Remote Desktop Services clients that support RD Gateway messaging"
	 */
	tsgCapNap->capabilities =
	    TSG_NAP_CAPABILITY_QUAR_SOH |
	    TSG_NAP_CAPABILITY_IDLE_TIMEOUT |
	    TSG_MESSAGING_CAP_CONSENT_SIGN |
	    TSG_MESSAGING_CAP_SERVICE_MSG |
	    TSG_MESSAGING_CAP_REAUTH;

	if (!TsProxyCreateTunnelWriteRequest(tsg, &tsgPacket))
	{
		WLog_ERR(TAG, "TsProxyCreateTunnel failure");
		tsg->state = TSG_STATE_FINAL;
		return -1;
	}

	tsg_transition_to_state(tsg, TSG_STATE_INITIAL);
	return 1;
}

static int tsg_proxy_reauth(rdpTsg* tsg)
{
	TSG_PACKET tsgPacket;
	PTSG_PACKET_REAUTH packetReauth;
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;
	tsg->reauthSequence = TRUE;
	packetReauth = &tsg->packetReauth;
	packetVersionCaps = &tsg->packetVersionCaps;
	tsgPacket.packetId = TSG_PACKET_TYPE_REAUTH;
	tsgPacket.tsgPacket.packetReauth = &tsg->packetReauth;
	packetReauth->tunnelContext = tsg->ReauthTunnelContext;
	packetReauth->packetId = TSG_PACKET_TYPE_VERSIONCAPS;
	packetReauth->tsgInitialPacket.packetVersionCaps = packetVersionCaps;

	if (!TsProxyCreateTunnelWriteRequest(tsg, &tsgPacket))
	{
		WLog_ERR(TAG, "TsProxyCreateTunnel failure");
		tsg->state = TSG_STATE_FINAL;
		return -1;
	}

	if (!TsProxyMakeTunnelCallWriteRequest(tsg, &tsg->TunnelContext, TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST))
	{
		WLog_ERR(TAG, "TsProxyMakeTunnelCall failure");
		tsg->state = TSG_STATE_FINAL;
		return -1;
	}

	tsg_transition_to_state(tsg, TSG_STATE_INITIAL);
	return 1;
}

int tsg_recv_pdu(rdpTsg* tsg, RPC_PDU* pdu)
{
	int status = -1;
	RpcClientCall* call;
	rdpRpc* rpc = tsg->rpc;

	switch (tsg->state)
	{
		case TSG_STATE_INITIAL:
			{
				CONTEXT_HANDLE* TunnelContext;
				TunnelContext = (tsg->reauthSequence) ? &tsg->NewTunnelContext : &tsg->TunnelContext;

				if (!TsProxyCreateTunnelReadResponse(tsg, pdu, TunnelContext, &tsg->TunnelId))
				{
					WLog_ERR(TAG, "TsProxyCreateTunnelReadResponse failure");
					return -1;
				}

				tsg_transition_to_state(tsg, TSG_STATE_CONNECTED);

				if (!TsProxyAuthorizeTunnelWriteRequest(tsg, TunnelContext))
				{
					WLog_ERR(TAG, "TsProxyAuthorizeTunnel failure");
					return -1;
				}

				status = 1;
			}
			break;

		case TSG_STATE_CONNECTED:
			{
				CONTEXT_HANDLE* TunnelContext;
				TunnelContext = (tsg->reauthSequence) ? &tsg->NewTunnelContext : &tsg->TunnelContext;

				if (!TsProxyAuthorizeTunnelReadResponse(tsg, pdu))
				{
					WLog_ERR(TAG, "TsProxyAuthorizeTunnelReadResponse failure");
					return -1;
				}

				tsg_transition_to_state(tsg, TSG_STATE_AUTHORIZED);

				if (!tsg->reauthSequence)
				{
					if (!TsProxyMakeTunnelCallWriteRequest(tsg, TunnelContext, TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST))
					{
						WLog_ERR(TAG, "TsProxyMakeTunnelCall failure");
						return -1;
					}
				}

				if (!TsProxyCreateChannelWriteRequest(tsg, TunnelContext))
				{
					WLog_ERR(TAG, "TsProxyCreateChannel failure");
					return -1;
				}

				status = 1;
			}
			break;

		case TSG_STATE_AUTHORIZED:
			call = rpc_client_call_find_by_id(rpc, pdu->CallId);

			if (!call)
				return -1;

			if (call->OpNum == TsProxyMakeTunnelCallOpnum)
			{
				if (!TsProxyMakeTunnelCallReadResponse(tsg, pdu))
				{
					WLog_ERR(TAG, "TsProxyMakeTunnelCallReadResponse failure");
					return -1;
				}

				status = 1;
			}
			else if (call->OpNum == TsProxyCreateChannelOpnum)
			{
				CONTEXT_HANDLE ChannelContext;

				if (!TsProxyCreateChannelReadResponse(tsg, pdu, &ChannelContext, &tsg->ChannelId))
				{
					WLog_ERR(TAG, "TsProxyCreateChannelReadResponse failure");
					return -1;
				}

				if (!tsg->reauthSequence)
					CopyMemory(&tsg->ChannelContext, &ChannelContext, sizeof(CONTEXT_HANDLE));
				else
					CopyMemory(&tsg->NewChannelContext, &ChannelContext, sizeof(CONTEXT_HANDLE));

				tsg_transition_to_state(tsg, TSG_STATE_CHANNEL_CREATED);

				if (!tsg->reauthSequence)
				{
					if (!TsProxySetupReceivePipeWriteRequest(tsg, &tsg->ChannelContext))
					{
						WLog_ERR(TAG, "TsProxySetupReceivePipe failure");
						return -1;
					}
				}
				else
				{
					if (!TsProxyCloseChannelWriteRequest(tsg, &tsg->NewChannelContext))
					{
						WLog_ERR(TAG, "TsProxyCloseChannelWriteRequest failure");
						return -1;
					}

					if (!TsProxyCloseTunnelWriteRequest(tsg, &tsg->NewTunnelContext))
					{
						WLog_ERR(TAG, "TsProxyCloseTunnelWriteRequest failure");
						return -1;
					}
				}

				tsg_transition_to_state(tsg, TSG_STATE_PIPE_CREATED);
				tsg->reauthSequence = FALSE;
				status = 1;
			}
			else
			{
				WLog_ERR(TAG, "TSG_STATE_AUTHORIZED unexpected OpNum: %"PRIu32"\n", call->OpNum);
			}

			break;

		case TSG_STATE_CHANNEL_CREATED:
			break;

		case TSG_STATE_PIPE_CREATED:
			call = rpc_client_call_find_by_id(rpc, pdu->CallId);

			if (!call)
				return -1;

			if (call->OpNum == TsProxyMakeTunnelCallOpnum)
			{
				if (!TsProxyMakeTunnelCallReadResponse(tsg, pdu))
				{
					WLog_ERR(TAG, "TsProxyMakeTunnelCallReadResponse failure");
					return -1;
				}

				if (tsg->ReauthTunnelContext)
					tsg_proxy_reauth(tsg);

				status = 1;
			}
			else if (call->OpNum == TsProxyCloseChannelOpnum)
			{
				CONTEXT_HANDLE ChannelContext;

				if (!TsProxyCloseChannelReadResponse(tsg, pdu, &ChannelContext))
				{
					WLog_ERR(TAG, "TsProxyCloseChannelReadResponse failure");
					return -1;
				}

				status = 1;
			}
			else if (call->OpNum == TsProxyCloseTunnelOpnum)
			{
				CONTEXT_HANDLE TunnelContext;

				if (!TsProxyCloseTunnelReadResponse(tsg, pdu, &TunnelContext))
				{
					WLog_ERR(TAG, "TsProxyCloseTunnelReadResponse failure");
					return -1;
				}

				status = 1;
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

				tsg_transition_to_state(tsg, TSG_STATE_CHANNEL_CLOSE_PENDING);

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

				status = 1;
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

				tsg_transition_to_state(tsg, TSG_STATE_FINAL);
				status = 1;
			}
			break;

		case TSG_STATE_FINAL:
			break;
	}

	return status;
}

int tsg_check_event_handles(rdpTsg* tsg)
{
	int status;
	status = rpc_client_in_channel_recv(tsg->rpc);

	if (status < 0)
		return -1;

	status = rpc_client_out_channel_recv(tsg->rpc);

	if (status < 0)
		return -1;

	return status;
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

	if (connection->DefaultInChannel && connection->DefaultInChannel->tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(connection->DefaultInChannel->tls->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	if (connection->NonDefaultInChannel && connection->NonDefaultInChannel->tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(connection->NonDefaultInChannel->tls->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	if (connection->DefaultOutChannel && connection->DefaultOutChannel->tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(connection->DefaultOutChannel->tls->bio, &events[nCount]);
			nCount++;
		}
		else
			return 0;
	}

	if (connection->NonDefaultOutChannel && connection->NonDefaultOutChannel->tls)
	{
		if (events && (nCount < count))
		{
			BIO_get_event(connection->NonDefaultOutChannel->tls->bio, &events[nCount]);
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

	tsg_set_hostname(tsg, hostname);
	tsg_set_machine_name(tsg, settings->ComputerName);

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

		if (tsg_check_event_handles(tsg) < 0)
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

	BIO_set_data(tsg->bio, (void*) tsg);
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

		tsg->state = TSG_STATE_CHANNEL_CLOSE_PENDING;
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

	if (!tsg)
		return -1;

	rpc = tsg->rpc;

	if (rpc->transport->layer == TRANSPORT_LAYER_CLOSED)
	{
		WLog_ERR(TAG, "tsg_read error: connection lost");
		return -1;
	}

	do
	{
		status = rpc_client_receive_pipe_read(rpc, data, (size_t) length);

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
				if (tsg_check_event_handles(tsg) < 0)
					return -1;

				WaitForSingleObject(rpc->client->PipeEvent, 100);
			}
		}
	}
	while (rpc->transport->blocking);

	return status;
}

static int tsg_write(rdpTsg* tsg, BYTE* data, UINT32 length)
{
	int status;

	if (tsg->rpc->transport->layer == TRANSPORT_LAYER_CLOSED)
	{
		WLog_ERR(TAG, "error, connection lost");
		return -1;
	}

	status = TsProxySendToServer((handle_t) tsg, data, 1, &length);

	if (status < 0)
		return -1;

	return length;
}

rdpTsg* tsg_new(rdpTransport* transport)
{
	rdpTsg* tsg;
	tsg = (rdpTsg*) calloc(1, sizeof(rdpTsg));

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
		if (tsg->rpc)
		{
			rpc_free(tsg->rpc);
			tsg->rpc = NULL;
		}

		free(tsg->Hostname);
		free(tsg->MachineName);
		free(tsg);
	}
}


static int transport_bio_tsg_write(BIO* bio, const char* buf, int num)
{
	int status;
	rdpTsg* tsg = (rdpTsg*) BIO_get_data(bio);
	BIO_clear_flags(bio, BIO_FLAGS_WRITE);
	status = tsg_write(tsg, (BYTE*) buf, num);

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
	rdpTsg* tsg = (rdpTsg*) BIO_get_data(bio);
	BIO_clear_flags(bio, BIO_FLAGS_READ);
	status = tsg_read(tsg, (BYTE*) buf, size);

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
	int status = 0;
	rdpTsg* tsg = (rdpTsg*) BIO_get_data(bio);
	RpcVirtualConnection* connection = tsg->rpc->VirtualConnection;
	RpcInChannel* inChannel = connection->DefaultInChannel;
	RpcOutChannel* outChannel = connection->DefaultOutChannel;

	if (cmd == BIO_CTRL_FLUSH)
	{
		(void)BIO_flush(inChannel->tls->bio);
		(void)BIO_flush(outChannel->tls->bio);
		status = 1;
	}
	else if (cmd == BIO_C_GET_EVENT)
	{
		if (arg2)
		{
			*((ULONG_PTR*) arg2) = (ULONG_PTR) tsg->rpc->client->PipeEvent;
			status = 1;
		}
	}
	else if (cmd == BIO_C_SET_NONBLOCK)
	{
		status = 1;
	}
	else if (cmd == BIO_C_READ_BLOCKED)
	{
		BIO* bio = outChannel->bio;
		status = BIO_read_blocked(bio);
	}
	else if (cmd == BIO_C_WRITE_BLOCKED)
	{
		BIO* bio = inChannel->bio;
		status = BIO_write_blocked(bio);
	}
	else if (cmd == BIO_C_WAIT_READ)
	{
		int timeout = (int) arg1;
		BIO* bio = outChannel->bio;

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
		BIO* bio = inChannel->bio;

		if (BIO_write_blocked(bio))
			status = BIO_wait_write(bio, timeout);
		else if (BIO_read_blocked(bio))
			status = BIO_wait_read(bio, timeout);
		else
			status = 1;
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
