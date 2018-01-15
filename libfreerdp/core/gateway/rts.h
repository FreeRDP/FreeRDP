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

#ifndef FREERDP_LIB_CORE_GATEWAY_RTS_H
#define FREERDP_LIB_CORE_GATEWAY_RTS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rpc.h"

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/log.h>

#define RTS_FLAG_NONE					0x0000
#define RTS_FLAG_PING					0x0001
#define RTS_FLAG_OTHER_CMD				0x0002
#define RTS_FLAG_RECYCLE_CHANNEL			0x0004
#define RTS_FLAG_IN_CHANNEL				0x0008
#define RTS_FLAG_OUT_CHANNEL				0x0010
#define RTS_FLAG_EOF					0x0020
#define RTS_FLAG_ECHO					0x0040

#define RTS_CMD_RECEIVE_WINDOW_SIZE			0x00000000
#define RTS_CMD_FLOW_CONTROL_ACK			0x00000001
#define RTS_CMD_CONNECTION_TIMEOUT			0x00000002
#define RTS_CMD_COOKIE					0x00000003
#define RTS_CMD_CHANNEL_LIFETIME			0x00000004
#define RTS_CMD_CLIENT_KEEPALIVE			0x00000005
#define RTS_CMD_VERSION	 				0x00000006
#define RTS_CMD_EMPTY					0x00000007
#define RTS_CMD_PADDING					0x00000008
#define RTS_CMD_NEGATIVE_ANCE				0x00000009
#define RTS_CMD_ANCE					0x0000000A
#define RTS_CMD_CLIENT_ADDRESS				0x0000000B
#define RTS_CMD_ASSOCIATION_GROUP_ID			0x0000000C
#define RTS_CMD_DESTINATION				0x0000000D
#define RTS_CMD_PING_TRAFFIC_SENT_NOTIFY		0x0000000E
#define RTS_CMD_LAST_ID					0x0000000F

#define RTS_CMD_RECEIVE_WINDOW_SIZE_LENGTH		0x00000004
#define RTS_CMD_FLOW_CONTROL_ACK_LENGTH			0x00000018
#define RTS_CMD_CONNECTION_TIMEOUT_LENGTH		0x00000004
#define RTS_CMD_COOKIE_LENGTH				0x00000010
#define RTS_CMD_CHANNEL_LIFETIME_LENGTH			0x00000004
#define RTS_CMD_CLIENT_KEEPALIVE_LENGTH			0x00000004
#define RTS_CMD_VERSION_LENGTH 				0x00000004
#define RTS_CMD_EMPTY_LENGTH				0x00000000
#define RTS_CMD_PADDING_LENGTH				0x00000000 /* variable-size */
#define RTS_CMD_NEGATIVE_ANCE_LENGTH			0x00000000
#define RTS_CMD_ANCE_LENGTH				0x00000000
#define RTS_CMD_CLIENT_ADDRESS_LENGTH			0x00000000 /* variable-size */
#define RTS_CMD_ASSOCIATION_GROUP_ID_LENGTH		0x00000010
#define RTS_CMD_DESTINATION_LENGTH			0x00000004
#define RTS_CMD_PING_TRAFFIC_SENT_NOTIFY_LENGTH		0x00000004

#define FDClient					0x00000000
#define FDInProxy					0x00000001
#define FDServer					0x00000002
#define FDOutProxy					0x00000003

FREERDP_LOCAL void rts_generate_cookie(BYTE* cookie);

FREERDP_LOCAL int rts_command_length(rdpRpc* rpc, UINT32 CommandType,
                                     BYTE* buffer, UINT32 length);

FREERDP_LOCAL int rts_send_CONN_A1_pdu(rdpRpc* rpc);
FREERDP_LOCAL int rts_recv_CONN_A3_pdu(rdpRpc* rpc, BYTE* buffer,
                                       UINT32 length);

FREERDP_LOCAL int rts_send_CONN_B1_pdu(rdpRpc* rpc);

FREERDP_LOCAL int rts_recv_CONN_C2_pdu(rdpRpc* rpc, BYTE* buffer,
                                       UINT32 length);

FREERDP_LOCAL int rts_send_OUT_R1_A3_pdu(rdpRpc* rpc);

FREERDP_LOCAL int rts_send_flow_control_ack_pdu(rdpRpc* rpc);

FREERDP_LOCAL int rts_recv_out_of_sequence_pdu(rdpRpc* rpc, BYTE* buffer,
        UINT32 length);

#include "rts_signature.h"

#endif /* FREERDP_LIB_CORE_GATEWAY_RTS_H */
