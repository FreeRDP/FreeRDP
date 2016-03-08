/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Request To Send (RTS) PDUs
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/crypto.h>
#include <winpr/winhttp.h>

#include <freerdp/log.h>

#include "ncacn_http.h"
#include "rpc_client.h"

#include "rts.h"

#define TAG FREERDP_TAG("core.gateway.rts")

const char* const RTS_CMD_STRINGS[] =
{
	"ReceiveWindowSize",
	"FlowControlAck",
	"ConnectionTimeout",
	"Cookie",
	"ChannelLifetime",
	"ClientKeepalive",
	"Version",
	"Empty",
	"Padding",
	"NegativeANCE",
	"ANCE",
	"ClientAddress",
	"AssociationGroupId",
	"Destination",
	"PingTrafficSentNotify"
};

/**
 * RTS PDU Header
 *
 * The RTS PDU Header has the same layout as the common header of the connection-oriented RPC
 * PDU as specified in [C706] section 12.6.1, with a few additional requirements around the contents
 * of the header fields. The additional requirements are as follows:
 *
 * All fields MUST use little-endian byte order.
 *
 * Fragmentation MUST NOT occur for an RTS PDU.
 *
 * PFC_FIRST_FRAG and PFC_LAST_FRAG MUST be present in all RTS PDUs, and all other PFC flags
 * MUST NOT be present.
 *
 * The rpc_vers and rpc_vers_minor fields MUST contain version information as described in
 * [MS-RPCE] section 1.7.
 *
 * PTYPE MUST be set to a value of 20 (0x14). This field differentiates RTS packets from other RPC packets.
 *
 * The packed_drep MUST indicate little-endian integer and floating-pointer byte order, IEEE float-point
 * format representation, and ASCII character format as specified in [C706] section 12.6.
 *
 * The auth_length MUST be set to 0.
 *
 * The frag_length field MUST reflect the size of the header plus the size of all commands, including
 * the variable portion of variable-sized commands.
 *
 * The call_id MUST be set to 0 by senders and MUST be 0 on receipt.
 *
 */

void rts_pdu_header_init(rpcconn_rts_hdr_t* header)
{
	ZeroMemory(header, sizeof(*header));
	header->rpc_vers = 5;
	header->rpc_vers_minor = 0;
	header->ptype = PTYPE_RTS;
	header->packed_drep[0] = 0x10;
	header->packed_drep[1] = 0x00;
	header->packed_drep[2] = 0x00;
	header->packed_drep[3] = 0x00;
	header->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
	header->auth_length = 0;
	header->call_id = 0;
}

int rts_receive_window_size_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length, UINT32* ReceiveWindowSize)
{
	if (ReceiveWindowSize)
		*ReceiveWindowSize = *((UINT32*) &buffer[0]); /* ReceiveWindowSize (4 bytes) */

	return 4;
}

int rts_receive_window_size_command_write(BYTE* buffer, UINT32 ReceiveWindowSize)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_RECEIVE_WINDOW_SIZE; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = ReceiveWindowSize; /* ReceiveWindowSize (4 bytes) */
	}

	return 8;
}

int rts_flow_control_ack_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length,
		UINT32* BytesReceived, UINT32* AvailableWindow, BYTE* ChannelCookie)
{
	/* Ack (24 bytes) */

	if (BytesReceived)
		*BytesReceived = *((UINT32*) &buffer[0]); /* BytesReceived (4 bytes) */

	if (AvailableWindow)
		*AvailableWindow = *((UINT32*) &buffer[4]); /* AvailableWindow (4 bytes) */

	if (ChannelCookie)
		CopyMemory(ChannelCookie, &buffer[8], 16); /* ChannelCookie (16 bytes) */

	return 24;
}

