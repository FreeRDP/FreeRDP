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

#include "info.h"
#include "per.h"
#include "redirection.h"

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

boolean rdp_read_share_control_header(STREAM* s, uint16* length, uint16* type, uint16* channel_id)
{
	/* Share Control Header */
	stream_read_uint16(s, *length); /* totalLength */
	stream_read_uint16(s, *type); /* pduType */
	stream_read_uint16(s, *channel_id); /* pduSource */
	*type &= 0x0F; /* type is in the 4 least significant bits */

	if (*length - 6 > stream_get_left(s))
		return False;

	return True;
}

void rdp_write_share_control_header(STREAM* s, uint16 length, uint16 type, uint16 channel_id)
{
	length -= RDP_PACKET_HEADER_LENGTH;

	/* Share Control Header */
	stream_write_uint16(s, length); /* totalLength */
	stream_write_uint16(s, type | 0x10); /* pduType */
	stream_write_uint16(s, channel_id); /* pduSource */
}

boolean rdp_read_share_data_header(STREAM* s, uint16* length, uint8* type, uint32* share_id)
{
	if (stream_get_left(s) < 12)
		return False;

	/* Share Data Header */
	stream_read_uint32(s, *share_id); /* shareId (4 bytes) */
	stream_seek_uint8(s); /* pad1 (1 byte) */
	stream_seek_uint8(s); /* streamId (1 byte) */
	stream_read_uint16(s, *length); /* uncompressedLength (2 bytes) */
	stream_read_uint8(s, *type); /* pduType2, Data PDU Type (1 byte) */
	stream_seek_uint8(s); /* compressedType (1 byte) */
	stream_seek_uint16(s); /* compressedLength (2 bytes) */

	return True;
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
 * Read an RDP packet header.\n
 * @param rdp rdp module
 * @param s stream
 * @param length RDP packet length
 * @param channel_id channel id
 */

boolean rdp_read_header(rdpRdp* rdp, STREAM* s, uint16* length, uint16* channel_id)
{
	uint16 initiator;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = (rdp->settings->server_mode ? DomainMCSPDU_SendDataRequest : DomainMCSPDU_SendDataIndication);
	mcs_read_domain_mcspdu_header(s, &MCSPDU, length);

	per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_read_integer16(s, channel_id, 0); /* channelId */
	stream_seek(s, 1); /* dataPriority + Segmentation (0x70) */
	per_read_length(s, length); /* userData (OCTET_STRING) */
	if (*length > stream_get_left(s))
		return False;

	return True;
}

/**
 * Write an RDP packet header.\n
 * @param rdp rdp module
 * @param s stream
 * @param length RDP packet length
 * @param channel_id channel id
 */

void rdp_write_header(rdpRdp* rdp, STREAM* s, uint16 length, uint16 channel_id)
{
	enum DomainMCSPDU MCSPDU;

	MCSPDU = (rdp->settings->server_mode ? DomainMCSPDU_SendDataIndication : DomainMCSPDU_SendDataRequest);

	mcs_write_domain_mcspdu_header(s, MCSPDU, length, 0);
	per_write_integer16(s, rdp->mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator */
	per_write_integer16(s, channel_id, 0); /* channelId */
	stream_write_uint8(s, 0x70); /* dataPriority + segmentation */

	length = (length - RDP_PACKET_HEADER_LENGTH) | 0x8000;
	stream_write_uint16_be(s, length); /* userData (OCTET_STRING) */
}

/**
 * Send an RDP packet.\n
 * @param rdp RDP module
 * @param s stream
 * @param channel_id channel id
 */

boolean rdp_send(rdpRdp* rdp, STREAM* s, uint16 channel_id)
{
	uint16 length;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length, channel_id);

	stream_set_pos(s, length);
	if (transport_write(rdp->transport, s) < 0)
		return False;

	return True;
}

boolean rdp_send_pdu(rdpRdp* rdp, STREAM* s, uint16 type, uint16 channel_id)
{
	uint16 length;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);
	rdp_write_share_control_header(s, length, type, channel_id);

	stream_set_pos(s, length);
	if (transport_write(rdp->transport, s) < 0)
		return False;

	return True;
}

boolean rdp_send_data_pdu(rdpRdp* rdp, STREAM* s, uint8 type, uint16 channel_id)
{
	uint16 length;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);
	rdp_write_share_control_header(s, length, PDU_TYPE_DATA, channel_id);
	rdp_write_share_data_header(s, length, type, rdp->settings->share_id);

	//printf("send %s Data PDU (0x%02X), length:%d\n", DATA_PDU_TYPE_STRINGS[type], type, length);

	stream_set_pos(s, length);
	if (transport_write(rdp->transport, s) < 0)
		return False;

	return True;
}

void rdp_recv_set_error_info_data_pdu(STREAM* s)
{
	uint32 errorInfo;

	stream_read_uint32(s, errorInfo); /* errorInfo (4 bytes) */

	if (errorInfo != ERRINFO_SUCCESS)
		rdp_print_errinfo(errorInfo);
}

