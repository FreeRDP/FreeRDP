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

static const char* const DATA_PDU_TYPE_STRINGS[] =
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

	if (*length - 2 > stream_get_left(s))
		return false;

	stream_read_uint16(s, *type); /* pduType */
	*type &= 0x0F; /* type is in the 4 least significant bits */

	if (*length > 4)
		stream_read_uint16(s, *channel_id); /* pduSource */
	else /* Windows XP can send such short DEACTIVATE_ALL PDUs. */
		*channel_id = 0;

	return true;
}

void rdp_write_share_control_header(STREAM* s, uint16 length, uint16 type, uint16 channel_id)
{
	length -= RDP_PACKET_HEADER_MAX_LENGTH;

	/* Share Control Header */
	stream_write_uint16(s, length); /* totalLength */
	stream_write_uint16(s, type | 0x10); /* pduType */
	stream_write_uint16(s, channel_id); /* pduSource */
}

boolean rdp_read_share_data_header(STREAM* s, uint16* length, uint8* type, uint32* share_id,
					uint8 *compressed_type, uint16 *compressed_len)
{
	if (stream_get_left(s) < 12)
		return false;

	/* Share Data Header */
	stream_read_uint32(s, *share_id); /* shareId (4 bytes) */
	stream_seek_uint8(s); /* pad1 (1 byte) */
	stream_seek_uint8(s); /* streamId (1 byte) */
	stream_read_uint16(s, *length); /* uncompressedLength (2 bytes) */
	stream_read_uint8(s, *type); /* pduType2, Data PDU Type (1 byte) */

	if (*type & 0x80)
	{
		stream_read_uint8(s, *compressed_type); /* compressedType (1 byte) */
		stream_read_uint16(s, *compressed_len); /* compressedLength (2 bytes) */
	}
	else
	{
		stream_seek(s, 3);
		*compressed_type = 0;
		*compressed_len = 0;
	}

	return true;
}