int rts_flow_control_ack_command_write(BYTE* buffer, UINT32 BytesReceived, UINT32 AvailableWindow, BYTE* ChannelCookie)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_FLOW_CONTROL_ACK; /* CommandType (4 bytes) */

		/* Ack (24 bytes) */
		*((UINT32*) &buffer[4]) = BytesReceived; /* BytesReceived (4 bytes) */
		*((UINT32*) &buffer[8]) = AvailableWindow; /* AvailableWindow (4 bytes) */
		CopyMemory(&buffer[12], ChannelCookie, 16); /* ChannelCookie (16 bytes) */
	}

	return 28;
}

int rts_connection_timeout_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length, UINT32* ConnectionTimeout)
{
	if (ConnectionTimeout)
		*ConnectionTimeout = *((UINT32*) &buffer[0]); /* ConnectionTimeout (4 bytes) */

	return 4;
}

int rts_connection_timeout_command_write(BYTE* buffer, UINT32 ConnectionTimeout)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_CONNECTION_TIMEOUT; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = ConnectionTimeout; /* ConnectionTimeout (4 bytes) */
	}

	return 8;
}

int rts_cookie_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* Cookie (16 bytes) */

	return 16;
}

int rts_cookie_command_write(BYTE* buffer, BYTE* Cookie)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_COOKIE; /* CommandType (4 bytes) */
		CopyMemory(&buffer[4], Cookie, 16); /* Cookie (16 bytes) */
	}

	return 20;
}

int rts_channel_lifetime_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* ChannelLifetime (4 bytes) */

	return 4;
}

int rts_channel_lifetime_command_write(BYTE* buffer, UINT32 ChannelLifetime)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_CHANNEL_LIFETIME; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = ChannelLifetime; /* ChannelLifetime (4 bytes) */
	}

	return 8;
}

int rts_client_keepalive_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* ClientKeepalive (4 bytes) */

	return 4;
}

int rts_client_keepalive_command_write(BYTE* buffer, UINT32 ClientKeepalive)
{
	/**
	 * An unsigned integer that specifies the keep-alive interval, in milliseconds,
	 * that this connection is configured to use. This value MUST be 0 or in the inclusive
	 * range of 60,000 through 4,294,967,295. If it is 0, it MUST be interpreted as 300,000.
	 */

	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_CLIENT_KEEPALIVE; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = ClientKeepalive; /* ClientKeepalive (4 bytes) */
	}

	return 8;
}

int rts_version_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* Version (4 bytes) */

	return 4;
}

int rts_version_command_write(BYTE* buffer)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_VERSION; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = 1; /* Version (4 bytes) */
	}

	return 8;
}

int rts_empty_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	return 0;
}

int rts_empty_command_write(BYTE* buffer)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_EMPTY; /* CommandType (4 bytes) */
	}

	return 4;
}

int rts_padding_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	UINT32 ConformanceCount;

	ConformanceCount = *((UINT32*) &buffer[0]); /* ConformanceCount (4 bytes) */
	/* Padding (variable) */

	return ConformanceCount + 4;
}

int rts_padding_command_write(BYTE* buffer, UINT32 ConformanceCount)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_PADDING; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = ConformanceCount; /* ConformanceCount (4 bytes) */
		ZeroMemory(&buffer[8], ConformanceCount); /* Padding (variable) */
	}

	return 8 + ConformanceCount;
}

int rts_negative_ance_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	return 0;
}

int rts_negative_ance_command_write(BYTE* buffer)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_NEGATIVE_ANCE; /* CommandType (4 bytes) */
	}

	return 4;
}

int rts_ance_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	return 0;
}

int rts_ance_command_write(BYTE* buffer)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_ANCE; /* CommandType (4 bytes) */
	}

	return 4;
}

int rts_client_address_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	UINT32 AddressType;

	AddressType = *((UINT32*) &buffer[0]); /* AddressType (4 bytes) */

	if (AddressType == 0)
	{
		/* ClientAddress (4 bytes) */
		/* padding (12 bytes) */

		return 4 + 4 + 12;
	}
	else
	{
		/* ClientAddress (16 bytes) */
		/* padding (12 bytes) */

		return 4 + 16 + 12;
	}
}

