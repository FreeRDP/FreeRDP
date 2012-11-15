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

#include "rts.h"

/**
 *                                      Connection Establishment\n
 *
 *     Client                  Outbound Proxy           Inbound Proxy                 Server\n
 *        |                         |                         |                         |\n
 *        |-----------------IN Channel Request--------------->|                         |\n
 *        |---OUT Channel Request-->|                         |<-Legacy Server Response-|\n
 *        |                         |<--------------Legacy Server Response--------------|\n
 *        |                         |                         |                         |\n
 *        |---------CONN_A1-------->|                         |                         |\n
 *        |----------------------CONN_B1--------------------->|                         |\n
 *        |                         |----------------------CONN_A2--------------------->|\n
 *        |                         |                         |                         |\n
 *        |<--OUT Channel Response--|                         |---------CONN_B2-------->|\n
 *        |<--------CONN_A3---------|                         |                         |\n
 *        |                         |<---------------------CONN_C1----------------------|\n
 *        |                         |                         |<--------CONN_B3---------|\n
 *        |<--------CONN_C2---------|                         |                         |\n
 *        |                         |                         |                         |\n
 *
 */

/**
 * [MS-RPCH]: Remote Procedure Call over HTTP Protocol Specification:
 * http://msdn.microsoft.com/en-us/library/cc243950/
 */

BOOL rts_connect(rdpRpc* rpc)
{
	int status;
	HttpResponse* http_response;

	if (!rpc_ntlm_http_out_connect(rpc))
	{
		printf("rpc_out_connect_http error!\n");
		return FALSE;
	}

	if (!rts_send_CONN_A1_pdu(rpc))
	{
		printf("rpc_send_CONN_A1_pdu error!\n");
		return FALSE;
	}

	if (!rpc_ntlm_http_in_connect(rpc))
	{
		printf("rpc_in_connect_http error!\n");
		return FALSE;
	}

	if (!rts_send_CONN_B1_pdu(rpc))
	{
		printf("rpc_send_CONN_B1_pdu error!\n");
		return FALSE;
	}

	/* Receive OUT Channel Response */
	http_response = http_response_recv(rpc->TlsOut);

	if (http_response->StatusCode != 200)
	{
		printf("rts_connect error! Status Code: %d\n", http_response->StatusCode);
		http_response_print(http_response);
		http_response_free(http_response);
		return FALSE;
	}

	http_response_print(http_response);

	http_response_free(http_response);

	/* Receive CONN_A3 RTS PDU */
	status = rts_recv_pdu(rpc);

	if (status < 1)
		return FALSE;

	/* Receive CONN_C2 RTS PDU */
	status = rts_recv_pdu(rpc);

	if (status < 1)
		return FALSE;

	return TRUE;
}

#ifdef WITH_DEBUG_RTS

static const char* const RTS_CMD_STRINGS[] =
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

#endif

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

int rts_receive_window_size_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* ReceiveWindowSize (4 bytes) */
	return 4;
}

int rts_receive_window_size_command_write(BYTE* buffer, UINT32 ReceiveWindowSize)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_RECEIVE_WINDOW_SIZE; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = ReceiveWindowSize; /* ReceiveWindowSize (4 bytes) */
	return 8;
}

int rts_flow_control_ack_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* Ack (24 bytes) */
	/* BytesReceived (4 bytes) */
	/* AvailableWindow (4 bytes) */
	/* ChannelCookie (16 bytes) */
	return 24;
}

int rts_flow_control_ack_command_write(BYTE* buffer, UINT32 BytesReceived, UINT32 AvailableWindow, BYTE* ChannelCookie)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_FLOW_CONTROL_ACK; /* CommandType (4 bytes) */

	/* Ack (24 bytes) */
	*((UINT32*) &buffer[4]) = BytesReceived; /* BytesReceived (4 bytes) */
	*((UINT32*) &buffer[8]) = AvailableWindow; /* AvailableWindow (4 bytes) */
	CopyMemory(&buffer[12], ChannelCookie, 16); /* ChannelCookie (16 bytes) */

	return 28;
}

int rts_connection_timeout_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* ConnectionTimeout (4 bytes) */
	return 4;
}

