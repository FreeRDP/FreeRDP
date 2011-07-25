/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Connection Sequence
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "connection.h"

/**
 *                                      Connection Sequence
 *     client                                                                    server
 *        |                                                                         |
 *        |-----------------------X.224 Connection Request PDU--------------------->|
 *        |<----------------------X.224 Connection Confirm PDU----------------------|
 *        |-------MCS Connect-Initial PDU with GCC Conference Create Request------->|
 *        |<-----MCS Connect-Response PDU with GCC Conference Create Response-------|
 *        |------------------------MCS Erect Domain Request PDU-------------------->|
 *        |------------------------MCS Attach User Request PDU--------------------->|
 *        |<-----------------------MCS Attach User Confirm PDU----------------------|
 *        |------------------------MCS Channel Join Request PDU-------------------->|
 *        |<-----------------------MCS Channel Join Confirm PDU---------------------|
 *        |----------------------------Security Exchange PDU----------------------->|
 *        |-------------------------------Client Info PDU-------------------------->|
 *        |<---------------------License Error PDU - Valid Client-------------------|
 *        |<-----------------------------Demand Active PDU--------------------------|
 *        |------------------------------Confirm Active PDU------------------------>|
 *        |-------------------------------Synchronize PDU-------------------------->|
 *        |---------------------------Control PDU - Cooperate---------------------->|
 *        |------------------------Control PDU - Request Control------------------->|
 *        |--------------------------Persistent Key List PDU(s)-------------------->|
 *        |--------------------------------Font List PDU--------------------------->|
 *        |<------------------------------Synchronize PDU---------------------------|
 *        |<--------------------------Control PDU - Cooperate-----------------------|
 *        |<-----------------------Control PDU - Granted Control--------------------|
 *        |<-------------------------------Font Map PDU-----------------------------|
 *
 */

uint8 CTRLACTION_STRINGS[][32] =
{
		"",
		"CTRLACTION_REQUEST_CONTROL",
		"CTRLACTION_GRANTED_CONTROL",
		"CTRLACTION_DETACH",
		"CTRLACTION_COOPERATE"
};

/**
 * Establish RDP Connection.\n
 * @msdn{cc240452}
 * @param rdp RDP module
 */

boolean rdp_client_connect(rdpRdp* rdp)
{
	rdp->settings->autologon = 1;

	nego_init(rdp->nego);
	nego_set_target(rdp->nego, rdp->settings->hostname, 3389);
	nego_set_cookie(rdp->nego, rdp->settings->username);
	nego_set_protocols(rdp->nego, 1, 1, 1);

	if (nego_connect(rdp->nego) != True)
	{
		printf("Error: protocol security negotiation failure\n");
		return False;
	}

	if (rdp->nego->selected_protocol & PROTOCOL_NLA)
		transport_connect_nla(rdp->transport);
	else if (rdp->nego->selected_protocol & PROTOCOL_TLS)
		transport_connect_tls(rdp->transport);
	else if (rdp->nego->selected_protocol & PROTOCOL_RDP)
		transport_connect_rdp(rdp->transport);

	if (mcs_connect(rdp->mcs) != True)
	{
		printf("Error: Multipoint Connection Service (MCS) connection failure\n");
		return False;
	}

	rdp_send_client_info(rdp);

	if (license_connect(rdp->license) != True)
	{
		printf("Error: license connection sequence failure\n");
		return False;
	}

	rdp->licensed = True;

	rdp_client_activate(rdp);

	while(1)
	{
		rdp_recv(rdp);
	}

	return True;
}

boolean rdp_client_activate(rdpRdp* rdp)
{
	while (rdp->activated != True)
	{
		rdp_recv(rdp);
	}

	printf("client is activated\n");

	return True;
}

void rdp_write_client_synchronize_pdu(STREAM* s, rdpSettings* settings)
{
	stream_write_uint16(s, SYNCMSGTYPE_SYNC); /* messageType (2 bytes) */
	stream_write_uint16(s, settings->pdu_source); /* targetUser (2 bytes) */
}

void rdp_recv_server_synchronize_pdu(rdpRdp* rdp, STREAM* s, rdpSettings* settings)
{
	rdp_send_client_control_pdu(rdp, CTRLACTION_COOPERATE);
}

