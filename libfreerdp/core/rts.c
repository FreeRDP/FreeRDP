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
 * [MS-RPCH]: Remote Procedure Call over HTTP Protocol Specification:
 * http://msdn.microsoft.com/en-us/library/cc243950/
 */

static RtsPduSignature RTS_PDU_CONN_A1_SIGNATURE;
static RtsPduSignature RTS_PDU_CONN_A2_SIGNATURE;
static RtsPduSignature RTS_PDU_CONN_A3_SIGNATURE;

static RtsPduSignature RTS_PDU_CONN_B1_SIGNATURE;
static RtsPduSignature RTS_PDU_CONN_B2_SIGNATURE;
static RtsPduSignature RTS_PDU_CONN_B3_SIGNATURE;

static RtsPduSignature RTS_PDU_CONN_C1_SIGNATURE;
static RtsPduSignature RTS_PDU_CONN_C2_SIGNATURE;

static RtsPduSignature RTS_PDU_IN_R1_A1_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R1_A2_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R1_A3_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R1_A4_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R1_A5_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R1_A6_SIGNATURE;

static RtsPduSignature RTS_PDU_IN_R1_B1_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R1_B2_SIGNATURE;

static RtsPduSignature RTS_PDU_IN_R2_A1_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R2_A2_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R2_A3_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R2_A4_SIGNATURE;
static RtsPduSignature RTS_PDU_IN_R2_A5_SIGNATURE;

static RtsPduSignature RTS_PDU_OUT_R1_A1_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A2_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A3_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A4_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A5_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A6_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A7_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A8_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A9_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A10_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R1_A11_SIGNATURE;

static RtsPduSignature RTS_PDU_OUT_R2_A1_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_A2_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_A3_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_A4_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_A5_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_A6_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_A7_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_A8_SIGNATURE;

static RtsPduSignature RTS_PDU_OUT_R2_B1_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_B2_SIGNATURE;
static RtsPduSignature RTS_PDU_OUT_R2_B3_SIGNATURE;

static RtsPduSignature RTS_PDU_OUT_R2_C1_SIGNATURE;

static RtsPduSignature RTS_PDU_KEEP_ALIVE_SIGNATURE;
static RtsPduSignature RTS_PDU_PING_TRAFFIC_SENT_NOTIFY_SIGNATURE;
static RtsPduSignature RTS_PDU_ECHO_SIGNATURE;
static RtsPduSignature RTS_PDU_PING_SIGNATURE;
static RtsPduSignature RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE;
static RtsPduSignature RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION_SIGNATURE;

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