int rts_connection_timeout_command_write(BYTE* buffer, UINT32 ConnectionTimeout)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_CONNECTION_TIMEOUT; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = ConnectionTimeout; /* ConnectionTimeout (4 bytes) */
	return 8;
}

int rts_cookie_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* Cookie (16 bytes) */
	return 16;
}

int rts_cookie_command_write(BYTE* buffer, BYTE* Cookie)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_COOKIE; /* CommandType (4 bytes) */
	CopyMemory(&buffer[4], Cookie, 16); /* Cookie (16 bytes) */
	return 20;
}

int rts_channel_lifetime_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* ChannelLifetime (4 bytes) */
	return 4;
}

int rts_channel_lifetime_command_write(BYTE* buffer, UINT32 ChannelLifetime)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_CHANNEL_LIFETIME; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = ChannelLifetime; /* ChannelLifetime (4 bytes) */
	return 8;
}

int rts_client_keepalive_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* ClientKeepalive (4 bytes) */
	return 4;
}

int rts_client_keepalive_command_write(BYTE* buffer, UINT32 ClientKeepalive)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_CLIENT_KEEPALIVE; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = ClientKeepalive; /* ClientKeepalive (4 bytes) */
	return 8;
}

int rts_version_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* Version (4 bytes) */
	return 4;
}

int rts_version_command_write(BYTE* buffer)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_VERSION; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = 1; /* Version (4 bytes) */
	return 8;
}

int rts_empty_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	return 0;
}

int rts_empty_command_write(BYTE* buffer)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_EMPTY; /* CommandType (4 bytes) */
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
	*((UINT32*) &buffer[0]) = RTS_CMD_PADDING; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = ConformanceCount; /* ConformanceCount (4 bytes) */
	ZeroMemory(&buffer[8], ConformanceCount); /* Padding (variable) */
	return 8 + ConformanceCount;
}

int rts_negative_ance_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	return 0;
}

int rts_negative_ance_command_write(BYTE* buffer)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_NEGATIVE_ANCE; /* CommandType (4 bytes) */
	return 4;
}

int rts_ance_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	return 0;
}

int rts_ance_command_write(BYTE* buffer)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_ANCE; /* CommandType (4 bytes) */
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
	*((UINT32*) &buffer[0]) = RTS_CMD_CLIENT_ADDRESS; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = AddressType; /* AddressType (4 bytes) */

	if (AddressType == 0)
	{
		CopyMemory(&buffer[8], ClientAddress, 4); /* ClientAddress (4 bytes) */
		ZeroMemory(&buffer[12], 12); /* padding (12 bytes) */
		return 24;
	}
	else
	{
		CopyMemory(&buffer[8], ClientAddress, 16); /* ClientAddress (16 bytes) */
		ZeroMemory(&buffer[24], 12); /* padding (12 bytes) */
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
	*((UINT32*) &buffer[0]) = RTS_CMD_ASSOCIATION_GROUP_ID; /* CommandType (4 bytes) */
	CopyMemory(&buffer[4], AssociationGroupId, 16); /* AssociationGroupId (16 bytes) */
	return 20;
}

int rts_destination_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* Destination (4 bytes) */
	return 4;
}

int rts_destination_command_write(BYTE* buffer, UINT32 Destination)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_DESTINATION; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = Destination; /* Destination (4 bytes) */
	return 8;
}

int rts_ping_traffic_sent_notify_command_read(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	/* PingTrafficSent (4 bytes) */
	return 4;
}

int rts_ping_traffic_sent_notify_command_write(BYTE* buffer, UINT32 PingTrafficSent)
{
	*((UINT32*) &buffer[0]) = RTS_CMD_PING_TRAFFIC_SENT_NOTIFY; /* CommandType (4 bytes) */
	*((UINT32*) &buffer[4]) = PingTrafficSent; /* PingTrafficSent (4 bytes) */
	return 8;
}

void rts_generate_cookie(BYTE* cookie)
{
	RAND_pseudo_bytes(cookie, 16);
}

