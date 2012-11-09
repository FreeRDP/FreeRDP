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

#include <freerdp/utils/sleep.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/unicode.h>

#include <winpr/crt.h>
#include <winpr/ndr.h>

#include "tsg.h"

/**
 * RPC Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378623/
 * Remote Procedure Call: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378651/
 * RPC NDR Interface Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802752/
 */

const RPC_FAULT_CODE RPC_TSG_FAULT_CODES[] =
{
	DEFINE_RPC_FAULT_CODE(ERROR_SUCCESS)
	DEFINE_RPC_FAULT_CODE(ERROR_ACCESS_DENIED)
	DEFINE_RPC_FAULT_CODE(ERROR_ONLY_IF_CONNECTED)
	DEFINE_RPC_FAULT_CODE(ERROR_INVALID_PARAMETER)
	DEFINE_RPC_FAULT_CODE(ERROR_GRACEFUL_DISCONNECT)
	DEFINE_RPC_FAULT_CODE(ERROR_OPERATION_ABORTED)
	DEFINE_RPC_FAULT_CODE(ERROR_BAD_ARGUMENTS)
	DEFINE_RPC_FAULT_CODE(E_PROXY_INTERNALERROR)
	DEFINE_RPC_FAULT_CODE(E_PROXY_RAP_ACCESSDENIED)
	DEFINE_RPC_FAULT_CODE(E_PROXY_NAP_ACCESSDENIED)
	DEFINE_RPC_FAULT_CODE(E_PROXY_TS_CONNECTFAILED)
	DEFINE_RPC_FAULT_CODE(E_PROXY_ALREADYDISCONNECTED)
	DEFINE_RPC_FAULT_CODE(E_PROXY_QUARANTINE_ACCESSDENIED)
	DEFINE_RPC_FAULT_CODE(E_PROXY_NOCERTAVAILABLE)
	DEFINE_RPC_FAULT_CODE(E_PROXY_COOKIE_BADPACKET)
	DEFINE_RPC_FAULT_CODE(E_PROXY_COOKIE_AUTHENTICATION_ACCESS_DENIED)
	DEFINE_RPC_FAULT_CODE(E_PROXY_UNSUPPORTED_AUTHENTICATION_METHOD)
	DEFINE_RPC_FAULT_CODE(E_PROXY_CAPABILITYMISMATCH)
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_NOTSUPPORTED))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_TS_CONNECTFAILED))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_MAXCONNECTIONSREACHED))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_INTERNALERROR))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_SESSIONTIMEOUT))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_REAUTH_AUTHN_FAILED))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_REAUTH_CAP_FAILED))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_REAUTH_RAP_FAILED))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_SDR_NOT_SUPPORTED_BY_TS))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_REAUTH_NAP_FAILED))
	DEFINE_RPC_FAULT_CODE(HRESULT_CODE(E_PROXY_CONNECTIONABORTED))
	DEFINE_RPC_FAULT_CODE(HRESULT_FROM_WIN32(RPC_S_CALL_CANCELLED))
	{ 0, NULL }
};

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
	STREAM* s;
	int status;
	int length;
	rdpTsg* tsg;
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

	s = stream_new(28 + totalDataBytes);

	/* PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE_NR (20 bytes) */
	stream_write(s, &tsg->ChannelContext.ContextType, 4); /* ContextType (4 bytes) */
	stream_write(s, tsg->ChannelContext.ContextUuid, 16); /* ContextUuid (16 bytes) */

	stream_write_UINT32_be(s, totalDataBytes); /* totalDataBytes (4 bytes) */
	stream_write_UINT32_be(s, numBuffers); /* numBuffers (4 bytes) */

	if (buffer1Length > 0)
		stream_write_UINT32_be(s, buffer1Length); /* buffer1Length (4 bytes) */
	if (buffer2Length > 0)
		stream_write_UINT32_be(s, buffer2Length); /* buffer2Length (4 bytes) */
	if (buffer3Length > 0)
		stream_write_UINT32_be(s, buffer3Length); /* buffer3Length (4 bytes) */

	if (buffer1Length > 0)
		stream_write(s, buffer1, buffer1Length); /* buffer1 (variable) */
	if (buffer2Length > 0)
		stream_write(s, buffer2, buffer2Length); /* buffer2 (variable) */
	if (buffer3Length > 0)
		stream_write(s, buffer3, buffer3Length); /* buffer3 (variable) */

	stream_seal(s);

	length = s->size;
	status = rpc_tsg_write(tsg->rpc, s->data, s->size, TsProxySendToServerOpnum);

	stream_free(s);

	if (status <= 0)
	{
		printf("rpc_tsg_write failed!\n");
		return -1;
	}

	return length;
}