BOOL rts_connect(rdpRpc* rpc)
{
	int status;
	rpcconn_rts_hdr_t* rts;
	HttpResponse* http_response;

	/**
	 * Connection Opening
	 *
	 * When opening a virtual connection to the server, an implementation of this protocol MUST perform
	 * the following sequence of steps:
	 *
	 * 1. Send an IN channel request as specified in section 2.1.2.1.1, containing the connection timeout,
	 *    ResourceType UUID, and Session UUID values, if any, supplied by the higher-layer protocol or application.
	 *
	 * 2. Send an OUT channel request as specified in section 2.1.2.1.2.
	 *
	 * 3. Send a CONN/A1 RTS PDU as specified in section 2.2.4.2
	 *
	 * 4. Send a CONN/B1 RTS PDU as specified in section 2.2.4.5
	 *
	 * 5. Wait for the connection establishment protocol sequence as specified in 3.2.1.5.3.1 to complete
	 *
	 * An implementation MAY execute steps 1 and 2 in parallel. An implementation SHOULD execute steps
	 * 3 and 4 in parallel. An implementation MUST execute step 3 after completion of step 1 and execute
	 * step 4 after completion of step 2.
	 *
	 */

	rpc->VirtualConnection->State = VIRTUAL_CONNECTION_STATE_INITIAL;
	DEBUG_RTS("VIRTUAL_CONNECTION_STATE_INITIAL");

	if (!rpc_ntlm_http_out_connect(rpc))
	{
		printf("rpc_out_connect_http error!\n");
		return FALSE;
	}

	if (rts_send_CONN_A1_pdu(rpc) != 0)
	{
		printf("rpc_send_CONN_A1_pdu error!\n");
		return FALSE;
	}

	if (!rpc_ntlm_http_in_connect(rpc))
	{
		printf("rpc_in_connect_http error!\n");
		return FALSE;
	}

	if (rts_send_CONN_B1_pdu(rpc) != 0)
	{
		printf("rpc_send_CONN_B1_pdu error!\n");
		return FALSE;
	}

	rpc->VirtualConnection->State = VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT;
	DEBUG_RTS("VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT");

	/**
	 * Receive OUT Channel Response
	 *
	 * A client implementation MUST NOT accept the OUT channel HTTP response in any state other than
	 * Out Channel Wait. If received in any other state, this HTTP response is a protocol error. Therefore,
	 * the client MUST consider the virtual connection opening a failure and indicate this to higher layers
	 * in an implementation-specific way. The Microsoft Windows® implementation returns
	 * RPC_S_PROTOCOL_ERROR, as specified in [MS-ERREF], to higher-layer protocols.
	 *
	 * If this HTTP response is received in Out Channel Wait state, the client MUST process the fields of
	 * this response as defined in this section.
	 *
	 * First, the client MUST determine whether the response indicates a success or a failure. If the status
	 * code is set to 200, the client MUST interpret this as a success, and it MUST do the following:
	 *
	 * 1. Ignore the values of all other header fields.
	 *
	 * 2. Transition to Wait_A3W state.
	 *
	 * 3. Wait for network events.
	 *
	 * 4. Skip the rest of the processing in this section.
	 *
	 * If the status code is not set to 200, the client MUST interpret this as a failure and follow the same
	 * processing rules as specified in section 3.2.2.5.6.
	 *
	 */

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

	rpc->VirtualConnection->State = VIRTUAL_CONNECTION_STATE_WAIT_A3W;
	DEBUG_RTS("VIRTUAL_CONNECTION_STATE_WAIT_A3W");

	/**
	 * Receive CONN_A3 RTS PDU
	 *
	 * A client implementation MUST NOT accept the CONN/A3 RTS PDU in any state other than
	 * Wait_A3W. If received in any other state, this PDU is a protocol error and the client
	 * MUST consider the virtual connection opening a failure and indicate this to higher
	 * layers in an implementation-specific way.
	 *
	 * Set the ConnectionTimeout in the Ping Originator of the Client's IN Channel to the
	 * ConnectionTimeout in the CONN/A3 PDU.
	 *
	 * If this RTS PDU is received in Wait_A3W state, the client MUST transition the state
	 * machine to Wait_C2 state and wait for network events.
	 *
	 */

	status = rts_recv_pdu(rpc);

	if (status < 1)
		return FALSE;

	rts = (rpcconn_rts_hdr_t*) rpc->buffer;

	if (!rts_match_pdu_signature(rpc, &RTS_PDU_CONN_A3_SIGNATURE, rts))
	{
		printf("Unexpected RTS PDU: Expected CONN/A3\n");
		return FALSE;
	}

	rts_recv_CONN_A3_pdu(rpc, rpc->buffer, rpc->length);

	rpc->VirtualConnection->State = VIRTUAL_CONNECTION_STATE_WAIT_C2;
	DEBUG_RTS("VIRTUAL_CONNECTION_STATE_WAIT_C2");

	/**
	 * Receive CONN_C2 RTS PDU
	 *
	 * A client implementation MUST NOT accept the CONN/C2 RTS PDU in any state other than Wait_C2.
	 * If received in any other state, this PDU is a protocol error and the client MUST consider the virtual
	 * connection opening a failure and indicate this to higher layers in an implementation-specific way.
	 *
	 * If this RTS PDU is received in Wait_C2 state, the client implementation MUST do the following:
	 *
	 * 1. Transition the state machine to opened state.
	 *
	 * 2. Set the connection time-out protocol variable to the value of the ConnectionTimeout field from
	 *    the CONN/C2 RTS PDU.
	 *
	 * 3. Set the PeerReceiveWindow value in the SendingChannel of the Client IN Channel to the
	 *    ReceiveWindowSize value in the CONN/C2 PDU.
	 *
	 * 4. Indicate to higher-layer protocols that the virtual connection opening is a success.
	 *
	 */

	status = rts_recv_pdu(rpc);

	if (status < 1)
		return FALSE;

	rts = (rpcconn_rts_hdr_t*) rpc->buffer;

	if (!rts_match_pdu_signature(rpc, &RTS_PDU_CONN_C2_SIGNATURE, rts))
	{
		printf("Unexpected RTS PDU: Expected CONN/C2\n");
		return FALSE;
	}

	rts_recv_CONN_C2_pdu(rpc, rpc->buffer, rpc->length);

	rpc->VirtualConnection->State = VIRTUAL_CONNECTION_STATE_OPENED;
	DEBUG_RTS("VIRTUAL_CONNECTION_STATE_OPENED");

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
	RAND_pseudo_bytes(cookie, 16);
}