BOOL rts_send_CONN_A1_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	rpcconn_rts_hdr_t header;
	UINT32 ReceiveWindowSize;
	BYTE* OUTChannelCookie;
	BYTE* VirtualConnectionCookie;

	rts_pdu_header_init(&header);
	header.frag_length = 76;
	header.Flags = RTS_FLAG_NONE;
	header.NumberOfCommands = 4;

	DEBUG_RPC("Sending CONN_A1 RTS PDU");

	rts_generate_cookie((BYTE*) &(rpc->VirtualConnection->Cookie));
	rts_generate_cookie((BYTE*) &(rpc->VirtualConnection->DefaultOutChannelCookie));

	VirtualConnectionCookie = (BYTE*) &(rpc->VirtualConnection->Cookie);
	OUTChannelCookie = (BYTE*) &(rpc->VirtualConnection->DefaultOutChannelCookie);
	ReceiveWindowSize = rpc->VirtualConnection->DefaultOutChannel->ReceiveWindow;

	buffer = (BYTE*) malloc(header.frag_length);

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_version_command_write(&buffer[20]); /* Version (8 bytes) */
	rts_cookie_command_write(&buffer[28], VirtualConnectionCookie); /* VirtualConnectionCookie (20 bytes) */
	rts_cookie_command_write(&buffer[48], OUTChannelCookie); /* OUTChannelCookie (20 bytes) */
	rts_receive_window_size_command_write(&buffer[68], ReceiveWindowSize); /* ReceiveWindowSize (8 bytes) */

	rpc_out_write(rpc, buffer, header.frag_length);

	free(buffer);

	return TRUE;
}

BOOL rts_send_CONN_B1_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	rpcconn_rts_hdr_t header;
	BYTE* INChannelCookie;
	BYTE* AssociationGroupId;
	BYTE* VirtualConnectionCookie;

	rts_pdu_header_init(&header);
	header.frag_length = 104;
	header.Flags = RTS_FLAG_NONE;
	header.NumberOfCommands = 6;

	DEBUG_RPC("Sending CONN_B1 RTS PDU");

	rts_generate_cookie((BYTE*) &(rpc->VirtualConnection->DefaultInChannelCookie));
	rts_generate_cookie((BYTE*) &(rpc->VirtualConnection->AssociationGroupId));

	VirtualConnectionCookie = (BYTE*) &(rpc->VirtualConnection->Cookie);
	INChannelCookie = (BYTE*) &(rpc->VirtualConnection->DefaultInChannelCookie);
	AssociationGroupId = (BYTE*) &(rpc->VirtualConnection->AssociationGroupId);

	/* TODO: fix hardcoded values */

	buffer = (BYTE*) malloc(header.frag_length);

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_version_command_write(&buffer[20]); /* Version (8 bytes) */
	rts_cookie_command_write(&buffer[28], VirtualConnectionCookie); /* VirtualConnectionCookie (20 bytes) */
	rts_cookie_command_write(&buffer[48], INChannelCookie); /* INChannelCookie (20 bytes) */
	rts_channel_lifetime_command_write(&buffer[68], 0x40000000); /* ChannelLifetime (8 bytes) */
	rts_client_keepalive_command_write(&buffer[76], 0x000493E0); /* ClientKeepalive (8 bytes) */
	rts_association_group_id_command_write(&buffer[84], AssociationGroupId); /* AssociationGroupId (20 bytes) */

	rpc_in_write(rpc, buffer, header.frag_length);

	free(buffer);

	return TRUE;
}

BOOL rts_send_keep_alive_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	rpcconn_rts_hdr_t header;

	rts_pdu_header_init(&header);
	header.frag_length = 28;
	header.Flags = RTS_FLAG_OTHER_CMD;
	header.NumberOfCommands = 1;

	DEBUG_RPC("Sending Keep-Alive RTS PDU");

	/* TODO: fix hardcoded value */

	buffer = (BYTE*) malloc(header.frag_length);
	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_client_keepalive_command_write(&buffer[20], 0x00007530); /* ClientKeepalive (8 bytes) */

	rpc_in_write(rpc, buffer, header.frag_length);

	free(buffer);

	return TRUE;
}

