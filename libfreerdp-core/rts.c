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

#include "rts.h"

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

void rts_pdu_header_read(STREAM* s, RTS_PDU_HEADER* header)
{
	stream_read_uint8(s, header->rpc_vers); /* rpc_vers (1 byte) */
	stream_read_uint8(s, header->rpc_vers_minor); /* rpc_vers_minor (1 byte) */
	stream_read_uint8(s, header->ptype); /* PTYPE (1 byte) */
	stream_read_uint8(s, header->pfc_flags); /* pfc_flags (1 byte) */
	stream_read_uint8(s, header->packed_drep[0]); /* packet_drep[0] (1 byte) */
	stream_read_uint8(s, header->packed_drep[1]); /* packet_drep[1] (1 byte) */
	stream_read_uint8(s, header->packed_drep[2]); /* packet_drep[2] (1 byte) */
	stream_read_uint8(s, header->packed_drep[3]); /* packet_drep[3] (1 byte) */
	stream_read_uint16(s, header->frag_length); /* frag_length (2 bytes) */
	stream_read_uint16(s, header->auth_length); /* auth_length (2 bytes) */
	stream_read_uint32(s, header->call_id); /* call_id (4 bytes) */
	stream_read_uint16(s, header->flags); /* flags (2 bytes) */
	stream_read_uint16(s, header->numberOfCommands); /* numberOfCommands (2 bytes) */
}

void rts_pdu_header_write(STREAM* s, RTS_PDU_HEADER* header)
{
	stream_write_uint8(s, header->rpc_vers); /* rpc_vers (1 byte) */
	stream_write_uint8(s, header->rpc_vers_minor); /* rpc_vers_minor (1 byte) */
	stream_write_uint8(s, header->ptype); /* PTYPE (1 byte) */
	stream_write_uint8(s, header->pfc_flags); /* pfc_flags (1 byte) */
	stream_write_uint8(s, header->packed_drep[0]); /* packet_drep[0] (1 byte) */
	stream_write_uint8(s, header->packed_drep[1]); /* packet_drep[1] (1 byte) */
	stream_write_uint8(s, header->packed_drep[2]); /* packet_drep[2] (1 byte) */
	stream_write_uint8(s, header->packed_drep[3]); /* packet_drep[3] (1 byte) */
	stream_write_uint16(s, header->frag_length); /* frag_length (2 bytes) */
	stream_write_uint16(s, header->auth_length); /* auth_length (2 bytes) */
	stream_write_uint32(s, header->call_id); /* call_id (4 bytes) */
	stream_write_uint16(s, header->flags); /* flags (2 bytes) */
	stream_write_uint16(s, header->numberOfCommands); /* numberOfCommands (2 bytes) */
}

void rts_receive_window_size_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek_uint32(s); /* ReceiveWindowSize (4 bytes) */
}

void rts_receive_window_size_command_write(STREAM* s, uint32 ReceiveWindowSize)
{
	stream_write_uint32(s, RTS_CMD_RECEIVE_WINDOW_SIZE); /* CommandType (4 bytes) */
	stream_write_uint32(s, ReceiveWindowSize); /* ReceiveWindowSize (4 bytes) */
}

void rts_flow_control_ack_command_read(rdpRpc* rpc, STREAM* s)
{
	/* Ack (24 bytes) */
	stream_seek_uint32(s); /* BytesReceived (4 bytes) */
	stream_seek_uint32(s); /* AvailableWindow (4 bytes) */
	stream_seek(s, 16); /* ChannelCookie (16 bytes) */
}

void rts_flow_control_ack_command_write(STREAM* s, uint32 BytesReceived, uint32 AvailableWindow, uint8* ChannelCookie)
{
	stream_write_uint32(s, RTS_CMD_FLOW_CONTROL_ACK); /* CommandType (4 bytes) */

	/* Ack (24 bytes) */
	stream_write_uint32(s, BytesReceived); /* BytesReceived (4 bytes) */
	stream_write_uint32(s, AvailableWindow); /* AvailableWindow (4 bytes) */
	stream_write(s, ChannelCookie, 16); /* ChannelCookie (16 bytes) */
}