int rts_client_address_command_write(BYTE* buffer, UINT32 AddressType, BYTE* ClientAddress)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_CLIENT_ADDRESS; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = AddressType; /* AddressType (4 bytes) */
	}

	if (AddressType == 0)
	{
		if (buffer)
		{
			CopyMemory(&buffer[8], ClientAddress, 4); /* ClientAddress (4 bytes) */
			ZeroMemory(&buffer[12], 12); /* padding (12 bytes) */
		}

		return 24;
	}
	else
	{
		if (buffer)
		{
			CopyMemory(&buffer[8], ClientAddress, 16); /* ClientAddress (16 bytes) */
			ZeroMemory(&buffer[24], 12); /* padding (12 bytes) */
		}

		return 36;
	}
}

int rts_association_group_id_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* AssociationGroupId (16 bytes) */

	return 16;
}

int rts_association_group_id_command_write(BYTE* buffer, BYTE* AssociationGroupId)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_ASSOCIATION_GROUP_ID; /* CommandType (4 bytes) */
		CopyMemory(&buffer[4], AssociationGroupId, 16); /* AssociationGroupId (16 bytes) */
	}

	return 20;
}

int rts_destination_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length, UINT32* Destination)
{
	if (Destination)
		*Destination = *((UINT32*) &buffer[0]); /* Destination (4 bytes) */

	return 4;
}

int rts_destination_command_write(BYTE* buffer, UINT32 Destination)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_DESTINATION; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = Destination; /* Destination (4 bytes) */
	}

	return 8;
}

int rts_ping_traffic_sent_notify_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* PingTrafficSent (4 bytes) */

	return 4;
}

int rts_ping_traffic_sent_notify_command_write(BYTE* buffer, UINT32 PingTrafficSent)
{
	if (buffer)
	{
		*((UINT32*) &buffer[0]) = RTS_CMD_PING_TRAFFIC_SENT_NOTIFY; /* CommandType (4 bytes) */
		*((UINT32*) &buffer[4]) = PingTrafficSent; /* PingTrafficSent (4 bytes) */
	}

	return 8;
}

void rts_generate_cookie(BYTE* cookie)
{
	winpr_RAND(cookie, 16);
}

/* CONN/A Sequence */

int rts_send_CONN_A1_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* buffer;
	rpcconn_rts_hdr_t header;
	UINT32 ReceiveWindowSize;
	BYTE* OUTChannelCookie;
	BYTE* VirtualConnectionCookie;
	RpcVirtualConnection* connection = rpc->VirtualConnection;
	RpcOutChannel* outChannel = connection->DefaultOutChannel;

	rts_pdu_header_init(&header);
	header.frag_length = 76;
	header.Flags = RTS_FLAG_NONE;
	header.NumberOfCommands = 4;

	WLog_DBG(TAG, "Sending CONN/A1 RTS PDU");

	VirtualConnectionCookie = (BYTE*) &(connection->Cookie);
	OUTChannelCookie = (BYTE*) &(outChannel->Cookie);
	ReceiveWindowSize = outChannel->ReceiveWindow;

	buffer = (BYTE*) malloc(header.frag_length);

	if (!buffer)
		return -1;

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_version_command_write(&buffer[20]); /* Version (8 bytes) */
	rts_cookie_command_write(&buffer[28], VirtualConnectionCookie); /* VirtualConnectionCookie (20 bytes) */
	rts_cookie_command_write(&buffer[48], OUTChannelCookie); /* OUTChannelCookie (20 bytes) */
	rts_receive_window_size_command_write(&buffer[68], ReceiveWindowSize); /* ReceiveWindowSize (8 bytes) */

	status = rpc_out_channel_write(outChannel, buffer, header.frag_length);

	free(buffer);

	return (status > 0) ? 1 : -1;
}

int rts_recv_CONN_A3_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	UINT32 ConnectionTimeout;

	rts_connection_timeout_command_read(rpc, &buffer[24], length - 24, &ConnectionTimeout);

	WLog_DBG(TAG, "Receiving CONN/A3 RTS PDU: ConnectionTimeout: %d", ConnectionTimeout);

	rpc->VirtualConnection->DefaultInChannel->PingOriginator.ConnectionTimeout = ConnectionTimeout;

	return 1;
}