void rdp_write_share_data_header(STREAM* s, uint16 length, uint8 type, uint32 share_id)
{
	length -= RDP_PACKET_HEADER_MAX_LENGTH;
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

static int rdp_security_stream_init(rdpRdp* rdp, STREAM* s)
{
	if (rdp->do_crypt)
	{
		stream_seek(s, 12);
		if (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS)
			stream_seek(s, 4);
		rdp->sec_flags |= SEC_ENCRYPT;
		if (rdp->do_secure_checksum)
			rdp->sec_flags |= SEC_SECURE_CHECKSUM;
	}
	else if (rdp->sec_flags != 0)
	{
		stream_seek(s, 4);
	}
	return 0;
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
	stream_seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
	rdp_security_stream_init(rdp, s);

	return s;
}

STREAM* rdp_pdu_init(rdpRdp* rdp)
{
	STREAM* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	stream_seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
	rdp_security_stream_init(rdp, s);
	stream_seek(s, RDP_SHARE_CONTROL_HEADER_LENGTH);
	return s;
}

STREAM* rdp_data_pdu_init(rdpRdp* rdp)
{
	STREAM* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	stream_seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
	rdp_security_stream_init(rdp, s);
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

	MCSPDU = (rdp->settings->server_mode) ? DomainMCSPDU_SendDataRequest : DomainMCSPDU_SendDataIndication;
	mcs_read_domain_mcspdu_header(s, &MCSPDU, length);

	if (*length - 8 > stream_get_left(s))
		return false;

	if (MCSPDU == DomainMCSPDU_DisconnectProviderUltimatum)
	{
		uint8 reason;

		(void) per_read_enumerated(s, &reason, 0);

		rdp->disconnect = true;

		return true;
	}

	per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_read_integer16(s, channel_id, 0); /* channelId */
	stream_seek(s, 1); /* dataPriority + Segmentation (0x70) */
	per_read_length(s, length); /* userData (OCTET_STRING) */

	if (*length > stream_get_left(s))
		return false;

	return true;
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
	int body_length;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = (rdp->settings->server_mode) ? DomainMCSPDU_SendDataIndication : DomainMCSPDU_SendDataRequest;

	if ((rdp->sec_flags & SEC_ENCRYPT) && (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS))
	{
		int pad;

		body_length = length - RDP_PACKET_HEADER_MAX_LENGTH - 16;
		pad = 8 - (body_length % 8);
		if (pad != 8)
			length += pad;
	}

	mcs_write_domain_mcspdu_header(s, MCSPDU, length, 0);
	per_write_integer16(s, rdp->mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator */
	per_write_integer16(s, channel_id, 0); /* channelId */
	stream_write_uint8(s, 0x70); /* dataPriority + segmentation */
	/*
	 * We always encode length in two bytes, eventhough we could use
	 * only one byte if length <= 0x7F. It is just easier that way,
	 * because we can leave room for fixed-length header, store all
	 * the data first and then store the header.
	 */
	length = (length - RDP_PACKET_HEADER_MAX_LENGTH) | 0x8000;
	stream_write_uint16_be(s, length); /* userData (OCTET_STRING) */
}

static uint32 rdp_security_stream_out(rdpRdp* rdp, STREAM* s, int length)
{
	uint8* data;
	uint32 sec_flags;
	uint32 pad = 0;

	sec_flags = rdp->sec_flags;

	if (sec_flags != 0)
	{
		rdp_write_security_header(s, sec_flags);

		if (sec_flags & SEC_ENCRYPT)
		{
			if (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS)
			{
				data = s->p + 12;

				length = length - (data - s->data);
				stream_write_uint16(s, 0x10); /* length */
				stream_write_uint8(s, 0x1); /* TSFIPS_VERSION 1*/

				/* handle padding */
				pad = 8 - (length % 8);

				if (pad == 8)
					pad = 0;
				if (pad)
					memset(data+length, 0, pad);

				stream_write_uint8(s, pad);

				security_hmac_signature(data, length, s->p, rdp);
				stream_seek(s, 8);
				security_fips_encrypt(data, length + pad, rdp);
			}
			else
			{
				data = s->p + 8;
				length = length - (data - s->data);
				if (sec_flags & SEC_SECURE_CHECKSUM)
					security_salted_mac_signature(rdp, data, length, true, s->p);
				else
					security_mac_signature(rdp, data, length, s->p);
				stream_seek(s, 8);
				security_encrypt(s->p, length, rdp);
			}
		}

		rdp->sec_flags = 0;
	}

	return pad;
}

static uint32 rdp_get_sec_bytes(rdpRdp* rdp)
{
	uint32 sec_bytes;

	if (rdp->sec_flags & SEC_ENCRYPT)
	{
		sec_bytes = 12;

		if (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS)
			sec_bytes += 4;
	}
	else if (rdp->sec_flags != 0)
	{
		sec_bytes = 4;
	}
	else
	{
		sec_bytes = 0;
	}

	return sec_bytes;
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
	uint32 sec_bytes;
	uint8* sec_hold;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length, channel_id);

	sec_bytes = rdp_get_sec_bytes(rdp);
	sec_hold = s->p;
	stream_seek(s, sec_bytes);

	s->p = sec_hold;
	length += rdp_security_stream_out(rdp, s, length);

	stream_set_pos(s, length);
	if (transport_write(rdp->transport, s) < 0)
		return false;

	return true;
}

boolean rdp_send_pdu(rdpRdp* rdp, STREAM* s, uint16 type, uint16 channel_id)
{
	uint16 length;
	uint32 sec_bytes;
	uint8* sec_hold;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);

	sec_bytes = rdp_get_sec_bytes(rdp);
	sec_hold = s->p;
	stream_seek(s, sec_bytes);

	rdp_write_share_control_header(s, length - sec_bytes, type, channel_id);

	s->p = sec_hold;
	length += rdp_security_stream_out(rdp, s, length);

	stream_set_pos(s, length);
	if (transport_write(rdp->transport, s) < 0)
		return false;

	return true;
}

boolean rdp_send_data_pdu(rdpRdp* rdp, STREAM* s, uint8 type, uint16 channel_id)
{
	uint16 length;
	uint32 sec_bytes;
	uint8* sec_hold;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);

	sec_bytes = rdp_get_sec_bytes(rdp);
	sec_hold = s->p;
	stream_seek(s, sec_bytes);

	rdp_write_share_control_header(s, length - sec_bytes, PDU_TYPE_DATA, channel_id);
	rdp_write_share_data_header(s, length - sec_bytes, type, rdp->settings->share_id);

	s->p = sec_hold;
	length += rdp_security_stream_out(rdp, s, length);

	stream_set_pos(s, length);
	if (transport_write(rdp->transport, s) < 0)
		return false;

	return true;
}

void rdp_recv_set_error_info_data_pdu(rdpRdp* rdp, STREAM* s)
{
	stream_read_uint32(s, rdp->errorInfo); /* errorInfo (4 bytes) */

	if (rdp->errorInfo != ERRINFO_SUCCESS)
		rdp_print_errinfo(rdp->errorInfo);
}