BOOL tsg_proxy_create_tunnel_write_request(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	UINT32 NapCapabilities;
	rdpRpc* rpc = tsg->rpc;

	length = 108;
	buffer = (BYTE*) malloc(length);

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
	 * FIXME: Alternate Code Path
	 *
	 * Using reduced capabilities appears to trigger
	 * TSG_PACKET_TYPE_QUARENC_RESPONSE instead of TSG_PACKET_TYPE_CAPS_RESPONSE
	 */
	NapCapabilities = TSG_NAP_CAPABILITY_IDLE_TIMEOUT;

	*((UINT32*) &buffer[44]) = NapCapabilities; /* capabilities */

	CopyMemory(&buffer[48], TsProxyCreateTunnelUnknownTrailerBytes, 60);

	status = rpc_tsg_write(rpc, buffer, length, TsProxyCreateTunnelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);

	return TRUE;
}

BOOL tsg_proxy_create_tunnel_read_response(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 count;
	UINT32 length;
	UINT32 offset;
	UINT32 Pointer;
	PTSG_PACKET packet;
	UINT32 SwitchValue;
	rdpRpc* rpc = tsg->rpc;
	PTSG_PACKET_CAPABILITIES tsgCaps;
	PTSG_PACKET_VERSIONCAPS versionCaps;
	PTSG_PACKET_CAPS_RESPONSE packetCapsResponse;
	PTSG_PACKET_QUARENC_RESPONSE packetQuarEncResponse;

	status = rpc_recv_pdu(rpc);

	if (status <= 0)
		return FALSE;

	length = status;
	buffer = rpc->buffer;

	packet = (PTSG_PACKET) malloc(sizeof(TSG_PACKET));
	ZeroMemory(packet, sizeof(TSG_PACKET));

	packet->packetId = *((UINT32*) &buffer[28]); /* PacketId */
	SwitchValue = *((UINT32*) &buffer[32]); /* SwitchValue */

	if ((packet->packetId == TSG_PACKET_TYPE_CAPS_RESPONSE) && (SwitchValue == TSG_PACKET_TYPE_CAPS_RESPONSE))
	{
		packetCapsResponse = (PTSG_PACKET_CAPS_RESPONSE) malloc(sizeof(TSG_PACKET_CAPS_RESPONSE));
		ZeroMemory(packetCapsResponse, sizeof(TSG_PACKET_CAPS_RESPONSE));
		packet->tsgPacket.packetCapsResponse = packetCapsResponse;

		/* PacketQuarResponsePtr (4 bytes) */
		packetCapsResponse->pktQuarEncResponse.flags = *((UINT32*) &buffer[40]); /* Flags */
		packetCapsResponse->pktQuarEncResponse.certChainLen = *((UINT32*) &buffer[44]); /* CertChainLength */
		/* CertChainDataPtr (4 bytes) */
		CopyMemory(&packetCapsResponse->pktQuarEncResponse.nonce, &buffer[52], 16); /* Nonce */
		offset = 68;

		Pointer = *((UINT32*) &buffer[offset]); /* Ptr */
		offset += 4;

		if (Pointer == 0x0002000C)
		{
			/* Not sure exactly what this is */
			offset += 4; /* 0x00000001 (4 bytes) */
			offset += 4; /* 0x00000001 (4 bytes) */
			offset += 4; /* 0x00000000 (4 bytes) */
			offset += 4; /* 0x00000001 (4 bytes) */
		}

		Pointer = *((UINT32*) &buffer[offset]); /* Ptr (4 bytes): 0x00020014 */
		offset += 4;

		offset += 4; /* MaxCount (4 bytes) */
		offset += 4; /* Offset (4 bytes) */
		count = *((UINT32*) &buffer[offset]); /* ActualCount (4 bytes) */
		offset += 4;

		/*
		 * CertChainData is a wide character string, and the count is
		 * given in characters excluding the null terminator, therefore:
		 * size = ((count + 1) * 2)
		 */
		offset += ((count + 1) * 2); /* CertChainData */

		versionCaps = (PTSG_PACKET_VERSIONCAPS) malloc(sizeof(TSG_PACKET_VERSIONCAPS));
		ZeroMemory(versionCaps, sizeof(TSG_PACKET_VERSIONCAPS));
		packetCapsResponse->pktQuarEncResponse.versionCaps = versionCaps;

		versionCaps->tsgHeader.ComponentId = *((UINT16*) &buffer[offset]); /* ComponentId */
		versionCaps->tsgHeader.PacketId = *((UINT16*) &buffer[offset + 2]); /* PacketId */
		offset += 4;

		if (versionCaps->tsgHeader.ComponentId != TS_GATEWAY_TRANSPORT)
		{
			printf("Unexpected ComponentId: 0x%04X\n", versionCaps->tsgHeader.ComponentId);
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

		tsgCaps = (PTSG_PACKET_CAPABILITIES) malloc(sizeof(TSG_PACKET_CAPABILITIES));
		ZeroMemory(tsgCaps, sizeof(TSG_PACKET_CAPABILITIES));
		versionCaps->tsgCaps = tsgCaps;

		offset += 4; /* MaxCount (4 bytes) */
		tsgCaps->capabilityType = *((UINT32*) &buffer[offset]); /* CapabilityType */
		SwitchValue = *((UINT32*) &buffer[offset + 4]); /* SwitchValue */
		offset += 8;

		if ((SwitchValue != TSG_CAPABILITY_TYPE_NAP) ||
				(tsgCaps->capabilityType != TSG_CAPABILITY_TYPE_NAP))
		{
			printf("Unexpected CapabilityType: 0x%08X, Expected TSG_CAPABILITY_TYPE_NAP\n",
					tsgCaps->capabilityType);
			return FALSE;
		}

		tsgCaps->tsgPacket.tsgCapNap.capabilities = *((UINT32*) &buffer[offset]); /* Capabilities */
		offset += 4;

		/* ??? (16 bytes): all zeros */
		offset += 16;

		/* TunnelContext (20 bytes) */
		CopyMemory(&tsg->TunnelContext.ContextType, &buffer[offset], 4); /* ContextType */
		CopyMemory(tsg->TunnelContext.ContextUuid, &buffer[offset + 4], 16); /* ContextUuid */
		offset += 20;

		/* TODO: trailing bytes */

#ifdef WITH_DEBUG_TSG
		printf("TSG TunnelContext:\n");
		freerdp_hexdump((void*) &tsg->TunnelContext, 20);
		printf("\n");
#endif

		free(tsgCaps);
		free(versionCaps);
		free(packetCapsResponse);
	}
	else if ((packet->packetId == TSG_PACKET_TYPE_QUARENC_RESPONSE) && (SwitchValue == TSG_PACKET_TYPE_QUARENC_RESPONSE))
	{
		packetQuarEncResponse = (PTSG_PACKET_QUARENC_RESPONSE) malloc(sizeof(TSG_PACKET_QUARENC_RESPONSE));
		ZeroMemory(packetQuarEncResponse, sizeof(TSG_PACKET_QUARENC_RESPONSE));
		packet->tsgPacket.packetQuarEncResponse = packetQuarEncResponse;

		/* PacketQuarResponsePtr (4 bytes) */
		packetQuarEncResponse->flags = *((UINT32*) &buffer[40]); /* Flags */
		packetQuarEncResponse->certChainLen = *((UINT32*) &buffer[44]); /* CertChainLength */
		/* CertChainDataPtr (4 bytes) */
		CopyMemory(&packetQuarEncResponse->nonce, &buffer[52], 16); /* Nonce */
		offset = 68;

		Pointer = *((UINT32*) &buffer[offset]); /* Ptr (4 bytes): 0x0002000C */
		offset += 4;

		offset += 4; /* MaxCount (4 bytes) */
		offset += 4; /* Offset (4 bytes) */
		count = *((UINT32*) &buffer[offset]); /* ActualCount (4 bytes) */
		offset += 4;

		/*
		 * CertChainData is a wide character string, and the count is
		 * given in characters excluding the null terminator, therefore:
		 * size = ((count + 1) * 2)
		 */
		offset += ((count + 1) * 2); /* CertChainData */

		versionCaps = (PTSG_PACKET_VERSIONCAPS) malloc(sizeof(TSG_PACKET_VERSIONCAPS));
		ZeroMemory(versionCaps, sizeof(TSG_PACKET_VERSIONCAPS));
		packetQuarEncResponse->versionCaps = versionCaps;

		versionCaps->tsgHeader.ComponentId = *((UINT16*) &buffer[offset]); /* ComponentId */
		versionCaps->tsgHeader.PacketId = *((UINT16*) &buffer[offset + 2]); /* PacketId */
		offset += 4;

		if (versionCaps->tsgHeader.ComponentId != TS_GATEWAY_TRANSPORT)
		{
			printf("Unexpected ComponentId: 0x%04X\n", versionCaps->tsgHeader.ComponentId);
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

		/* TODO: trailing bytes */

#ifdef WITH_DEBUG_TSG
		printf("TSG TunnelContext:\n");
		freerdp_hexdump((void*) &tsg->TunnelContext, 20);
		printf("\n");
#endif

		free(versionCaps);
		free(packetQuarEncResponse);
	}
	else
	{
		printf("Unexpected PacketId: 0x%08X, Expected TSG_PACKET_TYPE_CAPS_RESPONSE "
				"or TSG_PACKET_TYPE_QUARENC_RESPONSE\n", packet->packetId);
		return FALSE;
	}

	free(packet);

	return TRUE;
}

BOOL tsg_proxy_create_tunnel(rdpTsg* tsg)
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

	DEBUG_TSG("TsProxyCreateTunnel");

	if (!tsg_proxy_create_tunnel_write_request(tsg))
	{
		printf("TsProxyCreateTunnel: error writing request\n");
		return FALSE;
	}

	if (!tsg_proxy_create_tunnel_read_response(tsg))
	{
		printf("TsProxyCreateTunnel: error reading response\n");
		return FALSE;
	}

	return TRUE;
}

BOOL tsg_proxy_authorize_tunnel_write_request(rdpTsg* tsg)
{
	UINT32 pad;
	int status;
	BYTE* buffer;
	UINT32 count;
	UINT32 length;
	UINT32 offset;
	rdpRpc* rpc = tsg->rpc;

	count = _wcslen(tsg->MachineName) + 1;

	offset = 64 + (count * 2);
	rpc_offset_align(&offset, 4);
	offset += 4;

	length = offset;
	buffer = (BYTE*) malloc(length);

	/* TunnelContext */
	CopyMemory(&buffer[0], &tsg->TunnelContext.ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], tsg->TunnelContext.ContextUuid, 16); /* ContextUuid */

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

	status = rpc_tsg_write(rpc, buffer, length, TsProxyAuthorizeTunnelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);

	return TRUE;
}