void rts_connection_timeout_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek_uint32(s); /* ConnectionTimeout (4 bytes) */
}

void rts_connection_timeout_command_write(STREAM* s, uint32 ConnectionTimeout)
{
	stream_write_uint32(s, RTS_CMD_CONNECTION_TIMEOUT); /* CommandType (4 bytes) */
	stream_write_uint32(s, ConnectionTimeout); /* ConnectionTimeout (4 bytes) */
}

void rts_cookie_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek(s, 16); /* Cookie (16 bytes) */
}

void rts_cookie_command_write(STREAM* s, uint8* Cookie)
{
	stream_write_uint32(s, RTS_CMD_COOKIE); /* CommandType (4 bytes) */
	stream_write(s, Cookie, 16); /* Cookie (16 bytes) */
}

void rts_channel_lifetime_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek_uint32(s); /* ChannelLifetime (4 bytes) */
}

void rts_channel_lifetime_command_write(STREAM* s, uint32 ChannelLifetime)
{
	stream_write_uint32(s, RTS_CMD_CHANNEL_LIFETIME); /* CommandType (4 bytes) */
	stream_write_uint32(s, ChannelLifetime); /* ChannelLifetime (4 bytes) */
}

void rts_client_keepalive_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek_uint32(s); /* ClientKeepalive (4 bytes) */
}

void rts_client_keepalive_command_write(STREAM* s, uint32 ClientKeepalive)
{
	stream_write_uint32(s, RTS_CMD_CLIENT_KEEPALIVE); /* CommandType (4 bytes) */
	stream_write_uint32(s, ClientKeepalive); /* ClientKeepalive (4 bytes) */
}

void rts_version_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek_uint32(s); /* Version (4 bytes) */
}

void rts_version_command_write(STREAM* s)
{
	stream_write_uint32(s, RTS_CMD_VERSION); /* CommandType (4 bytes) */
	stream_write_uint32(s, 1); /* Version (4 bytes) */
}

void rts_empty_command_read(rdpRpc* rpc, STREAM* s)
{

}

void rts_empty_command_write(STREAM* s)
{
	stream_write_uint32(s, RTS_CMD_EMPTY); /* CommandType (4 bytes) */
}

void rts_padding_command_read(rdpRpc* rpc, STREAM* s)
{
	uint32 ConformanceCount;

	stream_read_uint32(s, ConformanceCount); /* ConformanceCount (4 bytes) */
	stream_seek(s, ConformanceCount); /* Padding (variable) */
}

void rts_padding_command_write(STREAM* s, uint32 ConformanceCount)
{
	stream_write_uint32(s, ConformanceCount); /* ConformanceCount (4 bytes) */
	stream_write_zero(s, ConformanceCount); /* Padding (variable) */
}

void rts_negative_ance_command_read(rdpRpc* rpc, STREAM* s)
{

}

void rts_negative_ance_command_write(STREAM* s)
{
	stream_write_uint32(s, RTS_CMD_NEGATIVE_ANCE); /* CommandType (4 bytes) */
}

void rts_ance_command_read(rdpRpc* rpc, STREAM* s)
{

}

void rts_ance_command_write(STREAM* s)
{
	stream_write_uint32(s, RTS_CMD_ANCE); /* CommandType (4 bytes) */
}

void rts_client_address_command_read(rdpRpc* rpc, STREAM* s)
{
	uint32 AddressType;

	stream_read_uint32(s, AddressType); /* AddressType (4 bytes) */

	if (AddressType == 0)
	{
		stream_seek(s, 4); /* ClientAddress (4 bytes) */
	}
	else
	{
		stream_seek(s, 16); /* ClientAddress (16 bytes) */
	}

	stream_seek(s, 12); /* padding (12 bytes) */
}

void rts_client_address_command_write(STREAM* s, uint32 AddressType, uint8* ClientAddress)
{
	stream_write_uint32(s, RTS_CMD_CLIENT_ADDRESS); /* CommandType (4 bytes) */
	stream_write_uint32(s, AddressType); /* AddressType (4 bytes) */

	if (AddressType == 0)
	{
		stream_write(s, ClientAddress, 4); /* ClientAddress (4 bytes) */
	}
	else
	{
		stream_write(s, ClientAddress, 16); /* ClientAddress (16 bytes) */
	}

	stream_write_zero(s, 12); /* padding (12 bytes) */
}

