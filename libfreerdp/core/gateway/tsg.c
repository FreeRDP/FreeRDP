/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Terminal Server Gateway (TSG)
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
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

#define TAG FREERDP_TAG("core.gateway.tsg")

/**
 * RPC Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378623/
 * Remote Procedure Call: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378651/
 * RPC NDR Interface Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802752/
 */

DWORD TsProxySendToServer(handle_t IDL_handle, byte pRpcMessage[], UINT32 count, UINT32* lengths)
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
	status = rpc_client_write_call(tsg->rpc, Stream_Buffer(s), Stream_Length(s), TsProxySendToServerOpnum);
	Stream_Free(s, TRUE);

	if (status <= 0)
	{
		WLog_ERR(TAG, "rpc_write failed!");
		return -1;
	}

	return length;
}

BOOL TsProxyCreateTunnelWriteRequest(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	UINT32 NapCapabilities;
	rdpRpc* rpc = tsg->rpc;

	length = 108;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	*((UINT32*) &buffer[0]) = TSG_PACKET_TYPE_VERSIONCAPS; /* PacketId */
	*((UINT32*) &buffer[4]) = TSG_PACKET_TYPE_VERSIONCAPS; /* SwitchValue */
	*((UINT32*) &buffer[8]) = 0x00020000; /* PacketVersionCapsPtr */
	*((UINT16*) &buffer[12]) = TS_GATEWAY_TRANSPORT; /* ComponentId */
	*((UINT16*) &buffer[14]) = TSG_PACKET_TYPE_VERSIONCAPS; /* PacketId */
	*((UINT32*) &buffer[16]) = 0x00020004; /* TsgCapsPtr */
	*((UINT32*) &buffer[20]) = 0x00000001; /* NumCapabilities */
	*((UINT16*) &buffer[24]) = 0x0001; /* MajorVersion */
	*((UINT16*) &buffer[26]) = 0x0001; /* MinorVersion */
	*((UINT16*) &buffer[28]) = 0x0000; /* QuarantineCapabilities */
	/* 4-byte alignment (30 + 2) */
	*((UINT16*) &buffer[30]) = 0x0000; /* 2-byte pad */
	*((UINT32*) &buffer[32]) = 0x00000001; /* MaxCount */
	*((UINT32*) &buffer[36]) = TSG_CAPABILITY_TYPE_NAP; /* CapabilityType */
	*((UINT32*) &buffer[40]) = TSG_CAPABILITY_TYPE_NAP; /* SwitchValue */

	NapCapabilities =
		TSG_NAP_CAPABILITY_QUAR_SOH |
		TSG_NAP_CAPABILITY_IDLE_TIMEOUT |
		TSG_MESSAGING_CAP_CONSENT_SIGN |
		TSG_MESSAGING_CAP_SERVICE_MSG |
		TSG_MESSAGING_CAP_REAUTH;
	/*
	 * Alternate Code Path
	 *
	 * Using reduced capabilities appears to trigger
	 * TSG_PACKET_TYPE_QUARENC_RESPONSE instead of TSG_PACKET_TYPE_CAPS_RESPONSE
	 *
	 * However, reduced capabilities may break connectivity with servers enforcing features, such as
	 * "Only allow connections from Remote Desktop Services clients that support RD Gateway messaging"
	 */

	*((UINT32*) &buffer[44]) = NapCapabilities; /* capabilities */

	/**
	 * The following 60-byte structure is apparently undocumented,
	 * but parts of it can be matched to known C706 data structures.
	 */

	/*
	 * 8-byte constant (8A E3 13 71 02 F4 36 71) also observed here:
	 * http://lists.samba.org/archive/cifs-protocol/2010-July/001543.html
	 */

	buffer[48] = 0x8A;
	buffer[49] = 0xE3;
	buffer[50] = 0x13;
	buffer[51] = 0x71;
	buffer[52] = 0x02;
	buffer[53] = 0xF4;
	buffer[54] = 0x36;
	buffer[55] = 0x71;

	*((UINT32*) &buffer[56]) = 0x00040001; /* 1.4 (version?) */
	*((UINT32*) &buffer[60]) = 0x00000001; /* 1 (element count?) */

	/* p_cont_list_t */

	buffer[64] = 2; /* ncontext_elem */
	buffer[65] = 0x40; /* reserved1 */
	*((UINT16*) &buffer[66]) = 0x0028; /* reserved2 */

	/* p_syntax_id_t */

	CopyMemory(&buffer[68], &TSGU_UUID, sizeof(p_uuid_t));
	*((UINT32*) &buffer[84]) = TSGU_SYNTAX_IF_VERSION;

	/* p_syntax_id_t */

	CopyMemory(&buffer[88], &NDR_UUID, sizeof(p_uuid_t));
	*((UINT32*) &buffer[104]) = NDR_SYNTAX_IF_VERSION;

	status = rpc_client_write_call(rpc, buffer, length, TsProxyCreateTunnelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);

	return TRUE;
}