/* CONN/B Sequence */

int rts_send_CONN_B1_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rpcconn_rts_hdr_t header;
	BYTE* INChannelCookie;
	BYTE* AssociationGroupId;
	BYTE* VirtualConnectionCookie;
	RpcVirtualConnection* connection = rpc->VirtualConnection;
	RpcInChannel* inChannel = connection->DefaultInChannel;

	rts_pdu_header_init(&header);
	header.frag_length = 104;
	header.Flags = RTS_FLAG_NONE;
	header.NumberOfCommands = 6;

	WLog_DBG(TAG, "Sending CONN/B1 RTS PDU");

	VirtualConnectionCookie = (BYTE*) &(connection->Cookie);
	INChannelCookie = (BYTE*) &(inChannel->Cookie);
	AssociationGroupId = (BYTE*) &(connection->AssociationGroupId);

	buffer = (BYTE*) malloc(header.frag_length);

	if (!buffer)
		return -1;

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_version_command_write(&buffer[20]); /* Version (8 bytes) */
	rts_cookie_command_write(&buffer[28], VirtualConnectionCookie); /* VirtualConnectionCookie (20 bytes) */
	rts_cookie_command_write(&buffer[48], INChannelCookie); /* INChannelCookie (20 bytes) */
	rts_channel_lifetime_command_write(&buffer[68], rpc->ChannelLifetime); /* ChannelLifetime (8 bytes) */
	rts_client_keepalive_command_write(&buffer[76], rpc->KeepAliveInterval); /* ClientKeepalive (8 bytes) */
	rts_association_group_id_command_write(&buffer[84], AssociationGroupId); /* AssociationGroupId (20 bytes) */

	length = header.frag_length;

	status = rpc_in_channel_write(inChannel, buffer, length);

	free(buffer);

	return (status > 0) ? 1 : -1;
}

/* CONN/C Sequence */

int rts_recv_CONN_C2_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	UINT32 offset;
	UINT32 ReceiveWindowSize;
	UINT32 ConnectionTimeout;

	offset = 24;
	offset += rts_version_command_read(rpc, &buffer[offset], length - offset) + 4;
	offset += rts_receive_window_size_command_read(rpc, &buffer[offset], length - offset, &ReceiveWindowSize) + 4;
	offset += rts_connection_timeout_command_read(rpc, &buffer[offset], length - offset, &ConnectionTimeout) + 4;

	WLog_DBG(TAG, "Receiving CONN/C2 RTS PDU: ConnectionTimeout: %d ReceiveWindowSize: %d",
			ConnectionTimeout, ReceiveWindowSize);

	rpc->VirtualConnection->DefaultInChannel->PingOriginator.ConnectionTimeout = ConnectionTimeout;
	rpc->VirtualConnection->DefaultInChannel->PeerReceiveWindow = ReceiveWindowSize;

	return 1;
}

/* Out-of-Sequence PDUs */

int rts_send_keep_alive_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rpcconn_rts_hdr_t header;
	RpcInChannel* inChannel = rpc->VirtualConnection->DefaultInChannel;

	rts_pdu_header_init(&header);
	header.frag_length = 28;
	header.Flags = RTS_FLAG_OTHER_CMD;
	header.NumberOfCommands = 1;

	WLog_DBG(TAG, "Sending Keep-Alive RTS PDU");

	buffer = (BYTE*) malloc(header.frag_length);

	if (!buffer)
		return -1;

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_client_keepalive_command_write(&buffer[20], rpc->CurrentKeepAliveInterval); /* ClientKeepAlive (8 bytes) */

	length = header.frag_length;

	status = rpc_in_channel_write(inChannel, buffer, length);

	free(buffer);

	return (status > 0) ? 1 : -1;
}