/* CONN/A Sequence */

int rts_send_CONN_A1_pdu(rdpRpc* rpc)
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

	return 0;
}

int rts_recv_CONN_A3_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	UINT32 ConnectionTimeout;

	rts_connection_timeout_command_read(rpc, &buffer[24], length - 24, &ConnectionTimeout);

	DEBUG_RTS("ConnectionTimeout: %d", ConnectionTimeout);

	rpc->VirtualConnection->DefaultInChannel->PingOriginator.ConnectionTimeout = ConnectionTimeout;

	return 0;
}

/* CONN/B Sequence */

int rts_send_CONN_B1_pdu(rdpRpc* rpc)
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

	buffer = (BYTE*) malloc(header.frag_length);

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_version_command_write(&buffer[20]); /* Version (8 bytes) */
	rts_cookie_command_write(&buffer[28], VirtualConnectionCookie); /* VirtualConnectionCookie (20 bytes) */
	rts_cookie_command_write(&buffer[48], INChannelCookie); /* INChannelCookie (20 bytes) */
	rts_channel_lifetime_command_write(&buffer[68], rpc->ChannelLifetime); /* ChannelLifetime (8 bytes) */
	rts_client_keepalive_command_write(&buffer[76], rpc->KeepAliveInterval); /* ClientKeepalive (8 bytes) */
	rts_association_group_id_command_write(&buffer[84], AssociationGroupId); /* AssociationGroupId (20 bytes) */

	rpc_in_write(rpc, buffer, header.frag_length);

	free(buffer);

	return 0;
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

	DEBUG_RTS("ConnectionTimeout: %d", ConnectionTimeout);
	DEBUG_RTS("ReceiveWindowSize: %d", ReceiveWindowSize);

	/* TODO: verify if this is the correct protocol variable */
	rpc->VirtualConnection->DefaultInChannel->PingOriginator.ConnectionTimeout = ConnectionTimeout;

	rpc->VirtualConnection->DefaultInChannel->PeerReceiveWindow = ReceiveWindowSize;

	rpc->VirtualConnection->DefaultInChannel->State = CLIENT_IN_CHANNEL_STATE_OPENED;
	rpc->VirtualConnection->DefaultOutChannel->State = CLIENT_OUT_CHANNEL_STATE_OPENED;

	return 0;
}

/* Out-of-Sequence PDUs */

