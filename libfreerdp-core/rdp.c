/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Core
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

#include "rdp.h"

uint8 DATA_PDU_TYPE_STRINGS[][32] =
{
		"", "", /* 0x00 - 0x01 */
		"Update", /* 0x02 */
		"", "", "", "", "", "", "", "", /* 0x03 - 0x0A */
		"", "", "", "", "", "", "", "", "", /* 0x0B - 0x13 */
		"Control", /* 0x14 */
		"", "", "", "", "", "", /* 0x15 - 0x1A */
		"Pointer", /* 0x1B */
		"Input", /* 0x1C */
		"", "", /* 0x1D - 0x1E */
		"Synchronize", /* 0x1F */
		"", /* 0x20 */
		"Refresh Rect", /* 0x21 */
		"Play Sound", /* 0x22 */
		"Suppress Output", /* 0x23 */
		"Shutdown Request", /* 0x24 */
		"Shutdown Denied", /* 0x25 */
		"Save Session Info", /* 0x26 */
		"Font List", /* 0x27 */
		"Font Map", /* 0x28 */
		"Set Keyboard Indicators", /* 0x29 */
		"", /* 0x2A */
		"Bitmap Cache Persistent List", /* 0x2B */
		"Bitmap Cache Error", /* 0x2C */
		"Set Keyboard IME Status", /* 0x2D */
		"Offscreen Cache Error", /* 0x2E */
		"Set Error Info", /* 0x2F */
		"Draw Nine Grid Error", /* 0x30 */
		"Draw GDI+ Error", /* 0x31 */
		"ARC Status", /* 0x32 */
		"", "", "", /* 0x33 - 0x35 */
		"Status Info", /* 0x36 */
		"Monitor Layout" /* 0x37 */
		"", "", "", /* 0x38 - 0x40 */
		"", "", "", "", "", "" /* 0x41 - 0x46 */
};

/**
 * Read RDP Security Header.\n
 * @msdn{cc240579}
 * @param s stream
 * @param flags security flags
 */

void rdp_read_security_header(STREAM* s, uint16* flags)
{
	/* Basic Security Header */
	stream_read_uint16(s, *flags); /* flags */
	stream_seek(s, 2); /* flagsHi (unused) */
}

/**
 * Write RDP Security Header.\n
 * @msdn{cc240579}
 * @param s stream
 * @param flags security flags
 */

void rdp_write_security_header(STREAM* s, uint16 flags)
{
	/* Basic Security Header */
	stream_write_uint16(s, flags); /* flags */
	stream_write_uint16(s, 0); /* flagsHi (unused) */
}

void rdp_read_share_control_header(STREAM* s, uint16* length, uint16* type, uint16* channel_id)
{
	/* Share Control Header */
	stream_read_uint16(s, *length); /* totalLength */
	stream_read_uint16(s, *type); /* pduType */
	stream_read_uint16(s, *channel_id); /* pduSource */
	*type &= 0x0F; /* type is in the 4 least significant bits */
}

void rdp_write_share_control_header(STREAM* s, uint16 length, uint16 type, uint16 channel_id)
{
	length -= RDP_PACKET_HEADER_LENGTH;

	/* Share Control Header */
	stream_write_uint16(s, length); /* totalLength */
	stream_write_uint16(s, type | 0x10); /* pduType */
	stream_write_uint16(s, channel_id); /* pduSource */
}

void rdp_read_share_data_header(STREAM* s, uint16* length, uint8* type, uint32* share_id)
{
	/* Share Data Header */
	stream_read_uint32(s, *share_id); /* shareId (4 bytes) */
	stream_seek_uint8(s); /* pad1 (1 byte) */
	stream_seek_uint8(s); /* streamId (1 byte) */
	stream_read_uint16(s, *length); /* uncompressedLength (2 bytes) */
	stream_read_uint8(s, *type); /* pduType2, Data PDU Type (1 byte) */
	stream_seek_uint8(s); /* compressedType (1 byte) */
	stream_seek_uint16(s); /* compressedLength (2 bytes) */
}

void rdp_write_share_data_header(STREAM* s, uint16 length, uint8 type, uint32 share_id)
{
	length -= RDP_PACKET_HEADER_LENGTH;
	length -= RDP_SHARE_CONTROL_HEADER_LENGTH;
	length -= RDP_SHARE_DATA_HEADER_LENGTH;

	/* Share Data Header */
	stream_write_uint32(s, share_id); /* shareId (4 bytes) */
	stream_write_uint8(s, 0); /* pad1 (1 byte) */
	stream_write_uint8(s, STREAM_LOW); /* streamId (1 byte) */
	stream_write_uint16(s, length); /* uncompressedLength (2 bytes) */
	stream_write_uint8(s, type); /* pduType2, Data PDU Type (1 byte) */
	stream_write_uint8(s, 0); /* compressedType (1 byte) */
	stream_write_uint16(s, 0); /* compressedLength (2 bytes) */
}