int rts_send_flow_control_ack_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rpcconn_rts_hdr_t header;
	UINT32 BytesReceived;
	UINT32 AvailableWindow;
	BYTE* ChannelCookie;
	RpcVirtualConnection* connection = rpc->VirtualConnection;
	RpcInChannel* inChannel = connection->DefaultInChannel;
	RpcOutChannel* outChannel = connection->DefaultOutChannel;

	rts_pdu_header_init(&header);
	header.frag_length = 56;
	header.Flags = RTS_FLAG_OTHER_CMD;
	header.NumberOfCommands = 2;

	WLog_DBG(TAG, "Sending FlowControlAck RTS PDU");

	BytesReceived = outChannel->BytesReceived;
	AvailableWindow = outChannel->AvailableWindowAdvertised;
	ChannelCookie = (BYTE*) &(outChannel->Cookie);

	outChannel->ReceiverAvailableWindow = outChannel->AvailableWindowAdvertised;

	buffer = (BYTE*) malloc(header.frag_length);

	if (!buffer)
		return -1;

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_destination_command_write(&buffer[20], FDOutProxy); /* Destination Command (8 bytes) */

	/* FlowControlAck Command (28 bytes) */
	rts_flow_control_ack_command_write(&buffer[28], BytesReceived, AvailableWindow, ChannelCookie);

	length = header.frag_length;

	status = rpc_in_channel_write(inChannel, buffer, length);

	free(buffer);

	return (status > 0) ? 1 : -1;
}

int rts_recv_flow_control_ack_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	UINT32 offset;
	UINT32 BytesReceived;
	UINT32 AvailableWindow;
	BYTE ChannelCookie[16];

	offset = 24;
	offset += rts_flow_control_ack_command_read(rpc, &buffer[offset], length - offset,
			&BytesReceived, &AvailableWindow, (BYTE*) &ChannelCookie) + 4;

	WLog_ERR(TAG, "Receiving FlowControlAck RTS PDU: BytesReceived: %d AvailableWindow: %d",
			BytesReceived, AvailableWindow);

	rpc->VirtualConnection->DefaultInChannel->SenderAvailableWindow =
		AvailableWindow - (rpc->VirtualConnection->DefaultInChannel->BytesSent - BytesReceived);

	return 1;
}

int rts_recv_flow_control_ack_with_destination_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	UINT32 offset;
	UINT32 Destination;
	UINT32 BytesReceived;
	UINT32 AvailableWindow;
	BYTE ChannelCookie[16];

	/**
	 * When the sender receives a FlowControlAck RTS PDU, it MUST use the following formula to
	 * recalculate its Sender AvailableWindow variable:
	 *
	 * Sender AvailableWindow = Receiver AvailableWindow_from_ack - (BytesSent - BytesReceived_from_ack)
	 *
	 * Where:
	 *
	 * Receiver AvailableWindow_from_ack is the Available Window field in the Flow Control
	 * Acknowledgement Structure (section 2.2.3.4) in the PDU received.
	 *
	 * BytesReceived_from_ack is the Bytes Received field in the Flow Control Acknowledgement structure
	 * in the PDU received.
	 *
	 */

	offset = 24;
	offset += rts_destination_command_read(rpc, &buffer[offset], length - offset, &Destination) + 4;
	offset += rts_flow_control_ack_command_read(rpc, &buffer[offset], length - offset,
			&BytesReceived, &AvailableWindow, (BYTE*) &ChannelCookie) + 4;

	WLog_DBG(TAG, "Receiving FlowControlAckWithDestination RTS PDU: BytesReceived: %d AvailableWindow: %d",
			BytesReceived, AvailableWindow);

	rpc->VirtualConnection->DefaultInChannel->SenderAvailableWindow =
		AvailableWindow - (rpc->VirtualConnection->DefaultInChannel->BytesSent - BytesReceived);

	return 1;
}