void rdp_recv_data_pdu(rdpRdp* rdp, STREAM* s)
{
	uint8 type;
	uint16 length;
	uint32 share_id;
	uint8 compressed_type;
	uint16 compressed_len;

	rdp_read_share_data_header(s, &length, &type, &share_id, &compressed_type, &compressed_len);

#ifdef WITH_DEBUG_RDP
	if (type != DATA_PDU_TYPE_UPDATE)
		printf("recv %s Data PDU (0x%02X), length:%d\n", DATA_PDU_TYPE_STRINGS[type], type, length);
#endif

	switch (type)
	{
		case DATA_PDU_TYPE_UPDATE:
			update_recv(rdp->update, s);
			break;

		case DATA_PDU_TYPE_CONTROL:
			rdp_recv_server_control_pdu(rdp, s);
			break;

		case DATA_PDU_TYPE_POINTER:
			update_recv_pointer(rdp->update, s);
			break;

		case DATA_PDU_TYPE_INPUT:
			break;

		case DATA_PDU_TYPE_SYNCHRONIZE:
			rdp_recv_synchronize_pdu(rdp, s);
			break;

		case DATA_PDU_TYPE_REFRESH_RECT:
			break;

		case DATA_PDU_TYPE_PLAY_SOUND:
			update_recv_play_sound(rdp->update, s);
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
			rdp_recv_font_map_pdu(rdp, s);
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
			rdp_recv_set_error_info_data_pdu(rdp, s);
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

boolean rdp_recv_out_of_sequence_pdu(rdpRdp* rdp, STREAM* s)
{
	uint16 type;
	uint16 length;
	uint16 channelId;

	rdp_read_share_control_header(s, &length, &type, &channelId);

	if (type == PDU_TYPE_DATA)
	{
		rdp_recv_data_pdu(rdp, s);
		return true;
	}
	else if (type == PDU_TYPE_SERVER_REDIRECTION)
	{
		rdp_recv_enhanced_security_redirection_packet(rdp, s);
		return true;
	}
	else
	{
		return false;
	}
}

/**
 * Decrypt an RDP packet.\n
 * @param rdp RDP module
 * @param s stream
 * @param length int
 */

boolean rdp_decrypt(rdpRdp* rdp, STREAM* s, int length, uint16 securityFlags)
{
	uint8 cmac[8], wmac[8];

	if (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS)
	{
		uint16 len;
		uint8 version, pad;
		uint8 *sig;

		stream_read_uint16(s, len); /* 0x10 */
		stream_read_uint8(s, version); /* 0x1 */
		stream_read_uint8(s, pad);

		sig = s->p;
		stream_seek(s, 8);	/* signature */

		length -= 12;

		if (!security_fips_decrypt(s->p, length, rdp))
		{
			printf("FATAL: cannot decrypt\n");
			return false; /* TODO */
		}

		if (!security_fips_check_signature(s->p, length - pad, sig, rdp))
		{
			printf("FATAL: invalid packet signature\n");
			return false; /* TODO */
		}

		/* is this what needs adjusting? */
		s->size -= pad;
		return true;
	}

	stream_read(s, wmac, sizeof(wmac));
	length -= sizeof(wmac);
	security_decrypt(s->p, length, rdp);
	if (securityFlags & SEC_SECURE_CHECKSUM)
		security_salted_mac_signature(rdp, s->p, length, false, cmac);
	else
		security_mac_signature(rdp, s->p, length, cmac);
	if (memcmp(wmac, cmac, sizeof(wmac)) != 0)
	{
		printf("WARNING: invalid packet signature\n");
		/*
		 * Because Standard RDP Security is totally broken,
		 * and cannot protect against MITM, don't treat signature
		 * verification failure as critical. This at least enables
		 * us to work with broken RDP clients and servers that
		 * generate invalid signatures.
		 */
		//return false;
	}
	return true;
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
	uint16 pduSource;
	uint16 channelId;
	uint16 securityFlags;

	if (!rdp_read_header(rdp, s, &length, &channelId))
	{
		printf("Incorrect RDP header.\n");
		return false;
	}

	if (rdp->settings->encryption)
	{
		rdp_read_security_header(s, &securityFlags);
		if (securityFlags & (SEC_ENCRYPT|SEC_REDIRECTION_PKT))
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				printf("rdp_decrypt failed\n");
				return false;
			}
		}
		if (securityFlags & SEC_REDIRECTION_PKT)
		{
			/*
			 * [MS-RDPBCGR] 2.2.13.2.1
			 *  - no share control header, nor the 2 byte pad
			 */
			s->p -= 2;
			rdp_recv_enhanced_security_redirection_packet(rdp, s);
			return true;
		}
	}

	if (channelId != MCS_GLOBAL_CHANNEL_ID)
	{
		freerdp_channel_process(rdp->instance, s, channelId);
	}
	else
	{
		rdp_read_share_control_header(s, &pduLength, &pduType, &pduSource);

		rdp->settings->pdu_source = pduSource;

		switch (pduType)
		{
			case PDU_TYPE_DATA:
				rdp_recv_data_pdu(rdp, s);
				break;

			case PDU_TYPE_DEACTIVATE_ALL:
				if (!rdp_recv_deactivate_all(rdp, s))
					return false;
				break;

			case PDU_TYPE_SERVER_REDIRECTION:
				rdp_recv_enhanced_security_redirection_packet(rdp, s);
				break;

			default:
				printf("incorrect PDU type: 0x%04X\n", pduType);
				break;
		}
	}

	return true;
}