BOOL tsg_proxy_authorize_tunnel_read_response(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;
	UINT32 Pointer;
	UINT32 SizeValue;
	UINT32 SwitchValue;
	PTSG_PACKET packet;
	rdpRpc* rpc = tsg->rpc;
	PTSG_PACKET_RESPONSE packetResponse;

	status = rpc_recv_pdu(rpc);

	if (status <= 0)
		return FALSE;

	length = status;
	buffer = rpc->buffer;

	packet = (PTSG_PACKET) malloc(sizeof(TSG_PACKET));
	ZeroMemory(packet, sizeof(TSG_PACKET));

	packet->packetId = *((UINT32*) &buffer[28]); /* PacketId */
	SwitchValue = *((UINT32*) &buffer[32]); /* SwitchValue */

	if ((packet->packetId != TSG_PACKET_TYPE_RESPONSE) ||
			(SwitchValue != TSG_PACKET_TYPE_RESPONSE))
	{
		printf("Unexpected PacketId: 0x%08X, Expected TSG_PACKET_TYPE_RESPONSE\n", packet->packetId);
		return FALSE;
	}

	packetResponse = (PTSG_PACKET_RESPONSE) malloc(sizeof(TSG_PACKET_RESPONSE));
	ZeroMemory(packetResponse, sizeof(TSG_PACKET_RESPONSE));
	packet->tsgPacket.packetResponse = packetResponse;

	Pointer = *((UINT32*) &buffer[36]); /* PacketResponsePtr */
	packetResponse->flags = *((UINT32*) &buffer[40]); /* Flags */

	if (packetResponse->flags != TSG_PACKET_TYPE_QUARREQUEST)
	{
		printf("Unexpected Packet Response Flags: 0x%08X, Expected TSG_PACKET_TYPE_QUARREQUEST\n",
				packetResponse->flags);
		return FALSE;
	}

	/* Reserved (4 bytes) */
	Pointer = *((UINT32*) &buffer[48]); /* ResponseDataPtr */
	packetResponse->responseDataLen = *((UINT32*) &buffer[52]); /* ResponseDataLength */

	packetResponse->redirectionFlags.enableAllRedirections = *((UINT32*) &buffer[56]); /* EnableAllRedirections */
	packetResponse->redirectionFlags.disableAllRedirections = *((UINT32*) &buffer[60]); /* DisableAllRedirections */
	packetResponse->redirectionFlags.driveRedirectionDisabled = *((UINT32*) &buffer[64]); /* DriveRedirectionDisabled */
	packetResponse->redirectionFlags.printerRedirectionDisabled = *((UINT32*) &buffer[68]); /* PrinterRedirectionDisabled */
	packetResponse->redirectionFlags.portRedirectionDisabled = *((UINT32*) &buffer[72]); /* PortRedirectionDisabled */
	packetResponse->redirectionFlags.reserved = *((UINT32*) &buffer[76]); /* Reserved */
	packetResponse->redirectionFlags.clipboardRedirectionDisabled = *((UINT32*) &buffer[80]); /* ClipboardRedirectionDisabled */
	packetResponse->redirectionFlags.pnpRedirectionDisabled = *((UINT32*) &buffer[84]); /* PnpRedirectionDisabled */
	offset = 88;

	SizeValue = *((UINT32*) &buffer[offset]);
	offset += 4;

	if (SizeValue != packetResponse->responseDataLen)
	{
		printf("Unexpected size value: %d, expected: %d\n", SizeValue, packetResponse->responseDataLen);
		return FALSE;
	}

	offset += SizeValue; /* ResponseData */

	/* TODO: trailing bytes */

	free(packetResponse);
	free(packet);

	return TRUE;
}