int rts_send_ping_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rpcconn_rts_hdr_t header;
	RpcInChannel* inChannel = rpc->VirtualConnection->DefaultInChannel;

	rts_pdu_header_init(&header);
	header.frag_length = 20;
	header.Flags = RTS_FLAG_PING;
	header.NumberOfCommands = 0;

	WLog_DBG(TAG, "Sending Ping RTS PDU");

	buffer = (BYTE*) malloc(header.frag_length);

	if (!buffer)
		return -1;

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */

	length = header.frag_length;

	status = rpc_in_channel_write(inChannel, buffer, length);

	free(buffer);

	return (status > 0) ? 1 : -1;
}

int rts_command_length(rdpRpc* rpc, UINT32 CommandType, BYTE* buffer, UINT32 length)
{
	int CommandLength = 0;

	switch (CommandType)
	{
		case RTS_CMD_RECEIVE_WINDOW_SIZE:
			CommandLength = RTS_CMD_RECEIVE_WINDOW_SIZE_LENGTH;
			break;

		case RTS_CMD_FLOW_CONTROL_ACK:
			CommandLength = RTS_CMD_FLOW_CONTROL_ACK_LENGTH;
			break;

		case RTS_CMD_CONNECTION_TIMEOUT:
			CommandLength = RTS_CMD_CONNECTION_TIMEOUT_LENGTH;
			break;

		case RTS_CMD_COOKIE:
			CommandLength = RTS_CMD_COOKIE_LENGTH;
			break;

		case RTS_CMD_CHANNEL_LIFETIME:
			CommandLength = RTS_CMD_CHANNEL_LIFETIME_LENGTH;
			break;

		case RTS_CMD_CLIENT_KEEPALIVE:
			CommandLength =  RTS_CMD_CLIENT_KEEPALIVE_LENGTH;
			break;

		case RTS_CMD_VERSION:
			CommandLength = RTS_CMD_VERSION_LENGTH;
			break;

		case RTS_CMD_EMPTY:
			CommandLength = RTS_CMD_EMPTY_LENGTH;
			break;

		case RTS_CMD_PADDING: /* variable-size */
			CommandLength = rts_padding_command_read(rpc, buffer, length);
			break;

		case RTS_CMD_NEGATIVE_ANCE:
			CommandLength = RTS_CMD_NEGATIVE_ANCE_LENGTH;
			break;

		case RTS_CMD_ANCE:
			CommandLength = RTS_CMD_ANCE_LENGTH;
			break;

		case RTS_CMD_CLIENT_ADDRESS: /* variable-size */
			CommandLength = rts_client_address_command_read(rpc, buffer, length);
			break;

		case RTS_CMD_ASSOCIATION_GROUP_ID:
			CommandLength = RTS_CMD_ASSOCIATION_GROUP_ID_LENGTH;
			break;

		case RTS_CMD_DESTINATION:
			CommandLength = RTS_CMD_DESTINATION_LENGTH;
			break;

		case RTS_CMD_PING_TRAFFIC_SENT_NOTIFY:
			CommandLength = RTS_CMD_PING_TRAFFIC_SENT_NOTIFY_LENGTH;
			break;

		default:
			WLog_ERR(TAG, "Error: Unknown RTS Command Type: 0x%x", CommandType);
			return -1;
			break;
	}

	return CommandLength;
}

int rts_send_OUT_R2_A7_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* buffer;
	rpcconn_rts_hdr_t header;
	BYTE* SuccessorChannelCookie;
	RpcInChannel* inChannel = rpc->VirtualConnection->DefaultInChannel;
	RpcOutChannel* nextOutChannel = rpc->VirtualConnection->NonDefaultOutChannel;

	rts_pdu_header_init(&header);
	header.frag_length = 56;
	header.Flags = RTS_FLAG_OUT_CHANNEL;
	header.NumberOfCommands = 3;

	WLog_DBG(TAG, "Sending OUT_R2/A7 RTS PDU");

	SuccessorChannelCookie = (BYTE*) &(nextOutChannel->Cookie);

	buffer = (BYTE*) malloc(header.frag_length);

	if (!buffer)
		return -1;

	CopyMemory(buffer, ((BYTE*)&header), 20); /* RTS Header (20 bytes) */
	rts_destination_command_write(&buffer[20], FDServer); /* Destination (8 bytes)*/
	rts_cookie_command_write(&buffer[28], SuccessorChannelCookie); /* SuccessorChannelCookie (20 bytes) */
	rts_version_command_write(&buffer[48]); /* Version (8 bytes) */

	status = rpc_in_channel_write(inChannel, buffer, header.frag_length);

	free(buffer);

	return (status > 0) ? 1 : -1;
}