BOOL rts_send_flow_control_ack_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	rpcconn_rts_hdr_t header;
	UINT32 BytesReceived;
	UINT32 AvailableWindow;
	BYTE* ChannelCookie;

	rts_pdu_header_init(&header);
	header.frag_length = 56;
	header.Flags = RTS_FLAG_OTHER_CMD;
	header.NumberOfCommands = 2;

	DEBUG_RPC("Sending FlowControlAck RTS PDU");

	BytesReceived = rpc->VirtualConnection->DefaultOutChannel->BytesReceived;
	AvailableWindow = rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow;
	ChannelCookie = (BYTE*) &(rpc->VirtualConnection->DefaultOutChannelCookie);

	buffer = (BYTE*) malloc(header.frag_length);

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_destination_command_write(&buffer[20], FDOutProxy); /* Destination Command (8 bytes) */

	/* FlowControlAck Command (28 bytes) */
	rts_flow_control_ack_command_write(&buffer[28], BytesReceived, AvailableWindow, ChannelCookie);

	rpc_in_write(rpc, buffer, header.frag_length);

	free(buffer);

	return TRUE;
}

BOOL rts_send_ping_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	rpcconn_rts_hdr_t header;

	rts_pdu_header_init(&header);
	header.frag_length = 20;
	header.Flags = RTS_FLAG_PING;
	header.NumberOfCommands = 0;

	DEBUG_RPC("Sending Ping RTS PDU");

	buffer = (BYTE*) malloc(header.frag_length);

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */

	rpc_in_write(rpc, buffer, header.frag_length);

	free(buffer);

	return TRUE;
}

int rts_recv_pdu_commands(rdpRpc* rpc, rpcconn_rts_hdr_t* rts)
{
	int i;
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;
	UINT32 CommandType;

	DEBUG_RTS("numberOfCommands:%d", rts->NumberOfCommands);

	if (rts->Flags & RTS_FLAG_PING)
	{
		rts_send_keep_alive_pdu(rpc);
		return 0;
	}

	offset = 24;
	buffer = &((BYTE*) rts)[offset];
	length = rts->frag_length - offset;

	for (i = 0; i < rts->NumberOfCommands; i++)
	{
		CommandType = *((UINT32*) &buffer[offset]); /* CommandType (4 bytes) */
		offset += 4;

		DEBUG_RTS("CommandType: %s (0x%08X)", RTS_CMD_STRINGS[CommandType % 14], CommandType);

		switch (CommandType)
		{
			case RTS_CMD_RECEIVE_WINDOW_SIZE:
				offset += rts_receive_window_size_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_FLOW_CONTROL_ACK:
				offset += rts_flow_control_ack_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_CONNECTION_TIMEOUT:
				offset += rts_connection_timeout_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_COOKIE:
				offset += rts_cookie_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_CHANNEL_LIFETIME:
				offset += rts_channel_lifetime_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_CLIENT_KEEPALIVE:
				offset += rts_client_keepalive_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_VERSION:
				offset += rts_version_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_EMPTY:
				offset += rts_empty_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_PADDING:
				offset += rts_padding_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_NEGATIVE_ANCE:
				offset += rts_negative_ance_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_ANCE:
				offset += rts_ance_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_CLIENT_ADDRESS:
				offset += rts_client_address_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_ASSOCIATION_GROUP_ID:
				offset += rts_association_group_id_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_DESTINATION:
				offset += rts_destination_command_read(rpc, buffer, length);
				break;

			case RTS_CMD_PING_TRAFFIC_SENT_NOTIFY:
				offset += rts_ping_traffic_sent_notify_command_read(rpc, buffer, length);
				break;

			default:
				printf("Error: Unknown RTS Command Type: 0x%x\n", CommandType);
				return -1;
				break;
		}

		buffer = &((BYTE*) rts)[offset];
		length = rts->frag_length - offset;
	}

	return 0;
}

int rts_recv_pdu(rdpRpc* rpc)
{
	int status;
	rpcconn_rts_hdr_t* rts;

	status = rpc_recv_pdu(rpc);

	if (status > 0)
	{
		rts = (rpcconn_rts_hdr_t*) rpc->buffer;

		if (rts->ptype != PTYPE_RTS)
		{
			printf("rts_recv_pdu: Unexpected type 0x%02X, Expected: PTYPE_RTS (0x%02X)\n",
					rts->ptype, PTYPE_RTS);
			return -1;
		}
	}

	rts_recv_pdu_commands(rpc, rts);

	return status;
}