BOOL TsProxyCreateTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
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

	if (!pdu)
		return FALSE;

	length = Stream_Length(pdu->s);
	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	packet = (PTSG_PACKET) calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	offset = 4; // Skip Packet Pointer
	packet->packetId = *((UINT32*) &buffer[offset]); /* PacketId */
	SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue */

	if ((packet->packetId == TSG_PACKET_TYPE_CAPS_RESPONSE) && (SwitchValue == TSG_PACKET_TYPE_CAPS_RESPONSE))
	{
		packetCapsResponse = (PTSG_PACKET_CAPS_RESPONSE) calloc(1, sizeof(TSG_PACKET_CAPS_RESPONSE));

		if (!packetCapsResponse)
		{
			free(packet);
			return FALSE;
		}

		packet->tsgPacket.packetCapsResponse = packetCapsResponse;
		/* PacketQuarResponsePtr (4 bytes) */
		packetCapsResponse->pktQuarEncResponse.flags = *((UINT32*) &buffer[offset + 12]); /* Flags */
		packetCapsResponse->pktQuarEncResponse.certChainLen = *((UINT32*) &buffer[offset + 16]); /* CertChainLength */
		/* CertChainDataPtr (4 bytes) */
		CopyMemory(&packetCapsResponse->pktQuarEncResponse.nonce, &buffer[offset + 24], 16); /* Nonce */
		offset += 40;
		Pointer = *((UINT32*) &buffer[offset]); /* VersionCapsPtr */
		offset += 4;

		if ((Pointer == 0x0002000C) || (Pointer == 0x00020008))
		{
			offset += 4; /* MsgID */
			offset += 4; /* MsgType */
			IsMessagePresent = *((UINT32*) &buffer[offset]);
			offset += 4;
			MessageSwitchValue = *((UINT32*) &buffer[offset]);
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
		versionCaps->tsgHeader.ComponentId = *((UINT16*) &buffer[offset]); /* ComponentId */
		versionCaps->tsgHeader.PacketId = *((UINT16*) &buffer[offset + 2]); /* PacketId */
		offset += 4;

		if (versionCaps->tsgHeader.ComponentId != TS_GATEWAY_TRANSPORT)
		{
			WLog_ERR(TAG, "Unexpected ComponentId: 0x%04X, Expected TS_GATEWAY_TRANSPORT",
					 versionCaps->tsgHeader.ComponentId);
			free(packetCapsResponse);
			free(versionCaps);
			free(packet);
			return FALSE;
		}

		Pointer = *((UINT32*) &buffer[offset]); /* TsgCapsPtr */
		versionCaps->numCapabilities = *((UINT32*) &buffer[offset + 4]); /* NumCapabilities */
		versionCaps->majorVersion = *((UINT16*) &buffer[offset + 8]); /* MajorVersion */
		versionCaps->minorVersion = *((UINT16*) &buffer[offset + 10]); /* MinorVersion */
		versionCaps->quarantineCapabilities = *((UINT16*) &buffer[offset + 12]); /* QuarantineCapabilities */
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
		tsgCaps->capabilityType = *((UINT32*) &buffer[offset]); /* CapabilityType */
		SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue */
		offset += 8;

		if ((SwitchValue != TSG_CAPABILITY_TYPE_NAP) || (tsgCaps->capabilityType != TSG_CAPABILITY_TYPE_NAP))
		{
			WLog_ERR(TAG, "Unexpected CapabilityType: 0x%08X, Expected TSG_CAPABILITY_TYPE_NAP",
					 tsgCaps->capabilityType);
			free(tsgCaps);
			free(versionCaps);
			free(packetCapsResponse);
			free(packet);
			return FALSE;
		}

		tsgCaps->tsgPacket.tsgCapNap.capabilities = *((UINT32*) &buffer[offset]); /* Capabilities */
		offset += 4;

		switch (MessageSwitchValue)
		{
			case TSG_ASYNC_MESSAGE_CONSENT_MESSAGE:
			case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
				offset += 4; // IsDisplayMandatory
				offset += 4; // IsConsent Mandatory
				MsgBytes = *((UINT32*) &buffer[offset]);
				offset += 4;
				Pointer = *((UINT32*) &buffer[offset]);
				offset += 4;

				if (Pointer)
				{
					offset += 4; // MaxCount
					offset += 8; // UnicodeString Offset, Length
				}

				if (MsgBytes > TSG_MESSAGING_MAX_MESSAGE_LENGTH)
				{
					WLog_ERR(TAG, "Out of Spec Message Length %d", MsgBytes);
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
				offset += 8; // UINT64 TunnelContext, not to be confused with
				// the ContextHandle TunnelContext below.
				break;

			default:
				WLog_ERR(TAG, "Unexpected Message Type: 0x%X", (int) MessageSwitchValue);
				free(tsgCaps);
				free(versionCaps);
				free(packetCapsResponse);
				free(packet);
				return FALSE;
		}

		rpc_offset_align(&offset, 4);
		/* TunnelContext (20 bytes) */
		CopyMemory(&tsg->TunnelContext.ContextType, &buffer[offset], 4); /* ContextType */
		CopyMemory(tsg->TunnelContext.ContextUuid, &buffer[offset + 4], 16); /* ContextUuid */
		offset += 20;
		// UINT32 TunnelId
		// HRESULT ReturnValue

		free(tsgCaps);
		free(versionCaps);
		free(packetCapsResponse);
	}
	else if ((packet->packetId == TSG_PACKET_TYPE_QUARENC_RESPONSE) && (SwitchValue == TSG_PACKET_TYPE_QUARENC_RESPONSE))
	{
		packetQuarEncResponse = (PTSG_PACKET_QUARENC_RESPONSE) calloc(1, sizeof(TSG_PACKET_QUARENC_RESPONSE));

		if (!packetQuarEncResponse)
		{
			free(packet);
			return FALSE;
		}

		packet->tsgPacket.packetQuarEncResponse = packetQuarEncResponse;
		/* PacketQuarResponsePtr (4 bytes) */
		packetQuarEncResponse->flags = *((UINT32*) &buffer[offset + 12]); /* Flags */
		packetQuarEncResponse->certChainLen = *((UINT32*) &buffer[offset + 16]); /* CertChainLength */
		/* CertChainDataPtr (4 bytes) */
		CopyMemory(&packetQuarEncResponse->nonce, &buffer[offset + 24], 16); /* Nonce */
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
		versionCaps->tsgHeader.ComponentId = *((UINT16*) &buffer[offset]); /* ComponentId */
		versionCaps->tsgHeader.PacketId = *((UINT16*) &buffer[offset + 2]); /* PacketId */
		offset += 4;

		if (versionCaps->tsgHeader.ComponentId != TS_GATEWAY_TRANSPORT)
		{
			WLog_ERR(TAG, "Unexpected ComponentId: 0x%04X, Expected TS_GATEWAY_TRANSPORT",
					 versionCaps->tsgHeader.ComponentId);
			free(versionCaps);
			free(packetQuarEncResponse);
			free(packet);
			return FALSE;
		}

		Pointer = *((UINT32*) &buffer[offset]); /* TsgCapsPtr */
		versionCaps->numCapabilities = *((UINT32*) &buffer[offset + 4]); /* NumCapabilities */
		versionCaps->majorVersion = *((UINT16*) &buffer[offset + 8]); /* MajorVersion */
		versionCaps->majorVersion = *((UINT16*) &buffer[offset + 10]); /* MinorVersion */
		versionCaps->quarantineCapabilities = *((UINT16*) &buffer[offset + 12]); /* QuarantineCapabilities */
		offset += 14;
		/* 4-byte alignment */
		rpc_offset_align(&offset, 4);
		/* Not sure exactly what this is */
		offset += 4; /* 0x00000001 (4 bytes) */
		offset += 4; /* 0x00000001 (4 bytes) */
		offset += 4; /* 0x00000001 (4 bytes) */
		offset += 4; /* 0x00000002 (4 bytes) */
		/* TunnelContext (20 bytes) */
		CopyMemory(&tsg->TunnelContext.ContextType, &buffer[offset], 4); /* ContextType */
		CopyMemory(tsg->TunnelContext.ContextUuid, &buffer[offset + 4], 16); /* ContextUuid */
		offset += 20;

		free(versionCaps);
		free(packetQuarEncResponse);
	}
	else
	{
		WLog_ERR(TAG, "Unexpected PacketId: 0x%08X, Expected TSG_PACKET_TYPE_CAPS_RESPONSE "
				 "or TSG_PACKET_TYPE_QUARENC_RESPONSE", packet->packetId);
		free(packet);
		return FALSE;
	}

	free(packet);
	return TRUE;
}

BOOL TsProxyCreateTunnel(rdpTsg* tsg, PTSG_PACKET tsgPacket, PTSG_PACKET* tsgPacketResponse,
			PTUNNEL_CONTEXT_HANDLE_SERIALIZE* tunnelContext, UINT32* tunnelId)
{
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

	WLog_DBG(TAG, "TsProxyCreateTunnel");

	if (!TsProxyCreateTunnelWriteRequest(tsg))
	{
		WLog_ERR(TAG, "error writing request");
		return FALSE;
	}

	return TRUE;
}

BOOL TsProxyAuthorizeTunnelWriteRequest(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext)
{
	UINT32 pad;
	int status;
	BYTE* buffer;
	UINT32 count;
	UINT32 length;
	UINT32 offset;
	CONTEXT_HANDLE* handle;
	rdpRpc* rpc = tsg->rpc;

	count = _wcslen(tsg->MachineName) + 1;
	offset = 64 + (count * 2);
	rpc_offset_align(&offset, 4);
	offset += 4;
	length = offset;

	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext */
	handle = (CONTEXT_HANDLE*) tunnelContext;
	CopyMemory(&buffer[0], &handle->ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], handle->ContextUuid, 16); /* ContextUuid */
	/* 4-byte alignment */
	*((UINT32*) &buffer[20]) = TSG_PACKET_TYPE_QUARREQUEST; /* PacketId */
	*((UINT32*) &buffer[24]) = TSG_PACKET_TYPE_QUARREQUEST; /* SwitchValue */
	*((UINT32*) &buffer[28]) = 0x00020000; /* PacketQuarRequestPtr */
	*((UINT32*) &buffer[32]) = 0x00000000; /* Flags */
	*((UINT32*) &buffer[36]) = 0x00020004; /* MachineNamePtr */
	*((UINT32*) &buffer[40]) = count; /* NameLength */
	*((UINT32*) &buffer[44]) = 0x00020008; /* DataPtr */
	*((UINT32*) &buffer[48]) = 0; /* DataLength */
	/* MachineName */
	*((UINT32*) &buffer[52]) = count; /* MaxCount */
	*((UINT32*) &buffer[56]) = 0; /* Offset */
	*((UINT32*) &buffer[60]) = count; /* ActualCount */
	CopyMemory(&buffer[64], tsg->MachineName, count * 2); /* Array */
	offset = 64 + (count * 2);
	/* 4-byte alignment */
	pad = rpc_offset_align(&offset, 4);
	ZeroMemory(&buffer[offset - pad], pad);
	*((UINT32*) &buffer[offset]) = 0x00000000; /* MaxCount */
	offset += 4;
	status = rpc_client_write_call(rpc, buffer, length, TsProxyAuthorizeTunnelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyAuthorizeTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
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

	if (!pdu)
		return FALSE;

	length = Stream_Length(pdu->s);
	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	packet = (PTSG_PACKET) calloc(1, sizeof(TSG_PACKET));

	if (!packet)
		return FALSE;

	offset = 4;
	packet->packetId = *((UINT32*) &buffer[offset]); /* PacketId */
	SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue */

	if (packet->packetId == E_PROXY_NAP_ACCESSDENIED)
	{
		WLog_ERR(TAG, "status: E_PROXY_NAP_ACCESSDENIED (0x%08X)", E_PROXY_NAP_ACCESSDENIED);
		WLog_ERR(TAG, "Ensure that the Gateway Connection Authorization Policy is correct");
		free(packet);
		return FALSE;
	}

	if ((packet->packetId != TSG_PACKET_TYPE_RESPONSE) || (SwitchValue != TSG_PACKET_TYPE_RESPONSE))
	{
		WLog_ERR(TAG, "Unexpected PacketId: 0x%08X, Expected TSG_PACKET_TYPE_RESPONSE",
				 packet->packetId);
		free(packet);
		return FALSE;
	}

	packetResponse = (PTSG_PACKET_RESPONSE) calloc(1, sizeof(TSG_PACKET_RESPONSE));

	if (!packetResponse)
		return FALSE;

	packet->tsgPacket.packetResponse = packetResponse;
	Pointer = *((UINT32*) &buffer[offset + 8]); /* PacketResponsePtr */
	packetResponse->flags = *((UINT32*) &buffer[offset + 12]); /* Flags */

	if (packetResponse->flags != TSG_PACKET_TYPE_QUARREQUEST)
	{
		WLog_ERR(TAG, "Unexpected Packet Response Flags: 0x%08X, Expected TSG_PACKET_TYPE_QUARREQUEST",
				 packetResponse->flags);
		free(packet);
		free(packetResponse);
		return FALSE;
	}

	/* Reserved (4 bytes) */
	Pointer = *((UINT32*) &buffer[offset + 20]); /* ResponseDataPtr */
	packetResponse->responseDataLen = *((UINT32*) &buffer[offset + 24]); /* ResponseDataLength */
	packetResponse->redirectionFlags.enableAllRedirections = *((UINT32*) &buffer[offset + 28]); /* EnableAllRedirections */
	packetResponse->redirectionFlags.disableAllRedirections = *((UINT32*) &buffer[offset + 32]); /* DisableAllRedirections */
	packetResponse->redirectionFlags.driveRedirectionDisabled = *((UINT32*) &buffer[offset + 36]); /* DriveRedirectionDisabled */
	packetResponse->redirectionFlags.printerRedirectionDisabled = *((UINT32*) &buffer[offset + 40]); /* PrinterRedirectionDisabled */
	packetResponse->redirectionFlags.portRedirectionDisabled = *((UINT32*) &buffer[offset + 44]); /* PortRedirectionDisabled */
	packetResponse->redirectionFlags.reserved = *((UINT32*) &buffer[offset + 48]); /* Reserved */
	packetResponse->redirectionFlags.clipboardRedirectionDisabled = *((UINT32*) &buffer[offset + 52]); /* ClipboardRedirectionDisabled */
	packetResponse->redirectionFlags.pnpRedirectionDisabled = *((UINT32*) &buffer[offset + 56]); /* PnpRedirectionDisabled */
	offset += 60;
	SizeValue = *((UINT32*) &buffer[offset]);
	offset += 4;

	if (SizeValue != packetResponse->responseDataLen)
	{
		WLog_ERR(TAG, "Unexpected size value: %d, expected: %d",
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

BOOL TsProxyAuthorizeTunnel(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
				PTSG_PACKET tsgPacket, PTSG_PACKET* tsgPacketResponse)
{
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

	WLog_DBG(TAG, "TsProxyAuthorizeTunnel");

	if (!TsProxyAuthorizeTunnelWriteRequest(tsg, tunnelContext))
	{
		WLog_ERR(TAG, "error writing request");
		return FALSE;
	}

	return TRUE;
}

BOOL TsProxyMakeTunnelCallWriteRequest(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext, unsigned long procId)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	CONTEXT_HANDLE* handle;
	rdpRpc* rpc = tsg->rpc;

	length = 40;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext */
	handle = (CONTEXT_HANDLE*) tunnelContext;
	CopyMemory(&buffer[0], &handle->ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], handle->ContextUuid, 16); /* ContextUuid */
	*((UINT32*) &buffer[20]) = procId; /* ProcId */
	/* 4-byte alignment */
	*((UINT32*) &buffer[24]) = TSG_PACKET_TYPE_MSGREQUEST_PACKET; /* PacketId */
	*((UINT32*) &buffer[28]) = TSG_PACKET_TYPE_MSGREQUEST_PACKET; /* SwitchValue */
	*((UINT32*) &buffer[32]) = 0x00020000; /* PacketMsgRequestPtr */
	*((UINT32*) &buffer[36]) = 0x00000001; /* MaxMessagesPerBatch */
	status = rpc_client_write_call(rpc, buffer, length, TsProxyMakeTunnelCallOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyMakeTunnelCallReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BOOL rc = TRUE;
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;
	UINT32 Pointer;
	UINT32 MaxCount;
	UINT32 ActualCount;
	UINT32 SwitchValue;
	PTSG_PACKET packet;
	char* messageText = NULL;
	PTSG_PACKET_MSG_RESPONSE packetMsgResponse;
	PTSG_PACKET_STRING_MESSAGE packetStringMessage = NULL;
	PTSG_PACKET_REAUTH_MESSAGE packetReauthMessage = NULL;

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

	offset = 4;
	packet->packetId = *((UINT32*) &buffer[offset]); /* PacketId */
	SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue */

	if ((packet->packetId != TSG_PACKET_TYPE_MESSAGE_PACKET) || (SwitchValue != TSG_PACKET_TYPE_MESSAGE_PACKET))
	{
		WLog_ERR(TAG, "Unexpected PacketId: 0x%08X, Expected TSG_PACKET_TYPE_MESSAGE_PACKET",
				 packet->packetId);
		free(packet);
		return FALSE;
	}

	packetMsgResponse = (PTSG_PACKET_MSG_RESPONSE) calloc(1, sizeof(TSG_PACKET_MSG_RESPONSE));

	if (!packetMsgResponse)
		return FALSE;

	packet->tsgPacket.packetMsgResponse = packetMsgResponse;
	Pointer = *((UINT32*) &buffer[offset + 8]); /* PacketMsgResponsePtr */
	packetMsgResponse->msgID = *((UINT32*) &buffer[offset + 12]); /* MsgId */
	packetMsgResponse->msgType = *((UINT32*) &buffer[offset + 16]); /* MsgType */
	packetMsgResponse->isMsgPresent = *((INT32*) &buffer[offset + 20]); /* IsMsgPresent */
	SwitchValue = *((UINT32*) &buffer[offset + 24]); /* SwitchValue */

	switch (SwitchValue)
	{
		case TSG_ASYNC_MESSAGE_CONSENT_MESSAGE:
			packetStringMessage = (PTSG_PACKET_STRING_MESSAGE) calloc(1, sizeof(TSG_PACKET_STRING_MESSAGE));

			if (!packetStringMessage)
				return FALSE;

			packetMsgResponse->messagePacket.consentMessage = packetStringMessage;
			Pointer = *((UINT32*) &buffer[offset + 28]); /* ConsentMessagePtr */
			packetStringMessage->isDisplayMandatory = *((INT32*) &buffer[offset + 32]); /* IsDisplayMandatory */
			packetStringMessage->isConsentMandatory = *((INT32*) &buffer[offset + 36]); /* IsConsentMandatory */
			packetStringMessage->msgBytes = *((UINT32*) &buffer[offset + 40]); /* MsgBytes */
			Pointer = *((UINT32*) &buffer[offset + 44]); /* MsgPtr */
			MaxCount = *((UINT32*) &buffer[offset + 48]); /* MaxCount */
			/* Offset */
			ActualCount = *((UINT32*) &buffer[offset + 56]); /* ActualCount */
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) &buffer[offset + 60], ActualCount, &messageText, 0, NULL, NULL);
			WLog_ERR(TAG, "Consent Message: %s", messageText);
			free(messageText);
			break;

		case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
			packetStringMessage = (PTSG_PACKET_STRING_MESSAGE) calloc(1, sizeof(TSG_PACKET_STRING_MESSAGE));

			if (!packetStringMessage)
				return FALSE;

			packetMsgResponse->messagePacket.serviceMessage = packetStringMessage;
			Pointer = *((UINT32*) &buffer[offset + 28]); /* ServiceMessagePtr */
			packetStringMessage->isDisplayMandatory = *((INT32*) &buffer[offset + 32]); /* IsDisplayMandatory */
			packetStringMessage->isConsentMandatory = *((INT32*) &buffer[offset + 36]); /* IsConsentMandatory */
			packetStringMessage->msgBytes = *((UINT32*) &buffer[offset + 40]); /* MsgBytes */
			Pointer = *((UINT32*) &buffer[offset + 44]); /* MsgPtr */
			MaxCount = *((UINT32*) &buffer[offset + 48]); /* MaxCount */
			/* Offset */
			ActualCount = *((UINT32*) &buffer[offset + 56]); /* ActualCount */
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) &buffer[offset + 60], ActualCount, &messageText, 0, NULL, NULL);
			WLog_ERR(TAG, "Service Message: %s", messageText);
			free(messageText);
			break;

		case TSG_ASYNC_MESSAGE_REAUTH:
			packetReauthMessage = (PTSG_PACKET_REAUTH_MESSAGE) calloc(1, sizeof(TSG_PACKET_REAUTH_MESSAGE));

			if (!packetReauthMessage)
				return FALSE;

			packetMsgResponse->messagePacket.reauthMessage = packetReauthMessage;
			Pointer = *((UINT32*) &buffer[offset + 28]); /* ReauthMessagePtr */
			break;

		default:
			WLog_ERR(TAG, "unexpected message type: %d", SwitchValue);
			rc = FALSE;
			break;
	}

	if (packet)
	{
		if (packet->tsgPacket.packetMsgResponse)
		{
			if (packet->tsgPacket.packetMsgResponse->messagePacket.reauthMessage)
				free(packet->tsgPacket.packetMsgResponse->messagePacket.reauthMessage);

			free(packet->tsgPacket.packetMsgResponse);
		}

		free(packet);
	}

	return rc;
}