static boolean rdp_recv_fastpath_pdu(rdpRdp* rdp, STREAM* s)
{
	uint16 length;
	rdpFastPath* fastpath;

	fastpath = rdp->fastpath;
	length = fastpath_read_header_rdp(fastpath, s);

	if (length == 0 || length > stream_get_left(s))
	{
		printf("incorrect FastPath PDU header length %d\n", length);
		return false;
	}

	if (fastpath->encryptionFlags & FASTPATH_OUTPUT_ENCRYPTED)
	{
		rdp_decrypt(rdp, s, length, (fastpath->encryptionFlags & FASTPATH_OUTPUT_SECURE_CHECKSUM) ? SEC_SECURE_CHECKSUM : 0);
	}

	return fastpath_recv_updates(rdp->fastpath, s);
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

static boolean rdp_recv_callback(rdpTransport* transport, STREAM* s, void* extra)
{
	rdpRdp* rdp = (rdpRdp*) extra;

	switch (rdp->state)
	{
		case CONNECTION_STATE_NEGO:
			if (!rdp_client_connect_mcs_connect_response(rdp, s))
				return false;
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			if (!rdp_client_connect_mcs_attach_user_confirm(rdp, s))
				return false;
			break;

		case CONNECTION_STATE_MCS_CHANNEL_JOIN:
			if (!rdp_client_connect_mcs_channel_join_confirm(rdp, s))
				return false;
			break;

		case CONNECTION_STATE_LICENSE:
			if (!rdp_client_connect_license(rdp, s))
				return false;
			break;

		case CONNECTION_STATE_CAPABILITY:
			if (!rdp_client_connect_demand_active(rdp, s))
			{
				printf("rdp_client_connect_demand_active failed\n");
				return false;
			}
			break;

		case CONNECTION_STATE_FINALIZATION:
			if (!rdp_recv_pdu(rdp, s))
				return false;
			if (rdp->finalize_sc_pdus == FINALIZE_SC_COMPLETE)
				rdp->state = CONNECTION_STATE_ACTIVE;
			break;

		case CONNECTION_STATE_ACTIVE:
			if (!rdp_recv_pdu(rdp, s))
				return false;
			break;

		default:
			printf("Invalid state %d\n", rdp->state);
			return false;
	}

	return true;
}

int rdp_send_channel_data(rdpRdp* rdp, int channel_id, uint8* data, int size)
{
	return freerdp_channel_send(rdp, channel_id, data, size);
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
		rdp->instance = instance;
		rdp->settings = settings_new((void*) instance);
		if (instance != NULL)
			instance->settings = rdp->settings;
		rdp->extension = extension_new(instance);
		rdp->transport = transport_new(rdp->settings);
		rdp->license = license_new(rdp);
		rdp->input = input_new(rdp);
		rdp->update = update_new(rdp);
		rdp->fastpath = fastpath_new(rdp);
		rdp->nego = nego_new(rdp->transport);
		rdp->mcs = mcs_new(rdp->transport);
		rdp->redirection = redirection_new();
		rdp->mppc = mppc_new(rdp);
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
		extension_free(rdp->extension);
		settings_free(rdp->settings);
		transport_free(rdp->transport);
		license_free(rdp->license);
		input_free(rdp->input);
		update_free(rdp->update);
		fastpath_free(rdp->fastpath);
		nego_free(rdp->nego);
		mcs_free(rdp->mcs);
		redirection_free(rdp->redirection);
		mppc_free(rdp);
		xfree(rdp);
	}
}