BOOL tsg_proxy_authorize_tunnel(rdpTsg* tsg)
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

	DEBUG_TSG("TsProxyAuthorizeTunnel");

	if (!tsg_proxy_authorize_tunnel_write_request(tsg))
	{
		printf("TsProxyAuthorizeTunnel: error writing request\n");
		return FALSE;
	}

	if (!tsg_proxy_authorize_tunnel_read_response(tsg))
	{
		printf("TsProxyAuthorizeTunnel: error reading response\n");
		return FALSE;
	}

	return TRUE;
}

BOOL tsg_proxy_make_tunnel_call_write_request(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	length = 40;
	buffer = (BYTE*) malloc(length);

	/* TunnelContext */
	CopyMemory(&buffer[0], &tsg->TunnelContext.ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], tsg->TunnelContext.ContextUuid, 16); /* ContextUuid */

	*((UINT32*) &buffer[20]) = TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST; /* ProcId */

	/* 4-byte alignment */

	*((UINT32*) &buffer[24]) = TSG_PACKET_TYPE_MSGREQUEST_PACKET; /* PacketId */
	*((UINT32*) &buffer[28]) = TSG_PACKET_TYPE_MSGREQUEST_PACKET; /* SwitchValue */

	*((UINT32*) &buffer[32]) = 0x00020000; /* PacketMsgRequestPtr */

	*((UINT32*) &buffer[36]) = 0x00000001; /* MaxMessagesPerBatch */

	status = rpc_tsg_write(rpc, buffer, length, TsProxyMakeTunnelCallOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);

	return TRUE;
}