/**
 * Initialize an RDP packet stream.\n
 * @param rdp rdp module
 * @return
 */

STREAM* rdp_send_stream_init(rdpRdp* rdp)
{
	STREAM* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	stream_seek(s, RDP_PACKET_HEADER_LENGTH);
	return s;
}

STREAM* rdp_pdu_init(rdpRdp* rdp)
{
	STREAM* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	stream_seek(s, RDP_PACKET_HEADER_LENGTH);
	stream_seek(s, RDP_SHARE_CONTROL_HEADER_LENGTH);
	return s;
}

STREAM* rdp_data_pdu_init(rdpRdp* rdp)
{
	STREAM* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	stream_seek(s, RDP_PACKET_HEADER_LENGTH);
	stream_seek(s, RDP_SHARE_CONTROL_HEADER_LENGTH);
	stream_seek(s, RDP_SHARE_DATA_HEADER_LENGTH);
	return s;
}

/**
 * Write an RDP packet header.\n
 * @param rdp rdp module
 * @param s stream
 * @param length RDP packet length
 */

void rdp_write_header(rdpRdp* rdp, STREAM* s, int length)
{
	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_SendDataRequest, length);
	per_write_integer16(s, rdp->mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator */
	per_write_integer16(s, MCS_GLOBAL_CHANNEL_ID, 0); /* channelId */
	stream_write_uint8(s, 0x70); /* dataPriority + segmentation */

	length = (length - RDP_PACKET_HEADER_LENGTH) | 0x8000;
	stream_write_uint16_be(s, length); /* userData (OCTET_STRING) */
}

/**
 * Send an RDP packet.\n
 * @param rdp RDP module
 * @param s stream
 */

void rdp_send(rdpRdp* rdp, STREAM* s)
{
	int length;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length);

	stream_set_pos(s, length);
	transport_write(rdp->transport, s);
}

void rdp_send_pdu(rdpRdp* rdp, STREAM* s, uint16 type, uint16 channel_id)
{
	int length;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length);
	rdp_write_share_control_header(s, length, type, channel_id);

	stream_set_pos(s, length);
	transport_write(rdp->transport, s);
}

void rdp_send_data_pdu(rdpRdp* rdp, STREAM* s, uint16 type, uint16 channel_id)
{
	int length;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length);
	rdp_write_share_control_header(s, length, PDU_TYPE_DATA, channel_id);
	rdp_write_share_data_header(s, length, type, rdp->settings->share_id);

	printf("send %s Data PDU (0x%02X), length:%d\n", DATA_PDU_TYPE_STRINGS[type], type, length);

	stream_set_pos(s, length);
	transport_write(rdp->transport, s);
}

void rdp_read_set_error_info_data_pdu(STREAM* s)
{
	uint32 errorInfo;

	stream_read_uint32(s, errorInfo); /* errorInfo (4 bytes) */

	if (errorInfo != 0)
		printf("Error Info: 0x%08X\n", errorInfo);
}

