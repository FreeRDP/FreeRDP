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

void rts_pdu_header_write(STREAM* s, uint8 pfc_flags, uint16 frag_length,
		uint16 auth_length, uint32 call_id, uint16 flags, uint16 numberOfCommands)
{
	stream_write_uint8(s, 5); /* rpc_vers (1 byte) */
	stream_write_uint8(s, 0); /* rpc_vers_minor (1 byte) */
	stream_write_uint8(s, PTYPE_RTS); /* PTYPE (1 byte) */
	stream_write_uint8(s, pfc_flags); /* pfc_flags (1 byte) */
	stream_write_uint32(s, 0x00000010); /* packet_drep (4 bytes) */
	stream_write_uint16(s, frag_length); /* frag_length (2 bytes) */
	stream_write_uint16(s, auth_length); /* auth_length (2 bytes) */
	stream_write_uint32(s, call_id); /* call_id (4 bytes) */
	stream_write_uint16(s, flags); /* flags (2 bytes) */
	stream_write_uint16(s, numberOfCommands); /* numberOfCommands (2 bytes) */
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

void rts_association_group_id_command_write(STREAM* s, uint8* associationGroupId)
{
	stream_write_uint32(s, RTS_CMD_ASSOCIATION_GROUP_ID); /* CommandType (4 bytes) */
	stream_write(s, associationGroupId, 16); /* AssociationGroupId (16 bytes) */
}