int rts_send_keep_alive_pdu(rdpRpc* rpc)
{
	BYTE* buffer;
	rpcconn_rts_hdr_t header;

	rts_pdu_header_init(&header);
	header.frag_length = 28;
	header.Flags = RTS_FLAG_OTHER_CMD;
	header.NumberOfCommands = 1;

	DEBUG_RPC("Sending Keep-Alive RTS PDU");

	buffer = (BYTE*) malloc(header.frag_length);
	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_client_keepalive_command_write(&buffer[20], rpc->CurrentKeepAliveInterval); /* ClientKeepAlive (8 bytes) */

	rpc_in_write(rpc, buffer, header.frag_length);

	free(buffer);

	return 0;
}

int rts_send_flow_control_ack_pdu(rdpRpc* rpc)
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
	AvailableWindow = rpc->VirtualConnection->DefaultOutChannel->AvailableWindowAdvertised;
	ChannelCookie = (BYTE*) &(rpc->VirtualConnection->DefaultOutChannelCookie);

	rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow =
			rpc->VirtualConnection->DefaultOutChannel->AvailableWindowAdvertised;

	buffer = (BYTE*) malloc(header.frag_length);

	CopyMemory(buffer, ((BYTE*) &header), 20); /* RTS Header (20 bytes) */
	rts_destination_command_write(&buffer[20], FDOutProxy); /* Destination Command (8 bytes) */

	/* FlowControlAck Command (28 bytes) */
	rts_flow_control_ack_command_write(&buffer[28], BytesReceived, AvailableWindow, ChannelCookie);

	rpc_in_write(rpc, buffer, header.frag_length);

	free(buffer);

	return 0;
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

	printf("Destination: %d BytesReceived: %d AvailableWindow: %d\n",
			Destination, BytesReceived, AvailableWindow);

	return 0;
}

int rts_send_ping_pdu(rdpRpc* rpc)
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

	return 0;
}

static RtsPduSignature RTS_PDU_CONN_A1_SIGNATURE = { RTS_FLAG_NONE, 4,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_CONN_A2_SIGNATURE = { RTS_FLAG_OUT_CHANNEL, 5,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_CHANNEL_LIFETIME,
			RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_CONN_A3_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_CONN_B1_SIGNATURE = { RTS_FLAG_NONE, 6,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_CHANNEL_LIFETIME,
			RTS_CMD_CLIENT_KEEPALIVE, RTS_CMD_ASSOCIATION_GROUP_ID, 0, 0 } };
static RtsPduSignature RTS_PDU_CONN_B2_SIGNATURE = { RTS_FLAG_IN_CHANNEL, 7,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_RECEIVE_WINDOW_SIZE,
			RTS_CMD_CONNECTION_TIMEOUT, RTS_CMD_ASSOCIATION_GROUP_ID, RTS_CMD_CLIENT_ADDRESS, 0 } };