int rts_send_OUT_R2_C1_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* buffer;
	rpcconn_rts_hdr_t header;
	RpcOutChannel* nextOutChannel = rpc->VirtualConnection->NonDefaultOutChannel;

	rts_pdu_header_init(&header);
	header.frag_length = 24;
	header.Flags = RTS_FLAG_PING;
	header.NumberOfCommands = 1;

	WLog_DBG(TAG, "Sending OUT_R2/C1 RTS PDU");

	buffer = (BYTE*) malloc(header.frag_length);

	if (!buffer)
		return -1;

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_empty_command_write(&buffer[20]); /* Empty command (4 bytes) */

	status = rpc_out_channel_write(nextOutChannel, buffer, header.frag_length);

	free(buffer);

	return (status > 0) ? 1 : -1;
}

int rts_send_OUT_R1_A3_pdu(rdpRpc* rpc)
{
	int status;
	BYTE* buffer;
	rpcconn_rts_hdr_t header;
	UINT32 ReceiveWindowSize;
	BYTE* VirtualConnectionCookie;
	BYTE* PredecessorChannelCookie;
	BYTE* SuccessorChannelCookie;
	RpcVirtualConnection* connection = rpc->VirtualConnection;
	RpcOutChannel* outChannel = connection->DefaultOutChannel;
	RpcOutChannel* nextOutChannel = connection->NonDefaultOutChannel;

	rts_pdu_header_init(&header);
	header.frag_length = 96;
	header.Flags = RTS_FLAG_RECYCLE_CHANNEL;
	header.NumberOfCommands = 5;

	WLog_DBG(TAG, "Sending OUT_R1/A3 RTS PDU");

	VirtualConnectionCookie = (BYTE*) &(connection->Cookie);
	PredecessorChannelCookie = (BYTE*) &(outChannel->Cookie);
	SuccessorChannelCookie = (BYTE*) &(nextOutChannel->Cookie);
	ReceiveWindowSize = outChannel->ReceiveWindow;

	buffer = (BYTE*) malloc(header.frag_length);

	if (!buffer)
		return -1;

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_version_command_write(&buffer[20]); /* Version (8 bytes) */
	rts_cookie_command_write(&buffer[28], VirtualConnectionCookie); /* VirtualConnectionCookie (20 bytes) */
	rts_cookie_command_write(&buffer[48], PredecessorChannelCookie); /* PredecessorChannelCookie (20 bytes) */
	rts_cookie_command_write(&buffer[68], SuccessorChannelCookie); /* SuccessorChannelCookie (20 bytes) */
	rts_receive_window_size_command_write(&buffer[88], ReceiveWindowSize); /* ReceiveWindowSize (8 bytes) */

	status = rpc_out_channel_write(nextOutChannel, buffer, header.frag_length);

	free(buffer);

	return (status > 0) ? 1 : -1;
}

int rts_recv_OUT_R1_A2_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	int status;
	UINT32 offset;
	UINT32 Destination = 0;
	RpcVirtualConnection* connection = rpc->VirtualConnection;

	WLog_DBG(TAG, "Receiving OUT R1/A2 RTS PDU");

	offset = 24;
	offset += rts_destination_command_read(rpc, &buffer[offset], length - offset, &Destination) + 4;

	connection->NonDefaultOutChannel = rpc_out_channel_new(rpc);

	if (!connection->NonDefaultOutChannel)
		return -1;

	status = rpc_out_channel_replacement_connect(connection->NonDefaultOutChannel, 5000);

	if (status < 0)
	{
		WLog_ERR(TAG, "rpc_out_channel_replacement_connect failure");
		return -1;
	}

	rpc_out_channel_transition_to_state(connection->DefaultOutChannel, CLIENT_OUT_CHANNEL_STATE_OPENED_A6W);

	return 1;
}