BOOL TsProxyMakeTunnelCall(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
			UINT32 procId, PTSG_PACKET tsgPacket, PTSG_PACKET* tsgPacketResponse)
{
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

	WLog_DBG(TAG, "TsProxyMakeTunnelCall");

	if (!TsProxyMakeTunnelCallWriteRequest(tsg, tunnelContext, procId))
	{
		WLog_ERR(TAG, "error writing request");
		return FALSE;
	}

	return TRUE;
}

BOOL TsProxyCreateChannelWriteRequest(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext)
{
	int status;
	UINT32 count;
	BYTE* buffer;
	UINT32 length;
	CONTEXT_HANDLE* handle;
	rdpRpc* rpc = tsg->rpc;
	count = _wcslen(tsg->Hostname) + 1;

	length = 60 + (count * 2);
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext */
	handle = (CONTEXT_HANDLE*) tunnelContext;
	CopyMemory(&buffer[0], &handle->ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], handle->ContextUuid, 16); /* ContextUuid */
	/* TSENDPOINTINFO */
	*((UINT32*) &buffer[20]) = 0x00020000; /* ResourceNamePtr */
	*((UINT32*) &buffer[24]) = 0x00000001; /* NumResourceNames */
	*((UINT32*) &buffer[28]) = 0x00000000; /* AlternateResourceNamesPtr */
	*((UINT16*) &buffer[32]) = 0x0000; /* NumAlternateResourceNames */
	*((UINT16*) &buffer[34]) = 0x0000; /* Pad (2 bytes) */
	/* Port (4 bytes) */
	*((UINT16*) &buffer[36]) = 0x0003; /* ProtocolId (RDP = 3) */
	*((UINT16*) &buffer[38]) = tsg->Port; /* PortNumber (0xD3D = 3389) */
	*((UINT32*) &buffer[40]) = 0x00000001; /* NumResourceNames */
	*((UINT32*) &buffer[44]) = 0x00020004; /* ResourceNamePtr */
	*((UINT32*) &buffer[48]) = count; /* MaxCount */
	*((UINT32*) &buffer[52]) = 0; /* Offset */
	*((UINT32*) &buffer[56]) = count; /* ActualCount */
	CopyMemory(&buffer[60], tsg->Hostname, count * 2); /* Array */
	status = rpc_client_write_call(rpc, buffer, length, TsProxyCreateChannelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyCreateChannelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;

	if (!pdu)
		return FALSE;

	length = Stream_Length(pdu->s);
	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	offset = 0;
	/* ChannelContext (20 bytes) */
	CopyMemory(&tsg->ChannelContext.ContextType, &buffer[offset], 4); /* ContextType (4 bytes) */
	CopyMemory(tsg->ChannelContext.ContextUuid, &buffer[offset + 4], 16); /* ContextUuid (16 bytes) */

	return TRUE;
}