void rdp_recv_data_pdu(rdpRdp* rdp, STREAM* s)
{
	uint8 type;
	uint16 length;
	uint32 share_id;

	rdp_read_share_data_header(s, &length, &type, &share_id);

	if (type != DATA_PDU_TYPE_UPDATE)
		printf("recv %s Data PDU (0x%02X), length:%d\n", DATA_PDU_TYPE_STRINGS[type], type, length);

	switch (type)
	{
		case DATA_PDU_TYPE_UPDATE:
			update_recv(rdp->update, s);
			break;

		case DATA_PDU_TYPE_CONTROL:
			rdp_recv_server_control_pdu(rdp, s);
			break;

		case DATA_PDU_TYPE_POINTER:
			break;

		case DATA_PDU_TYPE_INPUT:
			break;

		case DATA_PDU_TYPE_SYNCHRONIZE:
			rdp_recv_server_synchronize_pdu(rdp, s);
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
			rdp_recv_server_font_map_pdu(rdp, s);
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
			rdp_recv_set_error_info_data_pdu(s);
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
 * Process an RDP packet.\n
 * @param rdp RDP module
 * @param s stream
 */

static boolean rdp_recv_tpkt_pdu(rdpRdp* rdp, STREAM* s)
{
	uint16 length;
	uint16 pduType;
	uint16 pduLength;
	uint16 channelId;

	if (!rdp_read_header(rdp, s, &length, &channelId))
	{
		printf("Incorrect RDP header.\n");
		return False;
	}

	if (channelId != MCS_GLOBAL_CHANNEL_ID)
	{
		vchan_process(rdp->vchan, s, channelId);
	}
	else
	{
		rdp_read_share_control_header(s, &pduLength, &pduType, &rdp->settings->pdu_source);

		switch (pduType)
		{
			case PDU_TYPE_DATA:
				rdp_recv_data_pdu(rdp, s);
				break;

			case PDU_TYPE_DEACTIVATE_ALL:
				if (!rdp_recv_deactivate_all(rdp, s))
					return False;
				break;

			case PDU_TYPE_SERVER_REDIRECTION:
				rdp_recv_enhanced_security_redirection_packet(rdp, s);
				break;

			default:
				printf("incorrect PDU type: 0x%04X\n", pduType);
				break;
		}
	}

	return True;
}

static boolean rdp_recv_fastpath_pdu(rdpRdp* rdp, STREAM* s)
{
	uint16 length;

	length = fastpath_read_header(rdp->fastpath, s);
	if (length == 0 || length > stream_get_size(s))
	{
		printf("incorrect FastPath PDU header length %d\n", length);
		return False;
	}

	if (!fastpath_read_security_header(rdp->fastpath, s))
		return False;

	fastpath_recv_updates(rdp->fastpath, s);

	return True;
}

static boolean rdp_recv_pdu(rdpRdp* rdp, STREAM* s)
{
	if (tpkt_verify_header(s))
		return rdp_recv_tpkt_pdu(rdp, s);
	else
		return rdp_recv_fastpath_pdu(rdp, s);
}

/**
 * Receive an RDP packet.\n
 * @param rdp RDP module
 */

void rdp_recv(rdpRdp* rdp)
{
	STREAM* s;

	s = transport_recv_stream_init(rdp->transport, 4096);
	transport_read(rdp->transport, s);

	rdp_recv_pdu(rdp, s);
}

static int rdp_recv_callback(rdpTransport* transport, STREAM* s, void* extra)
{
	rdpRdp* rdp = (rdpRdp*) extra;

	switch (rdp->state)
	{
		case CONNECTION_STATE_NEGO:
			if (!rdp_client_connect_mcs_connect_response(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			if (!rdp_client_connect_mcs_attach_user_confirm(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_CHANNEL_JOIN:
			if (!rdp_client_connect_mcs_channel_join_confirm(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_LICENSE:
			if (!rdp_client_connect_license(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_CAPABILITY:
			if (!rdp_client_connect_demand_active(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_ACTIVE:
			if (!rdp_recv_pdu(rdp, s))
				return -1;
			break;

		default:
			printf("Invalid state %d\n", rdp->state);
			return -1;
	}

	return 1;
}

int rdp_send_channel_data(rdpRdp* rdp, int channel_id, uint8* data, int size)
{
	return vchan_send(rdp->vchan, channel_id, data, size);
}

/**
 * Set non-blocking mode information.
 * @param rdp RDP module
 * @param blocking blocking mode
 */
void rdp_set_blocking_mode(rdpRdp* rdp, boolean blocking)
{
	rdp->transport->recv_callback = rdp_recv_callback;
	rdp->transport->recv_extra = rdp;
	transport_set_blocking_mode(rdp->transport, blocking);
}

int rdp_check_fds(rdpRdp* rdp)
{
	return transport_check_fds(rdp->transport);
}

/**
 * Instantiate new RDP module.
 * @return new RDP module
 */

rdpRdp* rdp_new(freerdp* instance)
{
	rdpRdp* rdp;

	rdp = (rdpRdp*) xzalloc(sizeof(rdpRdp));

	if (rdp != NULL)
	{
		rdp->settings = settings_new();
		rdp->transport = transport_new(rdp->settings);
		rdp->license = license_new(rdp);
		rdp->input = input_new(rdp);
		rdp->update = update_new(rdp);
		rdp->fastpath = fastpath_new(rdp);
		rdp->nego = nego_new(rdp->transport);
		rdp->mcs = mcs_new(rdp->transport);
		rdp->vchan = vchan_new(instance);
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
		input_free(rdp->input);
		update_free(rdp->update);
		fastpath_free(rdp->fastpath);
		mcs_free(rdp->mcs);
		vchan_free(rdp->vchan);
		xfree(rdp);
	}
}