void rdp_read_data_pdu(rdpRdp* rdp, STREAM* s)
{
	uint8 type;
	uint16 length;
	uint32 share_id;

	rdp_read_share_data_header(s, &length, &type, &share_id);

	printf("recv %s Data PDU (0x%02X), length:%d\n", DATA_PDU_TYPE_STRINGS[type], type, length);

	switch (type)
	{
		case DATA_PDU_TYPE_UPDATE:
			rdp_recv_update_data_pdu(rdp, s);
			break;

		case DATA_PDU_TYPE_CONTROL:
			rdp_recv_server_control_pdu(rdp, s, rdp->settings);
			break;

		case DATA_PDU_TYPE_POINTER:
			break;

		case DATA_PDU_TYPE_INPUT:
			break;

		case DATA_PDU_TYPE_SYNCHRONIZE:
			rdp_recv_server_synchronize_pdu(rdp, s, rdp->settings);
			break;

		case DATA_PDU_TYPE_REFRESH_RECT:
			break;

		case DATA_PDU_TYPE_PLAY_SOUND:
			break;

		case DATA_PDU_TYPE_SUPPRESS_OUTPUT:
			break;

		case DATA_PDU_TYPE_SHUTDOWN_REQUEST:
			break;

		case DATA_PDU_TYPE_SHUTDOWN_DENIED:
			break;

		case DATA_PDU_TYPE_SAVE_SESSION_INFO:
			rdp_recv_save_session_info(rdp, s);
			break;

		case DATA_PDU_TYPE_FONT_LIST:
			break;

		case DATA_PDU_TYPE_FONT_MAP:
			rdp_recv_server_font_map_pdu(rdp, s, rdp->settings);
			break;

		case DATA_PDU_TYPE_SET_KEYBOARD_INDICATORS:
			break;

		case DATA_PDU_TYPE_BITMAP_CACHE_PERSISTENT_LIST:
			break;

		case DATA_PDU_TYPE_BITMAP_CACHE_ERROR:
			break;

		case DATA_PDU_TYPE_SET_KEYBOARD_IME_STATUS:
			break;

		case DATA_PDU_TYPE_OFFSCREEN_CACHE_ERROR:
			break;

		case DATA_PDU_TYPE_SET_ERROR_INFO:
			rdp_read_set_error_info_data_pdu(s);
			break;

		case DATA_PDU_TYPE_DRAW_NINEGRID_ERROR:
			break;

		case DATA_PDU_TYPE_DRAW_GDIPLUS_ERROR:
			break;

		case DATA_PDU_TYPE_ARC_STATUS:
			break;

		case DATA_PDU_TYPE_STATUS_INFO:
			break;

		case DATA_PDU_TYPE_MONITOR_LAYOUT:
			break;

		default:
			break;
	}
}

/**
 * Receive an RDP packet.\n
 * @param rdp RDP module
 */

void rdp_recv(rdpRdp* rdp)
{
	STREAM* s;
	int length;
	uint16 pduType;
	uint16 pduLength;
	uint16 initiator;
	uint16 channelId;
	uint16 sec_flags;
	enum DomainMCSPDU MCSPDU;

	s = transport_recv_stream_init(rdp->transport, 4096);
	transport_read(rdp->transport, s);

	MCSPDU = DomainMCSPDU_SendDataIndication;
	mcs_read_domain_mcspdu_header(s, &MCSPDU, &length);

	per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_read_integer16(s, &channelId, 0); /* channelId */
	stream_seek(s, 1); /* dataPriority + Segmentation (0x70) */
	per_read_length(s, &pduLength); /* userData (OCTET_STRING) */

	if (rdp->licensed != True)
	{
		rdp_read_security_header(s, &sec_flags);

		if (sec_flags & SEC_PKT_MASK)
		{
			switch (sec_flags & SEC_PKT_MASK)
			{
				case SEC_LICENSE_PKT:
					license_recv(rdp->license, s);
					break;

				case SEC_REDIRECTION_PKT:
					rdp_read_redirection_packet(rdp, s);
					break;

				default:
					//printf("incorrect security flags: 0x%04X\n", sec_flags);
					break;
			}
		}
	}
	else
	{
		rdp_read_share_control_header(s, &pduLength, &pduType, &rdp->settings->pdu_source);

		switch (pduType)
		{
			case PDU_TYPE_DATA:
				rdp_read_data_pdu(rdp, s);
				break;

			case PDU_TYPE_DEMAND_ACTIVE:
				rdp_recv_demand_active(rdp, s, rdp->settings);
				break;

			case PDU_TYPE_DEACTIVATE_ALL:
				rdp_recv_deactivate_all(rdp, s);
				break;

			case PDU_TYPE_SERVER_REDIRECTION:
				rdp_read_enhanced_security_redirection_packet(rdp, s);
				break;

			default:
				printf("incorrect PDU type: 0x%04X\n", pduType);
				break;
		}
	}
}

/**
 * Instantiate new RDP module.
 * @return new RDP module
 */

rdpRdp* rdp_new()
{
	rdpRdp* rdp;

	rdp = (rdpRdp*) xzalloc(sizeof(rdpRdp));

	if (rdp != NULL)
	{
		rdp->licensed = False;
		rdp->settings = settings_new();
		rdp->registry = registry_new(rdp->settings);
		rdp->transport = transport_new(rdp->settings);
		rdp->license = license_new(rdp);
		rdp->orders = orders_new();
		rdp->nego = nego_new(rdp->transport);
		rdp->mcs = mcs_new(rdp->transport);
	}

	return rdp;
}

/**
 * Free RDP module.
 * @param rdp RDP module to be freed
 */

void rdp_free(rdpRdp* rdp)
{
	if (rdp != NULL)
	{
		settings_free(rdp->settings);
		transport_free(rdp->transport);
		license_free(rdp->license);
		orders_free(rdp->orders);
		mcs_free(rdp->mcs);
		xfree(rdp);
	}
}