BOOL tsg_proxy_make_tunnel_call_read_response(rdpTsg* tsg)
{
	/* ??? */

	return TRUE;
}

BOOL tsg_proxy_make_tunnel_call(rdpTsg* tsg)
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

	DEBUG_TSG("TsProxyMakeTunnelCall");

	if (!tsg_proxy_make_tunnel_call_write_request(tsg))
	{
		printf("TsProxyMakeTunnelCall: error writing request\n");
		return FALSE;
	}

	if (!tsg_proxy_make_tunnel_call_read_response(tsg))
	{
		printf("TsProxyMakeTunnelCall: error reading response\n");
		return FALSE;
	}

	return TRUE;
}

BOOL tsg_proxy_create_channel_write_request(rdpTsg* tsg)
{
	int status;
	UINT32 count;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	count = _wcslen(tsg->Hostname) + 1;

#ifdef WITH_DEBUG_TSG
	printf("ResourceName:\n");
	freerdp_hexdump((BYTE*) tsg->Hostname, (count - 1) * 2);
	printf("\n");
#endif

	length = 60 + (count * 2);

	buffer = (BYTE*) malloc(length);

	/* TunnelContext */
	CopyMemory(&buffer[0], &tsg->TunnelContext.ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], tsg->TunnelContext.ContextUuid, 16); /* ContextUuid */

	/* TSENDPOINTINFO */

	*((UINT32*) &buffer[20]) = 0x00020000; /* ResourceNamePtr */
	*((UINT32*) &buffer[24]) = 0x00000001; /* NumResourceNames */
	*((UINT32*) &buffer[28]) = 0x00000000; /* AlternateResourceNamesPtr */
	*((UINT32*) &buffer[32]) = 0x00000000; /* NumAlternateResourceNames */

	*((UINT16*) &buffer[36]) = 0x0003; /* ??? */

	*((UINT16*) &buffer[38]) = tsg->Port; /* Port */

	*((UINT32*) &buffer[40]) = 0x00000001; /* ??? */

	*((UINT32*) &buffer[44]) = 0x00020004; /* ResourceNamePtr */
	*((UINT32*) &buffer[48]) = count; /* MaxCount */
	*((UINT32*) &buffer[52]) = 0; /* Offset */
	*((UINT32*) &buffer[56]) = count; /* ActualCount */
	CopyMemory(&buffer[60], tsg->Hostname, count * 2); /* Array */

	status = rpc_tsg_write(rpc, buffer, length, TsProxyCreateChannelOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);

	return TRUE;
}

