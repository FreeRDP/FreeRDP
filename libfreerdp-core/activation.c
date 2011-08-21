/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Activation Sequence
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

#include "activation.h"

uint8 CTRLACTION_STRINGS[][32] =
{
		"",
		"CTRLACTION_REQUEST_CONTROL",
		"CTRLACTION_GRANTED_CONTROL",
		"CTRLACTION_DETACH",
		"CTRLACTION_COOPERATE"
};

boolean rdp_client_activate(rdpRdp* rdp)
{
	while (rdp->activated != True)
	{
		rdp_recv(rdp);
	}

	printf("client is activated\n");

	return True;
}

void rdp_write_synchronize_pdu(STREAM* s, rdpSettings* settings)
{
	stream_write_uint16(s, SYNCMSGTYPE_SYNC); /* messageType (2 bytes) */
	stream_write_uint16(s, settings->pdu_source); /* targetUser (2 bytes) */
}

void rdp_recv_server_synchronize_pdu(rdpRdp* rdp, STREAM* s, rdpSettings* settings)
{
	rdp_send_client_control_pdu(rdp, CTRLACTION_COOPERATE);
}

boolean rdp_send_server_synchronize_pdu(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	rdp_write_synchronize_pdu(s, rdp->settings);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SYNCHRONIZE, rdp->mcs->user_id);

	return True;
}

boolean rdp_read_client_synchronize_pdu(STREAM* s)
{
	uint16 messageType;

	if (stream_get_left(s) < 4)
		return False;

	stream_read_uint16(s, messageType); /* messageType (2 bytes) */
	if (messageType != SYNCMSGTYPE_SYNC)
		return False;
	/* targetUser (2 bytes) */

	return True;
}

boolean rdp_send_client_synchronize_pdu(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	rdp_write_synchronize_pdu(s, rdp->settings);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SYNCHRONIZE, rdp->mcs->user_id);

	return True;
}

boolean rdp_read_control_pdu(STREAM* s, uint16* action)
{
	if (stream_get_left(s) < 8)
		return False;

	stream_read_uint16(s, *action); /* action (2 bytes) */
	stream_seek_uint16(s); /* grantId (2 bytes) */
	stream_seek_uint32(s); /* controlId (4 bytes) */

	return True;
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

	rdp_read_control_pdu(s, &action);

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

boolean rdp_send_server_control_cooperate_pdu(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	stream_write_uint16(s, CTRLACTION_COOPERATE); /* action (2 bytes) */
	stream_write_uint16(s, 0); /* grantId (2 bytes) */
	stream_write_uint32(s, 0); /* controlId (4 bytes) */

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_CONTROL, rdp->mcs->user_id);

	return True;
}

boolean rdp_send_server_control_granted_pdu(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	stream_write_uint16(s, CTRLACTION_GRANTED_CONTROL); /* action (2 bytes) */
	stream_write_uint16(s, rdp->mcs->user_id); /* grantId (2 bytes) */
	stream_write_uint32(s, 0x03EA); /* controlId (4 bytes) */

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_CONTROL, rdp->mcs->user_id);

	return True;
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

boolean rdp_read_client_font_list_pdu(STREAM* s)
{
	if (stream_get_left(s) < 8)
		return False;

	return True;
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
	update_reset_state(rdp->update);
	rdp->update->switch_surface.bitmapId = SCREEN_BITMAP_SURFACE;
	IFCALL(rdp->update->SwitchSurface, rdp->update, &(rdp->update->switch_surface));
}

boolean rdp_send_server_font_map_pdu(rdpRdp* rdp)
{
	STREAM* s;

	s = rdp_data_pdu_init(rdp);

	stream_write_uint16(s, 0); /* numberEntries (2 bytes) */
	stream_write_uint16(s, 0); /* totalNumEntries (2 bytes) */
	stream_write_uint16(s, FONTLIST_FIRST | FONTLIST_LAST); /* mapFlags (2 bytes) */
	stream_write_uint16(s, 4); /* entrySize (2 bytes) */

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_FONT_MAP, rdp->mcs->user_id);

	return True;
}

void rdp_recv_deactivate_all(rdpRdp* rdp, STREAM* s)
{
	uint16 lengthSourceDescriptor;

	printf("Deactivate All PDU\n");

	stream_read_uint32(s, rdp->settings->share_id); /* shareId (4 bytes) */
	stream_read_uint16(s, lengthSourceDescriptor); /* lengthSourceDescriptor (2 bytes) */
	stream_seek(s, lengthSourceDescriptor); /* sourceDescriptor (should be 0x00) */

	rdp->activated = False;
}

boolean rdp_server_accept_client_control_pdu(rdpRdp* rdp, STREAM* s)
{
	uint16 action;

	if (!rdp_read_control_pdu(s, &action))
		return False;
	if (action == CTRLACTION_REQUEST_CONTROL)
	{
		if (!rdp_send_server_control_granted_pdu(rdp))
			return False;
	}
	return True;
}

boolean rdp_server_accept_client_font_list_pdu(rdpRdp* rdp, STREAM* s)
{
	if (!rdp_read_client_font_list_pdu(s))
		return False;
	if (!rdp_send_server_font_map_pdu(rdp))
		return False;

	return True;
}