BOOL TsProxyCreateChannel(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext, PTSENDPOINTINFO tsEndPointInfo,
					PCHANNEL_CONTEXT_HANDLE_SERIALIZE* channelContext, UINT32* channelId)
{
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

	WLog_DBG(TAG, "TsProxyCreateChannel");

	if (!TsProxyCreateChannelWriteRequest(tsg, tunnelContext))
	{
		WLog_ERR(TAG, "error writing request");
		return FALSE;
	}

	return TRUE;
}

BOOL TsProxyCloseChannelWriteRequest(rdpTsg* tsg, PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE* context)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	length = 20;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext */
	CopyMemory(&buffer[0], &tsg->ChannelContext.ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], tsg->ChannelContext.ContextUuid, 16); /* ContextUuid */
	status = rpc_client_write_call(rpc, buffer, length, TsProxyCloseChannelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyCloseChannelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;

	if (!pdu)
		return FALSE;

	length = Stream_Length(pdu->s);
	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	offset = 0;

	return TRUE;
}

HRESULT TsProxyCloseChannel(rdpTsg* tsg, PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE* context)
{
	/**
	 * HRESULT TsProxyCloseChannel(
	 * [in, out] PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE* context
	 * );
	 */

	WLog_DBG(TAG, "TsProxyCloseChannel");

	if (!TsProxyCloseChannelWriteRequest(tsg, context))
	{
		WLog_ERR(TAG, "error writing request");
		return FALSE;
	}

	return TRUE;
}