int rts_recv_OUT_R2_A6_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	int status;
	RpcVirtualConnection* connection = rpc->VirtualConnection;

	WLog_DBG(TAG, "Receiving OUT R2/A6 RTS PDU");

	status = rts_send_OUT_R2_C1_pdu(rpc);

	if (status < 0)
	{
		WLog_ERR(TAG, "rts_send_OUT_R2_C1_pdu failure");
		return -1;
	}

	status = rts_send_OUT_R2_A7_pdu(rpc);

	if (status < 0)
	{
		WLog_ERR(TAG, "rts_send_OUT_R2_A7_pdu failure");
		return -1;
	}

	rpc_out_channel_transition_to_state(connection->NonDefaultOutChannel, CLIENT_OUT_CHANNEL_STATE_OPENED_B3W);
	rpc_out_channel_transition_to_state(connection->DefaultOutChannel, CLIENT_OUT_CHANNEL_STATE_OPENED_B3W);

	return 1;
}

int rts_recv_OUT_R2_B3_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	RpcVirtualConnection* connection = rpc->VirtualConnection;

	WLog_DBG(TAG, "Receiving OUT R2/B3 RTS PDU");

	rpc_out_channel_transition_to_state(connection->DefaultOutChannel, CLIENT_OUT_CHANNEL_STATE_RECYCLED);

	return 1;
}

int rts_recv_out_of_sequence_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	int status = -1;
	UINT32 SignatureId;
	rpcconn_rts_hdr_t* rts;
	RtsPduSignature signature;
	RpcVirtualConnection* connection = rpc->VirtualConnection;

	rts = (rpcconn_rts_hdr_t*) buffer;

	rts_extract_pdu_signature(rpc, &signature, rts);
	SignatureId = rts_identify_pdu_signature(rpc, &signature, NULL);

	if (rts_match_pdu_signature(rpc, &RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE, rts))
	{
		status = rts_recv_flow_control_ack_pdu(rpc, buffer, length);
	}
	else if (rts_match_pdu_signature(rpc, &RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION_SIGNATURE, rts))
	{
		status = rts_recv_flow_control_ack_with_destination_pdu(rpc, buffer, length);
	}
	else if (rts_match_pdu_signature(rpc, &RTS_PDU_PING_SIGNATURE, rts))
	{
		status = rts_send_ping_pdu(rpc);
	}
	else
	{
		if (connection->DefaultOutChannel->State == CLIENT_OUT_CHANNEL_STATE_OPENED)
		{
			if (rts_match_pdu_signature(rpc, &RTS_PDU_OUT_R1_A2_SIGNATURE, rts))
			{
				status = rts_recv_OUT_R1_A2_pdu(rpc, buffer, length);
			}
		}
		else if (connection->DefaultOutChannel->State == CLIENT_OUT_CHANNEL_STATE_OPENED_A6W)
		{
			if (rts_match_pdu_signature(rpc, &RTS_PDU_OUT_R2_A6_SIGNATURE, rts))
			{
				status = rts_recv_OUT_R2_A6_pdu(rpc, buffer, length);
			}
		}
		else if (connection->DefaultOutChannel->State == CLIENT_OUT_CHANNEL_STATE_OPENED_B3W)
		{
			if (rts_match_pdu_signature(rpc, &RTS_PDU_OUT_R2_B3_SIGNATURE, rts))
			{
				status = rts_recv_OUT_R2_B3_pdu(rpc, buffer, length);
			}
		}
	}

	if (status < 0)
	{
		WLog_ERR(TAG, "error parsing RTS PDU with signature id: 0x%08X", SignatureId);
		rts_print_pdu_signature(rpc, &signature);
	}

	return status;
}
