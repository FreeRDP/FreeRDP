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

void rts_receive_window_size_command_write(STREAM* s, uint32 receiveWindowSize)
{
	stream_write_uint32(s, RTS_CMD_RECEIVE_WINDOW_SIZE); /* CommandType (4 bytes) */
	stream_write_uint32(s, receiveWindowSize); /* ReceiveWindowSize (4 bytes) */
}

void rts_cookie_command_write(STREAM* s, uint8* cookie)
{
	stream_write_uint32(s, RTS_CMD_COOKIE); /* CommandType (4 bytes) */
	stream_write(s, cookie, 16); /* Cookie (16 bytes) */
}

void rts_channel_lifetime_command_write(STREAM* s, uint32 channelLifetime)
{
	stream_write_uint32(s, RTS_CMD_CHANNEL_LIFETIME); /* CommandType (4 bytes) */
	stream_write_uint32(s, channelLifetime); /* ChannelLifetime (4 bytes) */
}

void rts_client_keepalive_command_write(STREAM* s, uint32 clientKeepalive)
{
	stream_write_uint32(s, RTS_CMD_CLIENT_KEEPALIVE); /* CommandType (4 bytes) */
	stream_write_uint32(s, clientKeepalive); /* ClientKeepalive (4 bytes) */
}

void rts_version_command_write(STREAM* s)
{
	stream_write_uint32(s, RTS_CMD_VERSION); /* CommandType (4 bytes) */
	stream_write_uint32(s, 1); /* Version (4 bytes) */
}

void rts_padding_command_read(STREAM* s)
{
	uint32 ConformanceCount;

	stream_read_uint32(s, ConformanceCount); /* ConformanceCount (4 bytes) */
	stream_seek(s, ConformanceCount); /* Padding (variable) */
}

void rts_association_group_id_command_write(STREAM* s, uint8* associationGroupId)
{
	stream_write_uint32(s, RTS_CMD_ASSOCIATION_GROUP_ID); /* CommandType (4 bytes) */
	stream_write(s, associationGroupId, 16); /* AssociationGroupId (16 bytes) */
}

void rts_client_address_command_read(STREAM* s)
{
	uint32 AddressType;

	stream_read_uint32(s, AddressType); /* ConformanceCount (4 bytes) */

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

int rts_pdu_recv(rdpRpc* rpc, STREAM* s)
{
	int i;
	uint16 flags;
	uint32 CommandType;
	uint16 numberOfCommands;

	stream_seek_uint8(s); /* rpc_vers (1 byte) */
	stream_seek_uint8(s); /* rpc_vers_minor (1 byte) */
	stream_seek_uint8(s); /* PTYPE (1 byte) */
	stream_seek_uint8(s); /* pfc_flags (1 byte) */
	stream_seek_uint32(s); /* packet_drep (4 bytes) */
	stream_seek_uint16(s); /* frag_length (2 bytes) */
	stream_seek_uint16(s); /* auth_length (2 bytes) */
	stream_seek_uint32(s); /* call_id (4 bytes) */
	stream_read_uint16(s, flags); /* flags (2 bytes) */
	stream_read_uint16(s, numberOfCommands); /* numberOfCommands (2 bytes) */

	if (flags & RTS_FLAG_PING)
	{
		rpc_in_send_keep_alive(rpc);
		return 0;
	}

	for (i = 0; i < numberOfCommands; i++)
	{
		stream_read_uint32(s, CommandType); /* CommandType (4 bytes) */

		switch (CommandType)
		{
			case RTS_CMD_RECEIVE_WINDOW_SIZE:
				stream_seek(s, 4);
				break;

			case RTS_CMD_FLOW_CONTROL_ACK:
				stream_seek(s, 24);
				break;

			case RTS_CMD_CONNECTION_TIMEOUT:
				stream_seek(s, 4);
				break;

			case RTS_CMD_COOKIE:
				stream_seek(s, 16);
				break;

			case RTS_CMD_CHANNEL_LIFETIME:
				stream_seek(s, 4);
				break;

			case RTS_CMD_CLIENT_KEEPALIVE:
				stream_seek(s, 4);
				break;

			case RTS_CMD_VERSION:
				stream_seek(s, 4);
				break;

			case RTS_CMD_EMPTY:
				break;

			case RTS_CMD_PADDING:
				rts_padding_command_read(s);
				break;

			case RTS_CMD_NEGATIVE_ANCE:
				break;

			case RTS_CMD_ANCE:
				break;

			case RTS_CMD_CLIENT_ADDRESS:
				rts_client_address_command_read(s);
				break;

			case RTS_CMD_ASSOCIATION_GROUP_ID:
				stream_seek(s, 16);
				break;

			case RTS_CMD_DESTINATION:
				stream_seek(s, 4);
				break;

			case RTS_CMD_PING_TRAFFIC_SENT_NOTIFY:
				stream_seek(s, 4);
				break;

			default:
				printf(" Error: Unknown RTS Command Type: 0x%x\n", CommandType);
				return -1;
				break;
		}
	}

	return 0;
}