void rdp_send_client_synchronize_pdu(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	rdp_write_client_synchronize_pdu(s, rdp->settings);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SYNCHRONIZE, rdp->mcs->user_id);
}

void rdp_read_server_control_pdu(STREAM* s, uint16* action)
{
	stream_read_uint16(s, *action); /* action (2 bytes) */
	stream_seek_uint16(s); /* grantId (2 bytes) */
	stream_seek_uint32(s); /* controlId (4 bytes) */
}

void rdp_write_client_control_pdu(STREAM* s, uint16 action)
{
	stream_write_uint16(s, action); /* action (2 bytes) */
	stream_write_uint16(s, 0); /* grantId (2 bytes) */
	stream_write_uint32(s, 0); /* controlId (4 bytes) */
}

void rdp_recv_server_control_pdu(rdpRdp* rdp, STREAM* s, rdpSettings* settings)
{
	uint16 action;

	rdp_read_server_control_pdu(s, &action);

	if (action == CTRLACTION_COOPERATE)
	{
		rdp_send_client_control_pdu(rdp, CTRLACTION_REQUEST_CONTROL);
	}
	else if (action == CTRLACTION_GRANTED_CONTROL)
	{
		if (rdp->settings->rdp_version >= 5)
		{
			//rdp_send_client_persistent_key_list_pdu(rdp);
			rdp_send_client_font_list_pdu(rdp, FONTLIST_FIRST | FONTLIST_LAST);
		}
		else
		{
			rdp_send_client_font_list_pdu(rdp, FONTLIST_FIRST);
			rdp_send_client_font_list_pdu(rdp, FONTLIST_LAST);
		}
	}
}

void rdp_send_client_control_pdu(rdpRdp* rdp, uint16 action)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	rdp_write_client_control_pdu(s, action);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_CONTROL, rdp->mcs->user_id);
}

void rdp_write_persistent_list_entry(STREAM* s, uint32 key1, uint32 key2)
{
	stream_write_uint32(s, key1); /* key1 (4 bytes) */
	stream_write_uint32(s, key2); /* key2 (4 bytes) */
}

void rdp_write_client_persistent_key_list_pdu(STREAM* s, rdpSettings* settings)
{
	stream_write_uint16(s, 0); /* numEntriesCache0 (2 bytes) */
	stream_write_uint16(s, 0); /* numEntriesCache1 (2 bytes) */
	stream_write_uint16(s, 0); /* numEntriesCache2 (2 bytes) */
	stream_write_uint16(s, 0); /* numEntriesCache3 (2 bytes) */
	stream_write_uint16(s, 0); /* numEntriesCache4 (2 bytes) */
	stream_write_uint16(s, 0); /* totalEntriesCache0 (2 bytes) */
	stream_write_uint16(s, 0); /* totalEntriesCache1 (2 bytes) */
	stream_write_uint16(s, 0); /* totalEntriesCache2 (2 bytes) */
	stream_write_uint16(s, 0); /* totalEntriesCache3 (2 bytes) */
	stream_write_uint16(s, 0); /* totalEntriesCache4 (2 bytes) */
	stream_write_uint8(s, PERSIST_LAST_PDU); /* bBitMask (1 byte) */
	stream_write_uint8(s, 0); /* pad1 (1 byte) */
	stream_write_uint16(s, 0); /* pad3 (2 bytes) */

	/* entries */
}

void rdp_send_client_persistent_key_list_pdu(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	rdp_write_client_persistent_key_list_pdu(s, rdp->settings);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_BITMAP_CACHE_PERSISTENT_LIST, rdp->mcs->user_id);
}

void rdp_write_client_font_list_pdu(STREAM* s, uint16 flags)
{
	stream_write_uint16(s, 0); /* numberFonts (2 bytes) */
	stream_write_uint16(s, 0); /* totalNumFonts (2 bytes) */
	stream_write_uint16(s, flags); /* listFlags (2 bytes) */
	stream_write_uint16(s, 50); /* entrySize (2 bytes) */
}

void rdp_send_client_font_list_pdu(rdpRdp* rdp, uint16 flags)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	rdp_write_client_font_list_pdu(s, flags);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_FONT_LIST, rdp->mcs->user_id);
}

void rdp_recv_server_font_map_pdu(rdpRdp* rdp, STREAM* s, rdpSettings* settings)
{
	rdp->activated = True;
}