BOOL TsProxyCloseTunnelWriteRequest(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_SERIALIZE* context)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	length = 20;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* TunnelContext */
	CopyMemory(&buffer[0], &tsg->TunnelContext.ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], tsg->TunnelContext.ContextUuid, 16); /* ContextUuid */
	status = rpc_client_write_call(rpc, buffer, length, TsProxyCloseTunnelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyCloseTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;

	if (!pdu)
		return FALSE;

	length = Stream_Length(pdu->s);
	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	offset = 0;

	return TRUE;
}

HRESULT TsProxyCloseTunnel(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_SERIALIZE* context)
{
	/**
	 * HRESULT TsProxyCloseTunnel(
	 * [in, out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* context
	 * );
	 */

	WLog_DBG(TAG, "TsProxyCloseTunnel");

	if (!TsProxyCloseTunnelWriteRequest(tsg, context))
	{
		WLog_ERR(TAG, "error writing request");
		return FALSE;
	}

	return TRUE;
}

BOOL TsProxySetupReceivePipeWriteRequest(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	length = 20;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return FALSE;

	/* ChannelContext */
	CopyMemory(&buffer[0], &tsg->ChannelContext.ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], tsg->ChannelContext.ContextUuid, 16); /* ContextUuid */
	status = rpc_client_write_call(rpc, buffer, length, TsProxySetupReceivePipeOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxySetupReceivePipeReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	return TRUE;
}