BOOL tsg_proxy_create_channel_read_response(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	status = rpc_recv_pdu(rpc);

	if (status < 0)
		return FALSE;

	length = status;
	buffer = rpc->buffer;

	/* ChannelContext (20 bytes) */
	CopyMemory(&tsg->ChannelContext.ContextType, &rpc->buffer[24], 4); /* ContextType (4 bytes) */
	CopyMemory(tsg->ChannelContext.ContextUuid, &rpc->buffer[28], 16); /* ContextUuid (16 bytes) */

	/* TODO: trailing bytes */

#ifdef WITH_DEBUG_TSG
	printf("ChannelContext:\n");
	freerdp_hexdump((void*) &tsg->ChannelContext, 20);
	printf("\n");
#endif

	return TRUE;
}

BOOL tsg_proxy_create_channel(rdpTsg* tsg)
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

	DEBUG_TSG("TsProxyCreateChannel");

	if (!tsg_proxy_create_channel_write_request(tsg))
	{
		printf("TsProxyCreateChannel: error writing request\n");
		return FALSE;
	}

	if (!tsg_proxy_create_channel_read_response(tsg))
	{
		printf("TsProxyCreateChannel: error reading response\n");
		return FALSE;
	}

	return TRUE;
}

BOOL tsg_proxy_setup_receive_pipe_write_request(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	length = 20;

	buffer = (BYTE*) malloc(length);

	/* ChannelContext */
	CopyMemory(&buffer[0], &tsg->ChannelContext.ContextType, 4); /* ContextType */
	CopyMemory(&buffer[4], tsg->ChannelContext.ContextUuid, 16); /* ContextUuid */

	status = rpc_tsg_write(rpc, buffer, length, TsProxySetupReceivePipeOpnum);

	if (status <= 0)
		return FALSE;

	free(buffer);

	return TRUE;
}

BOOL tsg_proxy_setup_receive_pipe_read_response(rdpTsg* tsg)
{
#if 0
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	status = rpc_recv_pdu(rpc);

	if (status < 0)
		return FALSE;

	length = status;
	buffer = rpc->buffer;
#endif

	return TRUE;
}