void rts_association_group_id_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek(s, 16); /* AssociationGroupId (16 bytes) */
}

void rts_association_group_id_command_write(STREAM* s, uint8* associationGroupId)
{
	stream_write_uint32(s, RTS_CMD_ASSOCIATION_GROUP_ID); /* CommandType (4 bytes) */
	stream_write(s, associationGroupId, 16); /* AssociationGroupId (16 bytes) */
}

void rts_destination_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek_uint32(s); /* Destination (4 bytes) */
}

void rts_destination_command_write(STREAM* s, uint32 Destination)
{
	stream_write_uint32(s, RTS_CMD_DESTINATION); /* CommandType (4 bytes) */
	stream_write_uint32(s, Destination); /* Destination (4 bytes) */
}

void rts_ping_traffic_sent_notify_command_read(rdpRpc* rpc, STREAM* s)
{
	stream_seek_uint32(s); /* PingTrafficSent (4 bytes) */
}

void rts_ping_traffic_sent_notify_command_write(STREAM* s, uint32 PingTrafficSent)
{
	stream_write_uint32(s, RTS_CMD_PING_TRAFFIC_SENT_NOTIFY); /* CommandType (4 bytes) */
	stream_write_uint32(s, PingTrafficSent); /* PingTrafficSent (4 bytes) */
}

int rts_pdu_recv(rdpRpc* rpc, STREAM* s)
{
	int i;
	uint32 CommandType;
	RTS_PDU_HEADER header;

	rts_pdu_header_read(s, &header);

	DEBUG_RTS("numberOfCommands:%d", header.numberOfCommands);

	if (header.flags & RTS_FLAG_PING)
	{
		rpc_send_keep_alive_pdu(rpc);
		return 0;
	}

	for (i = 0; i < header.numberOfCommands; i++)
	{
		stream_read_uint32(s, CommandType); /* CommandType (4 bytes) */

		DEBUG_RTS("CommandType: %s (0x%08X)", RTS_CMD_STRINGS[CommandType % 14], CommandType);

		switch (CommandType)
		{
			case RTS_CMD_RECEIVE_WINDOW_SIZE:
				rts_receive_window_size_command_read(rpc, s);
				break;

			case RTS_CMD_FLOW_CONTROL_ACK:
				rts_flow_control_ack_command_read(rpc, s);
				break;

			case RTS_CMD_CONNECTION_TIMEOUT:
				rts_connection_timeout_command_read(rpc, s);
				break;

			case RTS_CMD_COOKIE:
				rts_cookie_command_read(rpc, s);
				break;

			case RTS_CMD_CHANNEL_LIFETIME:
				rts_channel_lifetime_command_read(rpc, s);
				break;

			case RTS_CMD_CLIENT_KEEPALIVE:
				rts_client_keepalive_command_read(rpc, s);
				break;

			case RTS_CMD_VERSION:
				rts_version_command_read(rpc, s);
				break;

			case RTS_CMD_EMPTY:
				rts_empty_command_read(rpc, s);
				break;

			case RTS_CMD_PADDING:
				rts_padding_command_read(rpc, s);
				break;

			case RTS_CMD_NEGATIVE_ANCE:
				rts_negative_ance_command_read(rpc, s);
				break;

			case RTS_CMD_ANCE:
				rts_ance_command_read(rpc, s);
				break;

			case RTS_CMD_CLIENT_ADDRESS:
				rts_client_address_command_read(rpc, s);
				break;

			case RTS_CMD_ASSOCIATION_GROUP_ID:
				rts_association_group_id_command_read(rpc, s);
				break;

			case RTS_CMD_DESTINATION:
				rts_destination_command_read(rpc, s);
				break;

			case RTS_CMD_PING_TRAFFIC_SENT_NOTIFY:
				rts_ping_traffic_sent_notify_command_read(rpc, s);
				break;

			default:
				printf("Error: Unknown RTS Command Type: 0x%x\n", CommandType);
				return -1;
				break;
		}
	}

	return 0;
}