BOOL TsProxySetupReceivePipe(handle_t IDL_handle, BYTE* pRpcMessage)
{
	rdpTsg* tsg = (rdpTsg*) IDL_handle;

	/**
	 * OpNum = 8
	 *
	 * DWORD TsProxySetupReceivePipe(
	 * [in, max_is(32767)] byte pRpcMessage[]
	 * );
	 */

	WLog_DBG(TAG, "TsProxySetupReceivePipe");

	if (!TsProxySetupReceivePipeWriteRequest(tsg))
	{
		WLog_ERR(TAG, "error writing request");
		return FALSE;
	}

	return TRUE;
}

int tsg_transition_to_state(rdpTsg* tsg, TSG_STATE state)
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

int tsg_recv_pdu(rdpTsg* tsg, RPC_PDU* pdu)
{
	int status = -1;
	RpcClientCall* call;
	rdpRpc* rpc = tsg->rpc;

	switch (tsg->state)
	{
		case TSG_STATE_INITIAL:

			if (!TsProxyCreateTunnelReadResponse(tsg, pdu))
			{
				WLog_ERR(TAG, "TsProxyCreateTunnelReadResponse failure");
				return -1;
			}

			tsg_transition_to_state(tsg, TSG_STATE_CONNECTED);

			if (!TsProxyAuthorizeTunnel(tsg, &tsg->TunnelContext, NULL, NULL))
			{
				WLog_ERR(TAG, "TsProxyAuthorizeTunnel failure");
				return -1;
			}

			status = 1;

			break;

		case TSG_STATE_CONNECTED:

			if (!TsProxyAuthorizeTunnelReadResponse(tsg, pdu))
			{
				WLog_ERR(TAG, "TsProxyAuthorizeTunnelReadResponse failure");
				return -1;
			}

			tsg_transition_to_state(tsg, TSG_STATE_AUTHORIZED);

			if (!TsProxyMakeTunnelCall(tsg, &tsg->TunnelContext, TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST, NULL, NULL))
			{
				WLog_ERR(TAG, "TsProxyMakeTunnelCall failure");
				return -1;
			}

			if (!TsProxyCreateChannel(tsg, &tsg->TunnelContext, NULL, NULL, NULL))
			{
				WLog_ERR(TAG, "TsProxyCreateChannel failure");
				return -1;
			}

			status = 1;

			break;

		case TSG_STATE_AUTHORIZED:

			call = rpc_client_call_find_by_id(rpc, pdu->CallId);

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
				if (!TsProxyCreateChannelReadResponse(tsg, pdu))
				{
					WLog_ERR(TAG, "TsProxyCreateChannelReadResponse failure");
					return -1;
				}

				tsg_transition_to_state(tsg, TSG_STATE_CHANNEL_CREATED);

				if (!TsProxySetupReceivePipe((handle_t) tsg, NULL))
				{
					WLog_ERR(TAG, "TsProxySetupReceivePipe failure");
					return -1;
				}

				tsg_transition_to_state(tsg, TSG_STATE_PIPE_CREATED);

				status = 1;
			}
			else
			{
				WLog_ERR(TAG, "TSG_STATE_AUTHORIZED unexpected OpNum: %d\n", call->OpNum);
			}

			break;

		case TSG_STATE_CHANNEL_CREATED:
			break;

		case TSG_STATE_PIPE_CREATED:
			break;

		case TSG_STATE_TUNNEL_CLOSE_PENDING:

			if (!TsProxyCloseChannelReadResponse(tsg, pdu))
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

			if (!TsProxyMakeTunnelCall(tsg, &tsg->TunnelContext, TSG_TUNNEL_CANCEL_ASYNC_MSG_REQUEST, NULL, NULL))
			{
				WLog_ERR(TAG, "TsProxyMakeTunnelCall failure");
				return FALSE;
			}

			status = 1;

			break;

		case TSG_STATE_CHANNEL_CLOSE_PENDING:

			if (!TsProxyCloseTunnelReadResponse(tsg, pdu))
			{
				WLog_ERR(TAG, "TsProxyCloseTunnelReadResponse failure");
				return FALSE;
			}

			tsg_transition_to_state(tsg, TSG_STATE_FINAL);

			status = 1;

			break;

		case TSG_STATE_FINAL:
			break;
	}

	return status;
}