BOOL tsg_proxy_setup_receive_pipe(rdpTsg* tsg)
{
	/**
	 * OpNum = 8
	 *
	 * DWORD TsProxySetupReceivePipe(
	 * [in, max_is(32767)] byte pRpcMessage[]
	 * );
	 */

	DEBUG_TSG("TsProxySetupReceivePipe");

	if (!tsg_proxy_setup_receive_pipe_write_request(tsg))
	{
		printf("TsProxySetupReceivePipe: error writing request\n");
		return FALSE;
	}

	if (!tsg_proxy_setup_receive_pipe_read_response(tsg))
	{
		printf("TsProxySetupReceivePipe: error reading response\n");
		return FALSE;
	}

	return TRUE;
}

BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port)
{
	rdpRpc* rpc = tsg->rpc;
	rdpSettings* settings = rpc->settings;

	tsg->Port = port;
	freerdp_AsciiToUnicodeAlloc(hostname, &tsg->Hostname, 0);
	freerdp_AsciiToUnicodeAlloc(settings->ComputerName, &tsg->MachineName, 0);

	if (!rpc_connect(rpc))
	{
		printf("rpc_connect failed!\n");
		return FALSE;
	}

	DEBUG_TSG("rpc_connect success");

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

	if (!tsg_proxy_create_tunnel(tsg))
		return FALSE;

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

	if (!tsg_proxy_authorize_tunnel(tsg))
		return FALSE;

	/**
	 *     Sequential processing rules for connection process (continued):
	 *
	 * 13. If the ADM element Negotiated Capabilities contains TSG_MESSAGING_CAP_SERVICE_MSG, a TsProxyMakeTunnelCall call MAY be
	 *     made by the client, with TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST as the parameter, to receive messages from the RDG server.
	 *
	 */

	if (!tsg_proxy_make_tunnel_call(tsg))
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

	if (!tsg_proxy_create_channel(tsg))
		return FALSE;

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

	if (!tsg_proxy_setup_receive_pipe(tsg))
		return FALSE;

	return TRUE;
}

int tsg_read(rdpTsg* tsg, BYTE* data, UINT32 length)
{
	int status;
	int copyLength;
	RPC_PDU_HEADER* header;
	rdpRpc* rpc = tsg->rpc;

	printf("tsg_read: %d, pending: %d\n", length, tsg->pendingPdu);

	if (tsg->pendingPdu)
	{
		header = (RPC_PDU_HEADER*) rpc->buffer;

		copyLength = (tsg->bytesAvailable > length) ? length : tsg->bytesAvailable;

		CopyMemory(data, &rpc->buffer[tsg->bytesRead], copyLength);
		tsg->bytesAvailable -= copyLength;
		tsg->bytesRead += copyLength;

		if (tsg->bytesAvailable < 1)
			tsg->pendingPdu = FALSE;

		return copyLength;
	}
	else
	{
		status = rpc_recv_pdu(rpc);
		tsg->pendingPdu = TRUE;

		header = (RPC_PDU_HEADER*) rpc->buffer;
		tsg->bytesAvailable = header->frag_length;
		tsg->bytesRead = 0;

		copyLength = (tsg->bytesAvailable > length) ? length : tsg->bytesAvailable;

		CopyMemory(data, &rpc->buffer[tsg->bytesRead], copyLength);
		tsg->bytesAvailable -= copyLength;
		tsg->bytesRead += copyLength;

		return copyLength;
	}
}

int tsg_write(rdpTsg* tsg, BYTE* data, UINT32 length)
{
	return TsProxySendToServer((handle_t) tsg, data, 1, &length);
}

rdpTsg* tsg_new(rdpTransport* transport)
{
	rdpTsg* tsg;

	tsg = xnew(rdpTsg);

	if (tsg != NULL)
	{
		tsg->transport = transport;
		tsg->settings = transport->settings;
		tsg->rpc = rpc_new(tsg->transport);
		tsg->pendingPdu = FALSE;
	}

	return tsg;
}

void tsg_free(rdpTsg* tsg)
{
	if (tsg != NULL)
	{
		rpc_free(tsg->rpc);
		free(tsg);
	}
}