static RtsPduSignature RTS_PDU_CONN_B3_SIGNATURE = { RTS_FLAG_NONE, 2,
		{ RTS_CMD_RECEIVE_WINDOW_SIZE, RTS_CMD_VERSION, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_CONN_C1_SIGNATURE = { RTS_FLAG_NONE, 3,
		{ RTS_CMD_VERSION, RTS_CMD_RECEIVE_WINDOW_SIZE, RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_CONN_C2_SIGNATURE = { RTS_FLAG_NONE, 3,
		{ RTS_CMD_VERSION, RTS_CMD_RECEIVE_WINDOW_SIZE, RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_IN_R1_A1_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL, 4,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R1_A2_SIGNATURE = { RTS_FLAG_NONE, 4,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE,
			RTS_CMD_RECEIVE_WINDOW_SIZE, RTS_CMD_CONNECTION_TIMEOUT, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R1_A3_SIGNATURE = { RTS_FLAG_NONE, 4,
		{ RTS_CMD_DESTINATION, RTS_CMD_VERSION, RTS_CMD_RECEIVE_WINDOW_SIZE,
			RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R1_A4_SIGNATURE = { RTS_FLAG_NONE, 4,
		{ RTS_CMD_DESTINATION, RTS_CMD_VERSION, RTS_CMD_RECEIVE_WINDOW_SIZE,
			RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R1_A5_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R1_A6_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_IN_R1_B1_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_EMPTY, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R1_B2_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_IN_R2_A1_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL, 4,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R2_A2_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R2_A3_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R2_A4_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_IN_R2_A5_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_OUT_R1_A1_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL, 1,
		{ RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A2_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL, 1,
		{ RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A3_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL, 5,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE,
			RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A4_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL | RTS_FLAG_OUT_CHANNEL, 7,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_CHANNEL_LIFETIME,
			RTS_CMD_RECEIVE_WINDOW_SIZE, RTS_CMD_CONNECTION_TIMEOUT, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A5_SIGNATURE = { RTS_FLAG_OUT_CHANNEL, 3,
		{ RTS_CMD_DESTINATION, RTS_CMD_VERSION, RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A6_SIGNATURE = { RTS_FLAG_OUT_CHANNEL, 3,
		{ RTS_CMD_DESTINATION, RTS_CMD_VERSION, RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A7_SIGNATURE = { RTS_FLAG_OUT_CHANNEL, 2,
		{ RTS_CMD_DESTINATION, RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A8_SIGNATURE = { RTS_FLAG_OUT_CHANNEL, 2,
		{ RTS_CMD_DESTINATION, RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A9_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A10_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R1_A11_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_OUT_R2_A1_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL, 1,
		{ RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_A2_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL, 1,
		{ RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_A3_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL, 5,
		{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE,
			RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_A4_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_A5_SIGNATURE = { RTS_FLAG_NONE, 2,
		{ RTS_CMD_DESTINATION, RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_A6_SIGNATURE = { RTS_FLAG_NONE, 2,
		{ RTS_CMD_DESTINATION, RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_A7_SIGNATURE = { RTS_FLAG_NONE, 3,
		{ RTS_CMD_DESTINATION, RTS_CMD_COOKIE, RTS_CMD_VERSION, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_A8_SIGNATURE = { RTS_FLAG_OUT_CHANNEL, 2,
		{ RTS_CMD_DESTINATION, RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_OUT_R2_B1_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_B2_SIGNATURE = { RTS_FLAG_NONE, 1,
		{ RTS_CMD_NEGATIVE_ANCE, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_OUT_R2_B3_SIGNATURE = { RTS_FLAG_EOF, 1,
		{ RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_OUT_R2_C1_SIGNATURE = { RTS_FLAG_PING, 1,
		{ 0, 0, 0, 0, 0, 0, 0, 0 } };

static RtsPduSignature RTS_PDU_KEEP_ALIVE_SIGNATURE = { RTS_FLAG_OTHER_CMD, 1,
		{ RTS_CMD_CLIENT_KEEPALIVE, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_PING_TRAFFIC_SENT_NOTIFY_SIGNATURE = { RTS_FLAG_OTHER_CMD, 1,
		{ RTS_CMD_PING_TRAFFIC_SENT_NOTIFY, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_ECHO_SIGNATURE = { RTS_FLAG_ECHO, 0,
		{ 0, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_PING_SIGNATURE = { RTS_FLAG_PING, 0,
		{ 0, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE = { RTS_FLAG_OTHER_CMD, 1,
		{ RTS_CMD_FLOW_CONTROL_ACK, 0, 0, 0, 0, 0, 0, 0 } };
static RtsPduSignature RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION_SIGNATURE = { RTS_FLAG_OTHER_CMD, 2,
		{ RTS_CMD_DESTINATION, RTS_CMD_FLOW_CONTROL_ACK, 0, 0, 0, 0, 0, 0 } };

struct _RTS_PDU_SIGNATURE_ENTRY
{
	UINT32 SignatureId;
	RtsPduSignature* Signature;
	const char* PduName;
};
typedef struct _RTS_PDU_SIGNATURE_ENTRY RTS_PDU_SIGNATURE_ENTRY;

RTS_PDU_SIGNATURE_ENTRY RTS_PDU_SIGNATURE_TABLE[] =
{
	{ RTS_PDU_CONN_A1, &RTS_PDU_CONN_A1_SIGNATURE, "CONN/A1" },
	{ RTS_PDU_CONN_A2, &RTS_PDU_CONN_A2_SIGNATURE, "CONN/A2" },
	{ RTS_PDU_CONN_A3, &RTS_PDU_CONN_A3_SIGNATURE, "CONN/A3" },

	{ RTS_PDU_CONN_B1, &RTS_PDU_CONN_B1_SIGNATURE, "CONN/B1" },
	{ RTS_PDU_CONN_B2, &RTS_PDU_CONN_B2_SIGNATURE, "CONN/B2" },
	{ RTS_PDU_CONN_B3, &RTS_PDU_CONN_B3_SIGNATURE, "CONN/B3" },

	{ RTS_PDU_CONN_C1, &RTS_PDU_CONN_C1_SIGNATURE, "CONN/C1" },
	{ RTS_PDU_CONN_C2, &RTS_PDU_CONN_C2_SIGNATURE, "CONN/C2" },

	{ RTS_PDU_IN_R1_A1, &RTS_PDU_IN_R1_A1_SIGNATURE, "IN_R1/A1" },
	{ RTS_PDU_IN_R1_A2, &RTS_PDU_IN_R1_A2_SIGNATURE, "IN_R1/A2" },
	{ RTS_PDU_IN_R1_A3, &RTS_PDU_IN_R1_A3_SIGNATURE, "IN_R1/A3" },
	{ RTS_PDU_IN_R1_A4, &RTS_PDU_IN_R1_A4_SIGNATURE, "IN_R1/A4" },
	{ RTS_PDU_IN_R1_A5, &RTS_PDU_IN_R1_A5_SIGNATURE, "IN_R1/A5" },
	{ RTS_PDU_IN_R1_A6, &RTS_PDU_IN_R1_A6_SIGNATURE, "IN_R1/A6" },

	{ RTS_PDU_IN_R1_B1, &RTS_PDU_IN_R1_B1_SIGNATURE, "IN_R1/B1" },
	{ RTS_PDU_IN_R1_B2, &RTS_PDU_IN_R1_B2_SIGNATURE, "IN_R1/B2" },

	{ RTS_PDU_IN_R2_A1, &RTS_PDU_IN_R2_A1_SIGNATURE, "IN_R2/A1" },
	{ RTS_PDU_IN_R2_A2, &RTS_PDU_IN_R2_A2_SIGNATURE, "IN_R2/A2" },
	{ RTS_PDU_IN_R2_A3, &RTS_PDU_IN_R2_A3_SIGNATURE, "IN_R2/A3" },
	{ RTS_PDU_IN_R2_A4, &RTS_PDU_IN_R2_A4_SIGNATURE, "IN_R2/A4" },
	{ RTS_PDU_IN_R2_A5, &RTS_PDU_IN_R2_A5_SIGNATURE, "IN_R2/A5" },

	{ RTS_PDU_OUT_R1_A1, &RTS_PDU_OUT_R1_A1_SIGNATURE, "OUT_R1/A1" },
	{ RTS_PDU_OUT_R1_A2, &RTS_PDU_OUT_R1_A2_SIGNATURE, "OUT_R1/A2" },
	{ RTS_PDU_OUT_R1_A3, &RTS_PDU_OUT_R1_A3_SIGNATURE, "OUT_R1/A3" },
	{ RTS_PDU_OUT_R1_A4, &RTS_PDU_OUT_R1_A4_SIGNATURE, "OUT_R1/A4" },
	{ RTS_PDU_OUT_R1_A5, &RTS_PDU_OUT_R1_A5_SIGNATURE, "OUT_R1/A5" },
	{ RTS_PDU_OUT_R1_A6, &RTS_PDU_OUT_R1_A6_SIGNATURE, "OUT_R1/A6" },
	{ RTS_PDU_OUT_R1_A7, &RTS_PDU_OUT_R1_A7_SIGNATURE, "OUT_R1/A7" },
	{ RTS_PDU_OUT_R1_A8, &RTS_PDU_OUT_R1_A8_SIGNATURE, "OUT_R1/A8" },
	{ RTS_PDU_OUT_R1_A9, &RTS_PDU_OUT_R1_A9_SIGNATURE, "OUT_R1/A9" },
	{ RTS_PDU_OUT_R1_A10, &RTS_PDU_OUT_R1_A10_SIGNATURE, "OUT_R1/A10" },
	{ RTS_PDU_OUT_R1_A11, &RTS_PDU_OUT_R1_A11_SIGNATURE, "OUT_R1/A11" },

	{ RTS_PDU_OUT_R2_A1, &RTS_PDU_OUT_R2_A1_SIGNATURE, "OUT_R2/A1" },
	{ RTS_PDU_OUT_R2_A2, &RTS_PDU_OUT_R2_A2_SIGNATURE, "OUT_R2/A2" },
	{ RTS_PDU_OUT_R2_A3, &RTS_PDU_OUT_R2_A3_SIGNATURE, "OUT_R2/A3" },
	{ RTS_PDU_OUT_R2_A4, &RTS_PDU_OUT_R2_A4_SIGNATURE, "OUT_R2/A4" },
	{ RTS_PDU_OUT_R2_A5, &RTS_PDU_OUT_R2_A5_SIGNATURE, "OUT_R2/A5" },
	{ RTS_PDU_OUT_R2_A6, &RTS_PDU_OUT_R2_A6_SIGNATURE, "OUT_R2/A6" },
	{ RTS_PDU_OUT_R2_A7, &RTS_PDU_OUT_R2_A7_SIGNATURE, "OUT_R2/A7" },
	{ RTS_PDU_OUT_R2_A8, &RTS_PDU_OUT_R2_A8_SIGNATURE, "OUT_R2/A8" },

	{ RTS_PDU_OUT_R2_B1, &RTS_PDU_OUT_R2_B1_SIGNATURE, "OUT_R2/B1" },
	{ RTS_PDU_OUT_R2_B2, &RTS_PDU_OUT_R2_B2_SIGNATURE, "OUT_R2/B2" },
	{ RTS_PDU_OUT_R2_B3, &RTS_PDU_OUT_R2_B3_SIGNATURE, "OUT_R2/B3" },

	{ RTS_PDU_OUT_R2_C1, &RTS_PDU_OUT_R2_C1_SIGNATURE, "OUT_R2/C1" },

	{ RTS_PDU_KEEP_ALIVE, &RTS_PDU_KEEP_ALIVE_SIGNATURE, "Keep-Alive" },
	{ RTS_PDU_PING_TRAFFIC_SENT_NOTIFY, &RTS_PDU_PING_TRAFFIC_SENT_NOTIFY_SIGNATURE, "Ping Traffic Sent Notify" },
	{ RTS_PDU_ECHO, &RTS_PDU_ECHO_SIGNATURE, "Echo" },
	{ RTS_PDU_PING, &RTS_PDU_PING_SIGNATURE, "Ping" },
	{ RTS_PDU_FLOW_CONTROL_ACK, &RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE, "FlowControlAck" },
	{ RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION, &RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION_SIGNATURE, "FlowControlAckWithDestination" },

	{ 0, NULL }
};

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
			printf("Error: Unknown RTS Command Type: 0x%x\n", CommandType);
			return -1;
			break;
	}

	return CommandLength;
}

BOOL rts_match_pdu_signature(rdpRpc* rpc, RtsPduSignature* signature, rpcconn_rts_hdr_t* rts)
{
	int i;
	int status;
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;
	UINT32 CommandType;
	UINT32 CommandLength;

	if (rts->Flags != signature->Flags)
		return FALSE;

	if (rts->NumberOfCommands != signature->NumberOfCommands)
		return FALSE;

	buffer = (BYTE*) rts;
	offset = RTS_PDU_HEADER_LENGTH;
	length = rts->frag_length - offset;

	for (i = 0; i < rts->NumberOfCommands; i++)
	{
		CommandType = *((UINT32*) &buffer[offset]); /* CommandType (4 bytes) */
		offset += 4;

		if (CommandType != signature->CommandTypes[i])
			return FALSE;

		status = rts_command_length(rpc, CommandType, &buffer[offset], length);

		if (status < 0)
			return FALSE;

		CommandLength = (UINT32) status;
		offset += CommandLength;

		length = rts->frag_length - offset;
	}

	return TRUE;
}

int rts_extract_pdu_signature(rdpRpc* rpc, RtsPduSignature* signature, rpcconn_rts_hdr_t* rts)
{
	int i;
	int status;
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;
	UINT32 CommandType;
	UINT32 CommandLength;

	signature->Flags = rts->Flags;
	signature->NumberOfCommands = rts->NumberOfCommands;

	buffer = (BYTE*) rts;
	offset = RTS_PDU_HEADER_LENGTH;
	length = rts->frag_length - offset;

	for (i = 0; i < rts->NumberOfCommands; i++)
	{
		CommandType = *((UINT32*) &buffer[offset]); /* CommandType (4 bytes) */
		offset += 4;

		signature->CommandTypes[i] = CommandType;

		status = rts_command_length(rpc, CommandType, &buffer[offset], length);

		if (status < 0)
			return FALSE;

		CommandLength = (UINT32) status;
		offset += CommandLength;

		length = rts->frag_length - offset;
	}

	return 0;
}

UINT32 rts_identify_pdu_signature(rdpRpc* rpc, RtsPduSignature* signature, RTS_PDU_SIGNATURE_ENTRY** entry)
{
	int i, j;
	RtsPduSignature* pSignature;

	for (i = 0; RTS_PDU_SIGNATURE_TABLE[i].SignatureId != 0; i++)
	{
		pSignature = RTS_PDU_SIGNATURE_TABLE[i].Signature;

		if (signature->Flags == pSignature->Flags)
		{
			if (signature->NumberOfCommands == pSignature->NumberOfCommands)
			{
				for (j = 0; j < signature->NumberOfCommands; j++)
				{
					if (signature->CommandTypes[j] != pSignature->CommandTypes[j])
						continue;
				}

				if (entry)
					*entry = &RTS_PDU_SIGNATURE_TABLE[i];

				return RTS_PDU_SIGNATURE_TABLE[i].SignatureId;
			}
		}
	}

	return 0;
}

int rts_print_pdu_signature(rdpRpc* rpc, RtsPduSignature* signature)
{
	UINT32 SignatureId;
	RTS_PDU_SIGNATURE_ENTRY* entry;

	printf("RTS PDU Signature: Flags: 0x%04X NumberOfCommands: %d\n",
			signature->Flags, signature->NumberOfCommands);

	SignatureId = rts_identify_pdu_signature(rpc, signature, &entry);

	if (SignatureId)
		printf("Identified %s RTS PDU\n", entry->PduName);

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

	return status;
}

int rts_recv_out_of_sequence_pdu(rdpRpc* rpc)
{
	UINT32 SignatureId;
	rpcconn_rts_hdr_t* rts;
	RtsPduSignature signature;

	rts = (rpcconn_rts_hdr_t*) rpc->buffer;

	rts_extract_pdu_signature(rpc, &signature, rts);
	rts_print_pdu_signature(rpc, &signature);
	SignatureId = rts_identify_pdu_signature(rpc, &signature, NULL);

	if (SignatureId == RTS_PDU_FLOW_CONTROL_ACK)
	{

	}
	else if (SignatureId == RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION)
	{
		return rts_recv_flow_control_ack_with_destination_pdu(rpc, rpc->buffer, rpc->length);
	}

	return 0;
}