int tsg_check(rdpTsg* tsg)
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

BOOL tsg_set_hostname(rdpTsg* tsg, const char* hostname)
{
	free(tsg->Hostname);
	tsg->Hostname = NULL;

	ConvertToUnicode(CP_UTF8, 0, hostname, -1, &tsg->Hostname, 0);

	return TRUE;
}

BOOL tsg_set_machine_name(rdpTsg* tsg, const char* machineName)
{
	free(tsg->MachineName);
	tsg->MachineName = NULL;

	ConvertToUnicode(CP_UTF8, 0, machineName, -1, &tsg->MachineName, 0);

	return TRUE;
}

BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port, int timeout)
{
	HANDLE events[2];
	rdpRpc* rpc = tsg->rpc;
	RpcInChannel* inChannel;
	RpcOutChannel* outChannel;
	RpcVirtualConnection* connection;
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

	connection = rpc->VirtualConnection;
	inChannel = connection->DefaultInChannel;
	outChannel = connection->DefaultOutChannel;

	BIO_get_event(inChannel->tls->bio, &events[0]);
	BIO_get_event(outChannel->tls->bio, &events[1]);

	while (tsg->state != TSG_STATE_PIPE_CREATED)
	{
		WaitForMultipleObjects(2, events, FALSE, 100);

		if (tsg_check(tsg) < 0)
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

	tsg->bio->ptr = (void*) tsg;

	transport->frontBio = tsg->bio;
	transport->TcpIn = inChannel->tcp;
	transport->TlsIn = inChannel->tls;
	transport->TcpOut = outChannel->tcp;
	transport->TlsOut = outChannel->tls;
	transport->GatewayEvent = rpc->client->PipeEvent;
	transport->SplitInputOutput = TRUE;
	transport->layer = TRANSPORT_LAYER_TSG;

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
		if (!TsProxyCloseChannel(tsg, NULL))
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

int tsg_read(rdpTsg* tsg, BYTE* data, UINT32 length)
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
				if (tsg_check(tsg) < 0)
					return -1;

				WaitForSingleObject(rpc->client->PipeEvent, 100);
			}
		}
	}
	while (rpc->transport->blocking);

	return status;
}

