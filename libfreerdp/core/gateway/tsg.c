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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/ndr.h>
#include <winpr/error.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <freerdp/log.h>
#include "rpc_client.h"
#include "tsg.h"

#define TAG FREERDP_TAG("core.gateway.tsg")

/**
 * RPC Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378623/
 * Remote Procedure Call: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378651/
 * RPC NDR Interface Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802752/
 */

/* this might be a verification trailer */

BYTE TsProxyCreateTunnelUnknownTrailerBytes[60] =
{
	0x8A, 0xE3, 0x13, 0x71, 0x02, 0xF4, 0x36, 0x71, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x02, 0x40, 0x28, 0x00, 0xDD, 0x65, 0xE2, 0x44, 0xAF, 0x7D, 0xCD, 0x42, 0x85, 0x60, 0x3C, 0xDB,
	0x6E, 0x7A, 0x27, 0x29, 0x01, 0x00, 0x03, 0x00, 0x04, 0x5D, 0x88, 0x8A, 0xEB, 0x1C, 0xC9, 0x11,
	0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60, 0x02, 0x00, 0x00, 0x00
};

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

	Stream_Length(s) = Stream_GetPosition(s);
	status = rpc_write(tsg->rpc, Stream_Buffer(s), Stream_Length(s), TsProxySendToServerOpnum);
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
	CopyMemory(&buffer[48], TsProxyCreateTunnelUnknownTrailerBytes, 60);
	status = rpc_write(rpc, buffer, length, TsProxyCreateTunnelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyCreateTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BYTE* buffer;
	UINT32 count;
	UINT32 offset;
	PTSG_PACKET packet;
	UINT32 SwitchValue;
	UINT32 MessageSwitchValue = 0;
	UINT32 MsgBytes;
	rdpRpc* rpc = tsg->rpc;
	PTSG_PACKET_CAPABILITIES tsgCaps;
	PTSG_PACKET_VERSIONCAPS versionCaps;
	PTSG_PACKET_CAPS_RESPONSE packetCapsResponse;
	PTSG_PACKET_QUARENC_RESPONSE packetQuarEncResponse;

	if (!pdu)
		return FALSE;

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
		UINT32 Pointer;
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
			offset += 4;
			MessageSwitchValue = *((UINT32*) &buffer[offset]);
			offset += 4;
		}

		if (packetCapsResponse->pktQuarEncResponse.certChainLen > 0)
		{
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

	rpc_client_receive_pool_return(rpc, pdu);
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
	status = rpc_write(rpc, buffer, length, TsProxyAuthorizeTunnelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyAuthorizeTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BYTE* buffer;
	UINT32 offset;
	UINT32 SizeValue;
	UINT32 SwitchValue;
	PTSG_PACKET packet;
	rdpRpc* rpc = tsg->rpc;
	PTSG_PACKET_RESPONSE packetResponse;

	if (!pdu)
		return FALSE;

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
	{
		free(packet);
		return FALSE;
	}

	packet->tsgPacket.packetResponse = packetResponse;
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
		offset += 4;
	}
	else
	{
		offset += SizeValue; /* ResponseData */
	}
	
	rpc_client_receive_pool_return(rpc, pdu);
	
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
	status = rpc_write(rpc, buffer, length, TsProxyMakeTunnelCallOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyMakeTunnelCallReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BOOL rc = TRUE;
	BYTE* buffer;
	UINT32 offset;
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
	{
		if (packet)
			free(packet);
		return FALSE;
	}

	packet->tsgPacket.packetMsgResponse = packetMsgResponse;
	packetMsgResponse->msgID = *((UINT32*) &buffer[offset + 12]); /* MsgId */
	packetMsgResponse->msgType = *((UINT32*) &buffer[offset + 16]); /* MsgType */
	packetMsgResponse->isMsgPresent = *((INT32*) &buffer[offset + 20]); /* IsMsgPresent */
	SwitchValue = *((UINT32*) &buffer[offset + 24]); /* SwitchValue */

	switch (SwitchValue)
	{
		case TSG_ASYNC_MESSAGE_CONSENT_MESSAGE:
			packetStringMessage = (PTSG_PACKET_STRING_MESSAGE) calloc(1, sizeof(TSG_PACKET_STRING_MESSAGE));

			if (!packetStringMessage)
			{
				if (packet)
					free(packet);
				return FALSE;
			}

			packetMsgResponse->messagePacket.consentMessage = packetStringMessage;
			packetStringMessage->isDisplayMandatory = *((INT32*) &buffer[offset + 32]); /* IsDisplayMandatory */
			packetStringMessage->isConsentMandatory = *((INT32*) &buffer[offset + 36]); /* IsConsentMandatory */
			packetStringMessage->msgBytes = *((UINT32*) &buffer[offset + 40]); /* MsgBytes */
			/* Offset */
			ActualCount = *((UINT32*) &buffer[offset + 56]); /* ActualCount */
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) &buffer[offset + 60], ActualCount, &messageText, 0, NULL, NULL);
			WLog_ERR(TAG, "Consent Message: %s", messageText);
			free(messageText);
			break;

		case TSG_ASYNC_MESSAGE_SERVICE_MESSAGE:
			packetStringMessage = (PTSG_PACKET_STRING_MESSAGE) calloc(1, sizeof(TSG_PACKET_STRING_MESSAGE));

			if (!packetStringMessage)
			{
				if (packet)
					free(packet);
				return FALSE;
			}

			packetMsgResponse->messagePacket.serviceMessage = packetStringMessage;
			packetStringMessage->isDisplayMandatory = *((INT32*) &buffer[offset + 32]); /* IsDisplayMandatory */
			packetStringMessage->isConsentMandatory = *((INT32*) &buffer[offset + 36]); /* IsConsentMandatory */
			packetStringMessage->msgBytes = *((UINT32*) &buffer[offset + 40]); /* MsgBytes */
			/* Offset */
			ActualCount = *((UINT32*) &buffer[offset + 56]); /* ActualCount */
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) &buffer[offset + 60], ActualCount, &messageText, 0, NULL, NULL);
			WLog_ERR(TAG, "Service Message: %s", messageText);
			free(messageText);
			break;

		case TSG_ASYNC_MESSAGE_REAUTH:
			packetReauthMessage = (PTSG_PACKET_REAUTH_MESSAGE) calloc(1, sizeof(TSG_PACKET_REAUTH_MESSAGE));

			if (!packetReauthMessage)
			{
				if (packet)
					free(packet);
				return FALSE;
			}

			packetMsgResponse->messagePacket.reauthMessage = packetReauthMessage;
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
	status = rpc_write(rpc, buffer, length, TsProxyCreateChannelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyCreateChannelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	BYTE* buffer;
	UINT32 offset;
	rdpRpc* rpc = tsg->rpc;

	if (!pdu)
		return FALSE;

	buffer = Stream_Buffer(pdu->s);

	if (!(pdu->Flags & RPC_PDU_FLAG_STUB))
		buffer = &buffer[24];

	offset = 0;
	/* ChannelContext (20 bytes) */
	CopyMemory(&tsg->ChannelContext.ContextType, &buffer[offset], 4); /* ContextType (4 bytes) */
	CopyMemory(tsg->ChannelContext.ContextUuid, &buffer[offset + 4], 16); /* ContextUuid (16 bytes) */

	rpc_client_receive_pool_return(rpc, pdu);

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
	status = rpc_write(rpc, buffer, length, TsProxyCloseChannelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyCloseChannelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	rdpRpc* rpc = tsg->rpc;
	pdu = rpc_recv_dequeue_pdu(rpc);

	if (!pdu)
		return FALSE;

	rpc_client_receive_pool_return(rpc, pdu);
	return TRUE;
}

HRESULT TsProxyCloseChannel(rdpTsg* tsg, PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE* context)
{
	RPC_PDU* pdu = NULL;

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

	if (!TsProxyCloseChannelReadResponse(tsg, pdu))
	{
		WLog_ERR(TAG, "error reading response");
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
	status = rpc_write(rpc, buffer, length, TsProxyCloseTunnelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);
	return TRUE;
}

BOOL TsProxyCloseTunnelReadResponse(rdpTsg* tsg, RPC_PDU* pdu)
{
	rdpRpc* rpc = tsg->rpc;

	pdu = rpc_recv_dequeue_pdu(rpc);

	if (!pdu)
		return FALSE;

	rpc_client_receive_pool_return(rpc, pdu);
	return TRUE;
}

HRESULT TsProxyCloseTunnel(rdpTsg* tsg, PTUNNEL_CONTEXT_HANDLE_SERIALIZE* context)
{
	RPC_PDU* pdu = NULL;

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

	if (!TsProxyCloseTunnelReadResponse(tsg, pdu))
	{
		WLog_ERR(TAG, "error reading response");
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
	status = rpc_write(rpc, buffer, length, TsProxySetupReceivePipeOpnum);

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

BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port)
{
	RPC_PDU* pdu = NULL;
	RpcClientCall* call;
	rdpRpc* rpc = tsg->rpc;
	rdpSettings* settings = rpc->settings;

	tsg->Port = port;

	free(tsg->Hostname);
	tsg->Hostname = NULL;
	ConvertToUnicode(CP_UTF8, 0, hostname, -1, &tsg->Hostname, 0);

	free(tsg->MachineName);
	tsg->MachineName = NULL;
	ConvertToUnicode(CP_UTF8, 0, settings->ComputerName, -1, &tsg->MachineName, 0);

	if (!rpc_connect(rpc))
	{
		WLog_ERR(TAG, "rpc_connect failed!");
		return FALSE;
	}

	WLog_DBG(TAG, "rpc_connect success");

	tsg->state = TSG_STATE_INITIAL;
	rpc->client->SynchronousSend = TRUE;
	rpc->client->SynchronousReceive = TRUE;

	/*
	 *     Sequential processing rules for connection process:
	 *
	 *  1. The RDG client MUST call TsProxyCreateTunnel to create a tunnel to the gateway.
	 *
	 *  2. If the call fails, the RDG client MUST end the protocol and MUST NOT perform the following steps.
	 *
	 *  3. The RDG client MUST initialize the following ADM elements using TsProxyCreateTunnel out parameters:
	 *
	 * 	a. The RDG client MUST initialize the ADM element Tunnel id with the tunnelId out parameter.
	 *
	 * 	b. The RDG client MUST initialize the ADM element Tunnel Context Handle with the tunnelContext
	 * 	   out parameter. This Tunnel Context Handle is used for subsequent tunnel-related calls.
	 *
	 * 	c. If TSGPacketResponse->packetId is TSG_PACKET_TYPE_CAPS_RESPONSE, where TSGPacketResponse is an out parameter,
	 *
	 * 		 i. The RDG client MUST initialize the ADM element Nonce with TSGPacketResponse->
	 * 		    TSGPacket.packetCapsResponse->pktQuarEncResponse.nonce.
	 *
	 * 		ii. The RDG client MUST initialize the ADM element Negotiated Capabilities with TSGPacketResponse->
	 * 		    TSGPacket.packetCapsResponse->pktQuarEncResponse.versionCaps->TSGCaps[0].TSGPacket.TSGCapNap.capabilities.
	 *
	 * 	d. If TSGPacketResponse->packetId is TSG_PACKET_TYPE_QUARENC_RESPONSE, where TSGPacketResponse is an out parameter,
	 *
	 * 		 i. The RDG client MUST initialize the ADM element Nonce with TSGPacketResponse->
	 * 		    TSGPacket.packetQuarEncResponse->nonce.
	 *
	 * 		ii. The RDG client MUST initialize the ADM element Negotiated Capabilities with TSGPacketResponse->
	 * 		    TSGPacket.packetQuarEncResponse->versionCaps->TSGCaps[0].TSGPacket.TSGCapNap.capabilities.
	 *
	 *  4. The RDG client MUST get its statement of health (SoH) by calling NAP EC API.<49> Details of the SoH format are
	 *     specified in [TNC-IF-TNCCSPBSoH]. If the SoH is received successfully, then the RDG client MUST encrypt the SoH
	 *     using the Triple Data Encryption Standard algorithm and encode it using one of PKCS #7 or X.509 encoding types,
	 *     whichever is supported by the RDG server certificate context available in the ADM element CertChainData.
	 *
	 *  5. The RDG client MUST copy the ADM element Nonce to TSGPacket.packetQuarRequest->data and append the encrypted SoH
	 *     message into TSGPacket.packetQuarRequest->data. The RDG client MUST set the TSGPacket.packetQuarRequest->dataLen
	 *     to the sum of the number of bytes in the encrypted SoH message and number of bytes in the ADM element Nonce, where
	 *     TSGpacket is an input parameter of TsProxyAuthorizeTunnel. The format of the packetQuarRequest field is specified
	 *     in section 2.2.9.2.1.4.
	 */

	if (!TsProxyCreateTunnel(tsg, NULL, NULL, NULL, NULL))
	{
		tsg->state = TSG_STATE_FINAL;
		return FALSE;
	}

	pdu = rpc_recv_dequeue_pdu(rpc);

	if (!TsProxyCreateTunnelReadResponse(tsg, pdu))
	{
		WLog_ERR(TAG, "error reading response");
		return FALSE;
	}

	tsg->state = TSG_STATE_CONNECTED;

	/**
	 *     Sequential processing rules for connection process (continued):
	 *
	 *  6. The RDG client MUST call TsProxyAuthorizeTunnel to authorize the tunnel.
	 *
	 *  7. If the call succeeds or fails with error E_PROXY_QUARANTINE_ACCESSDENIED, follow the steps later in this section.
	 *     Else, the RDG client MUST end the protocol and MUST NOT follow the steps later in this section.
	 *
	 *  8. If the ADM element Negotiated Capabilities contains TSG_NAP_CAPABILITY_IDLE_TIMEOUT, then the ADM element Idle
	 *     Timeout Value SHOULD be initialized with first 4 bytes of TSGPacketResponse->TSGPacket.packetResponse->responseData
	 *     and the Statement of health response variable should be initialized with the remaining bytes of responseData, where
	 *     TSGPacketResponse is an out parameter of TsProxyAuthorizeTunnel. The format of the responseData member is specified
	 *     in section 2.2.9.2.1.5.1.
	 *
	 *  9. If the ADM element Negotiated Capabilities doesn't contain TSG_NAP_CAPABILITY_IDLE_TIMEOUT, then the ADM element Idle
	 *     Timeout Value SHOULD be initialized to zero and the Statement of health response variable should be initialized with all
	 *     the bytes of TSGPacketResponse->TSGPacket.packetResponse->responseData.
	 *
	 * 10. Verify the signature of the Statement of health response variable using SHA-1 hash and decode it using the RDG server
	 *     certificate context available in the ADM element CertChainData using one of PKCS #7 or X.509 encoding types, whichever
	 *     is supported by the RDG Server certificate. The SoHR is processed by calling the NAP EC API
	 *     INapEnforcementClientConnection::GetSoHResponse.
	 *
	 * 11. If the call TsProxyAuthorizeTunnel fails with error E_PROXY_QUARANTINE_ACCESSDENIED, the RDG client MUST end the protocol
	 *     and MUST NOT follow the steps later in this section.
	 *
	 * 12. If the ADM element Idle Timeout Value is nonzero, the RDG client SHOULD start the idle time processing as specified in
	 *     section 3.6.2.1.1 and SHOULD end the protocol when the connection has been idle for the specified Idle Timeout Value.
	 */

	if (!TsProxyAuthorizeTunnel(tsg, &tsg->TunnelContext, NULL, NULL))
	{
		tsg->state = TSG_STATE_TUNNEL_CLOSE_PENDING;
		return FALSE;
	}

	pdu = rpc_recv_dequeue_pdu(rpc);

	if (!TsProxyAuthorizeTunnelReadResponse(tsg, pdu))
	{
		WLog_ERR(TAG, "error reading response");
		return FALSE;
	}

	tsg->state = TSG_STATE_AUTHORIZED;

	/**
	 *     Sequential processing rules for connection process (continued):
	 *
	 * 13. If the ADM element Negotiated Capabilities contains TSG_MESSAGING_CAP_SERVICE_MSG, a TsProxyMakeTunnelCall call MAY be
	 *     made by the client, with TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST as the parameter, to receive messages from the RDG server.
	 *
	 */

	if (!TsProxyMakeTunnelCall(tsg, &tsg->TunnelContext, TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST, NULL, NULL))
		return FALSE;

	/**
	 *     Sequential processing rules for connection process (continued):
	 *
	 * 14. The RDG client MUST call TsProxyCreateChannel to create a channel to the target server name as specified by the ADM
	 *     element Target Server Name (section 3.5.1).
	 *
	 * 15. If the call fails, the RDG client MUST end the protocol and MUST not follow the below steps.
	 *
	 * 16. The RDG client MUST initialize the following ADM elements using TsProxyCreateChannel out parameters.
	 *
	 * 	a. The RDG client MUST initialize the ADM element Channel id with the channelId out parameter.
	 *
	 * 	b. The RDG client MUST initialize the ADM element Channel Context Handle with the channelContext
	 * 	   out parameter. This Channel Context Handle is used for subsequent channel-related calls.
	 */

	if (!TsProxyCreateChannel(tsg, &tsg->TunnelContext, NULL, NULL, NULL))
		return FALSE;

	pdu = rpc_recv_dequeue_pdu(rpc);

	if (!pdu)
	{
		WLog_ERR(TAG, "error reading response");
		return FALSE;
	}

	call = rpc_client_call_find_by_id(rpc, pdu->CallId);

	if (call->OpNum == TsProxyMakeTunnelCallOpnum)
	{
		if (!TsProxyMakeTunnelCallReadResponse(tsg, pdu))
		{
			WLog_ERR(TAG, "error reading response");
			return FALSE;
		}

		pdu = rpc_recv_dequeue_pdu(rpc);
	}

	if (!TsProxyCreateChannelReadResponse(tsg, pdu))
	{
		WLog_ERR(TAG, "error reading response");
		return FALSE;
	}

	tsg->state = TSG_STATE_CHANNEL_CREATED;

	/**
	 *  Sequential processing rules for data transfer:
	 *
	 *  1. The RDG client MUST call TsProxySetupReceivePipe to receive data from the target server, via the RDG server.
	 *
	 *  2. The RDG client MUST call TsProxySendToServer to send data to the target server via the RDG server, and if
	 *     the Idle Timeout Timer is started, the RDG client SHOULD reset the Idle Timeout Timer.
	 *
	 *  3. If TsProxyMakeTunnelCall is returned, the RDG client MUST process the message and MAY call TsProxyMakeTunnelCall
	 *     again with TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST as the parameter.
	 *
	 *  4. The RDG client MUST end the protocol after it receives the final response to TsProxySetupReceivePipe.
	 *     The final response format is specified in section 2.2.9.4.3.
	 */

	if (!TsProxySetupReceivePipe((handle_t) tsg, NULL))
		return FALSE;

	rpc->client->SynchronousSend = TRUE;
	rpc->client->SynchronousReceive = TRUE;

	WLog_INFO(TAG,  "TS Gateway Connection Success");

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

	tsg->rpc->client->SynchronousReceive = TRUE;

	/* if we are already in state pending (i.e. if a server initiated disconnect was issued)
	   we have to skip TsProxyCloseChannel - see Figure 13 in section 3.2.3
	 */
	if (tsg->state != TSG_STATE_TUNNEL_CLOSE_PENDING)
	{
		if (!TsProxyCloseChannel(tsg, NULL))
			return FALSE;
	}

	if (!TsProxyMakeTunnelCall(tsg, &tsg->TunnelContext, TSG_TUNNEL_CANCEL_ASYNC_MSG_REQUEST, NULL, NULL))
		return FALSE;

	if (!TsProxyCloseTunnel(tsg, NULL))
		return FALSE;

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
	int CopyLength;
	rdpRpc* rpc;

	if (!tsg)
		return -1;

	rpc = tsg->rpc;

	if (rpc->transport->layer == TRANSPORT_LAYER_CLOSED)
	{
		WLog_ERR(TAG, "tsg_read error: connection lost");
		return -1;
	}

	if (tsg->PendingPdu)
	{
		CopyLength = (length < tsg->BytesAvailable) ? length : tsg->BytesAvailable;
		CopyMemory(data, &tsg->pdu->s->buffer[tsg->BytesRead], CopyLength);
		tsg->BytesAvailable -= CopyLength;
		tsg->BytesRead += CopyLength;

		if (tsg->BytesAvailable < 1)
		{
			tsg->PendingPdu = FALSE;
			rpc_recv_dequeue_pdu(rpc);
			rpc_client_receive_pool_return(rpc, tsg->pdu);
		}

		return CopyLength;
	}

	do
	{
		tsg->pdu = rpc_recv_peek_pdu(rpc);

		/* there is a pdu to process - move on*/
		if (tsg->pdu)
			break;

		/*
		 * no pdu available and synchronous is not required
		 * return 0 to indicate that there is no data
		 * available at the moment
		 */
		if (!tsg->rpc->client->SynchronousReceive)
			return 0;

		/* ensure that the transport wasn't already closed - in case of a retry */
		if (rpc->transport->layer == TRANSPORT_LAYER_CLOSED)
		{
			WLog_ERR(TAG, "tsg_read error: connection lost");
			return -1;
		}

	/* retry in case synchronous receive is required */
	} while (tsg->rpc->client->SynchronousReceive);

	tsg->PendingPdu = TRUE;
	tsg->BytesAvailable = Stream_Length(tsg->pdu->s);
	tsg->BytesRead = 0;
	CopyLength = (length < tsg->BytesAvailable) ? length : tsg->BytesAvailable;
	CopyMemory(data, &tsg->pdu->s->buffer[tsg->BytesRead], CopyLength);
	tsg->BytesAvailable -= CopyLength;
	tsg->BytesRead += CopyLength;

	if (tsg->BytesAvailable < 1)
	{
		tsg->PendingPdu = FALSE;
		rpc_recv_dequeue_pdu(rpc);
		rpc_client_receive_pool_return(rpc, tsg->pdu);
	}

	return CopyLength;
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

BOOL tsg_set_blocking_mode(rdpTsg* tsg, BOOL blocking)
{
	tsg->rpc->client->SynchronousSend = TRUE;
	tsg->rpc->client->SynchronousReceive = blocking;
	tsg->transport->GatewayEvent = Queue_Event(tsg->rpc->client->ReceiveQueue);
	return TRUE;
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

	tsg->PendingPdu = FALSE;
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
