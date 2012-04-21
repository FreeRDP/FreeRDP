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

#ifndef FREERDP_CORE_RTS_H
#define FREERDP_CORE_RTS_H

#include "rpc.h"

#include "config.h"

#include <freerdp/types.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>

#define PTYPE_REQUEST				0x00
#define PTYPE_PING				0x01
#define PTYPE_RESPONSE				0x02
#define PTYPE_FAULT				0x03
#define PTYPE_WORKING				0x04
#define PTYPE_NOCALL				0x05
#define PTYPE_REJECT				0x06
#define PTYPE_ACK				0x07
#define PTYPE_CL_CANCEL				0x08
#define PTYPE_FACK				0x09
#define PTYPE_CANCEL_ACK			0x0A
#define PTYPE_BIND				0x0B
#define PTYPE_BIND_ACK				0x0C
#define PTYPE_BIND_NAK				0x0D
#define PTYPE_ALTER_CONTEXT			0x0E
#define PTYPE_ALTER_CONTEXT_RESP		0x0F
#define PTYPE_RPC_AUTH_3			0x10
#define PTYPE_SHUTDOWN				0x11
#define PTYPE_CO_CANCEL				0x12
#define PTYPE_ORPHANED				0x13
#define PTYPE_RTS				0x14

#define PFC_FIRST_FRAG				0x01
#define PFC_LAST_FRAG				0x02
#define PFC_PENDING_CANCEL			0x04
#define PFC_RESERVED_1				0x08
#define PFC_CONC_MPX				0x10
#define PFC_DID_NOT_EXECUTE			0x20
#define PFC_MAYBE				0x40
#define PFC_OBJECT_UUID				0x80

#define RTS_FLAG_NONE				0x0000
#define RTS_FLAG_PING				0x0001
#define RTS_FLAG_OTHER_CMD			0x0002
#define RTS_FLAG_RECYCLE_CHANNEL		0x0004
#define RTS_FLAG_IN_CHANNEL			0x0008
#define RTS_FLAG_OUT_CHANNEL			0x0010
#define RTS_FLAG_EOF				0x0020
#define RTS_FLAG_ECHO				0x0040

#define RTS_CMD_RECEIVE_WINDOW_SIZE		0x00000000
#define RTS_CMD_FLOW_CONTROL_ACK		0x00000001
#define RTS_CMD_CONNECTION_TIMEOUT		0x00000002
#define RTS_CMD_COOKIE				0x00000003
#define RTS_CMD_CHANNEL_LIFETIME		0x00000004
#define RTS_CMD_CLIENT_KEEPALIVE		0x00000005
#define RTS_CMD_VERSION	 			0x00000006
#define RTS_CMD_EMPTY				0x00000007
#define RTS_CMD_PADDING				0x00000008
#define RTS_CMD_NEGATIVE_ANCE			0x00000009
#define RTS_CMD_ANCE				0x0000000A
#define RTS_CMD_CLIENT_ADDRESS			0x0000000B
#define RTS_CMD_ASSOCIATION_GROUP_ID		0x0000000C
#define RTS_CMD_DESTINATION			0x0000000D
#define RTS_CMD_PING_TRAFFIC_SENT_NOTIFY	0x0000000E

#define FDClient				0x00000000
#define FDInProxy				0x00000001
#define FDServer				0x00000002
#define FDOutProxy				0x00000003

struct _rts_pdu_header
{
	uint8 rpc_vers;
	uint8 rpc_vers_minor;
	uint8 ptype;
	uint8 pfc_flags;
	uint8 packed_drep[4];
	uint16 frag_length;
	uint16 auth_length;
	uint32 call_id;
	uint16 flags;
	uint16 numberOfCommands;
};
typedef struct _rts_pdu_header RTS_PDU_HEADER;

struct _rts_pdu
{
	RTS_PDU_HEADER header;
	uint8* content;
};
typedef struct _rts_pdu RTS_PDU;

boolean rts_connect(rdpRpc* rpc);

void rts_pdu_header_read(STREAM* s, RTS_PDU_HEADER* header);
void rts_pdu_header_write(STREAM* s, RTS_PDU_HEADER* header);

void rts_receive_window_size_command_read(rdpRpc* rpc, STREAM* s);
void rts_receive_window_size_command_write(STREAM* s, uint32 ReceiveWindowSize);
void rts_flow_control_ack_command_read(rdpRpc* rpc, STREAM* s);
void rts_flow_control_ack_command_write(STREAM* s, uint32 BytesReceived, uint32 AvailableWindow, uint8* ChannelCookie);
void rts_connection_timeout_command_read(rdpRpc* rpc, STREAM* s);
void rts_connection_timeout_command_write(STREAM* s, uint32 ConnectionTimeout);
void rts_cookie_command_read(rdpRpc* rpc, STREAM* s);
void rts_cookie_command_write(STREAM* s, uint8* Cookie);
void rts_channel_lifetime_command_read(rdpRpc* rpc, STREAM* s);
void rts_channel_lifetime_command_write(STREAM* s, uint32 ChannelLifetime);
void rts_client_keepalive_command_read(rdpRpc* rpc, STREAM* s);
void rts_client_keepalive_command_write(STREAM* s, uint32 ClientKeepalive);
void rts_version_command_read(rdpRpc* rpc, STREAM* s);
void rts_version_command_write(STREAM* s);
void rts_empty_command_read(rdpRpc* rpc, STREAM* s);
void rts_empty_command_write(STREAM* s);
void rts_padding_command_read(rdpRpc* rpc, STREAM* s);
void rts_padding_command_write(STREAM* s, uint32 ConformanceCount);
void rts_negative_ance_command_read(rdpRpc* rpc, STREAM* s);
void rts_negative_ance_command_write(STREAM* s);
void rts_ance_command_read(rdpRpc* rpc, STREAM* s);
void rts_ance_command_write(STREAM* s);
void rts_client_address_command_read(rdpRpc* rpc, STREAM* s);
void rts_client_address_command_write(STREAM* s, uint32 AddressType, uint8* ClientAddress);
void rts_association_group_id_command_read(rdpRpc* rpc, STREAM* s);
void rts_association_group_id_command_write(STREAM* s, uint8* AssociationGroupId);
void rts_destination_command_read(rdpRpc* rpc, STREAM* s);
void rts_destination_command_write(STREAM* s, uint32 Destination);
void rts_ping_traffic_sent_notify_command_read(rdpRpc* rpc, STREAM* s);
void rts_ping_traffic_sent_notify_command_write(STREAM* s, uint32 PingTrafficSent);

boolean rts_send_CONN_A1_pdu(rdpRpc* rpc);
boolean rts_send_CONN_B1_pdu(rdpRpc* rpc);
boolean rts_send_keep_alive_pdu(rdpRpc* rpc);
boolean rts_send_flow_control_ack_pdu(rdpRpc* rpc);
boolean rts_send_ping_pdu(rdpRpc* rpc);

int rts_recv_pdu(rdpRpc* rpc, RTS_PDU* rts_pdu);

#ifdef WITH_DEBUG_TSG
#define WITH_DEBUG_RTS
#endif

#ifdef WITH_DEBUG_RTS
#define DEBUG_RTS(fmt, ...) DEBUG_CLASS(RTS, fmt, ## __VA_ARGS__)
#else
#define DEBUG_RTS(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_CORE_RTS_H */