int tsg_write(rdpTsg* tsg, BYTE* data, UINT32 length)
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
		rdpTransport* transport = tsg->transport;

		if (tsg->bio)
		{
			BIO_free(tsg->bio);
			tsg->bio = NULL;
		}

		if (tsg->rpc)
		{
			rpc_free(tsg->rpc);
			tsg->rpc = NULL;
		}

		free(tsg->Hostname);
		free(tsg->MachineName);

		if (transport->TlsIn)
			tls_free(transport->TlsIn);

		if (transport->TcpIn)
			freerdp_tcp_free(transport->TcpIn);

		if (transport->TlsOut)
			tls_free(transport->TlsOut);

		if (transport->TcpOut)
			freerdp_tcp_free(transport->TcpOut);

		free(tsg);
	}
}

long transport_bio_tsg_callback(BIO* bio, int mode, const char* argp, int argi, long argl, long ret)
{
	return 1;
}

static int transport_bio_tsg_write(BIO* bio, const char* buf, int num)
{
	int status;
	rdpTsg* tsg = (rdpTsg*) bio->ptr;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE);

	status = tsg_write(tsg, (BYTE*) buf, num);

	if (status < 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	}
	else if (status == 0)
	{
		BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
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
	rdpTsg* tsg = (rdpTsg*) bio->ptr;

	BIO_clear_flags(bio, BIO_FLAGS_READ);

	status = tsg_read(tsg, (BYTE*) buf, size);

	if (status < 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	}
	else if (status == 0)
	{
		BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
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
	if (cmd == BIO_CTRL_FLUSH)
	{
		return 1;
	}

	return 0;
}

static int transport_bio_tsg_new(BIO* bio)
{
	bio->init = 1;
	bio->num = 0;
	bio->ptr = NULL;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;
	return 1;
}

static int transport_bio_tsg_free(BIO* bio)
{
	return 1;
}

static BIO_METHOD transport_bio_tsg_methods =
{
	BIO_TYPE_TSG,
	"TSGateway",
	transport_bio_tsg_write,
	transport_bio_tsg_read,
	transport_bio_tsg_puts,
	transport_bio_tsg_gets,
	transport_bio_tsg_ctrl,
	transport_bio_tsg_new,
	transport_bio_tsg_free,
	NULL,
};

BIO_METHOD* BIO_s_tsg(void)
{
	return &transport_bio_tsg_methods;
}
