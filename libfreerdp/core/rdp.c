/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include "rdp.h"

#include "info.h"
#include "redirection.h"

#include <freerdp/crypto/per.h>

#ifdef WITH_DEBUG_RDP
const char* DATA_PDU_TYPE_STRINGS[80] =
{
		"?", "?", /* 0x00 - 0x01 */
		"Update", /* 0x02 */
		"?", "?", "?", "?", "?", "?", "?", "?", /* 0x03 - 0x0A */
		"?", "?", "?", "?", "?", "?", "?", "?", "?", /* 0x0B - 0x13 */
		"Control", /* 0x14 */
		"?", "?", "?", "?", "?", "?", /* 0x15 - 0x1A */
		"Pointer", /* 0x1B */
		"Input", /* 0x1C */
		"?", "?", /* 0x1D - 0x1E */
		"Synchronize", /* 0x1F */
		"?", /* 0x20 */
		"Refresh Rect", /* 0x21 */
		"Play Sound", /* 0x22 */
		"Suppress Output", /* 0x23 */
		"Shutdown Request", /* 0x24 */
		"Shutdown Denied", /* 0x25 */
		"Save Session Info", /* 0x26 */
		"Font List", /* 0x27 */
		"Font Map", /* 0x28 */
		"Set Keyboard Indicators", /* 0x29 */
		"?", /* 0x2A */
		"Bitmap Cache Persistent List", /* 0x2B */
		"Bitmap Cache Error", /* 0x2C */
		"Set Keyboard IME Status", /* 0x2D */
		"Offscreen Cache Error", /* 0x2E */
		"Set Error Info", /* 0x2F */
		"Draw Nine Grid Error", /* 0x30 */
		"Draw GDI+ Error", /* 0x31 */
		"ARC Status", /* 0x32 */
		"?", "?", "?", /* 0x33 - 0x35 */
		"Status Info", /* 0x36 */
		"Monitor Layout" /* 0x37 */
		"FrameAcknowledge", "?", "?", /* 0x38 - 0x40 */
		"?", "?", "?", "?", "?", "?" /* 0x41 - 0x46 */
};
#endif

/**
 * Read RDP Security Header.\n
 * @msdn{cc240579}
 * @param s stream
 * @param flags security flags
 */

BOOL rdp_read_security_header(wStream* s, UINT16* flags)
{
	/* Basic Security Header */
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT16(s, *flags); /* flags */
	Stream_Seek(s, 2); /* flagsHi (unused) */
	return TRUE;
}

/**
 * Write RDP Security Header.\n
 * @msdn{cc240579}
 * @param s stream
 * @param flags security flags
 */

void rdp_write_security_header(wStream* s, UINT16 flags)
{
	/* Basic Security Header */
	Stream_Write_UINT16(s, flags); /* flags */
	Stream_Write_UINT16(s, 0); /* flagsHi (unused) */
}

BOOL rdp_read_share_control_header(wStream* s, UINT16* length, UINT16* type, UINT16* channel_id)
{
	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	/* Share Control Header */
	Stream_Read_UINT16(s, *length); /* totalLength */

	if (((size_t) *length - 2) > Stream_GetRemainingLength(s))
		return FALSE;

	Stream_Read_UINT16(s, *type); /* pduType */
	*type &= 0x0F; /* type is in the 4 least significant bits */

	if (*length > 4)
		Stream_Read_UINT16(s, *channel_id); /* pduSource */
	else
		*channel_id = 0; /* Windows XP can send such short DEACTIVATE_ALL PDUs. */

	return TRUE;
}

void rdp_write_share_control_header(wStream* s, UINT16 length, UINT16 type, UINT16 channel_id)
{
	length -= RDP_PACKET_HEADER_MAX_LENGTH;

	/* Share Control Header */
	Stream_Write_UINT16(s, length); /* totalLength */
	Stream_Write_UINT16(s, type | 0x10); /* pduType */
	Stream_Write_UINT16(s, channel_id); /* pduSource */
}

BOOL rdp_read_share_data_header(wStream* s, UINT16* length, BYTE* type, UINT32* shareId,
					BYTE* compressedType, UINT16* compressedLength)
{
	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;

	/* Share Data Header */
	Stream_Read_UINT32(s, *shareId); /* shareId (4 bytes) */
	Stream_Seek_UINT8(s); /* pad1 (1 byte) */
	Stream_Seek_UINT8(s); /* streamId (1 byte) */
	Stream_Read_UINT16(s, *length); /* uncompressedLength (2 bytes) */
	Stream_Read_UINT8(s, *type); /* pduType2, Data PDU Type (1 byte) */
	Stream_Read_UINT8(s, *compressedType); /* compressedType (1 byte) */
	Stream_Read_UINT16(s, *compressedLength); /* compressedLength (2 bytes) */

	return TRUE;
}

void rdp_write_share_data_header(wStream* s, UINT16 length, BYTE type, UINT32 share_id)
{
	length -= RDP_PACKET_HEADER_MAX_LENGTH;
	length -= RDP_SHARE_CONTROL_HEADER_LENGTH;
	length -= RDP_SHARE_DATA_HEADER_LENGTH;

	/* Share Data Header */
	Stream_Write_UINT32(s, share_id); /* shareId (4 bytes) */
	Stream_Write_UINT8(s, 0); /* pad1 (1 byte) */
	Stream_Write_UINT8(s, STREAM_LOW); /* streamId (1 byte) */
	Stream_Write_UINT16(s, length); /* uncompressedLength (2 bytes) */
	Stream_Write_UINT8(s, type); /* pduType2, Data PDU Type (1 byte) */
	Stream_Write_UINT8(s, 0); /* compressedType (1 byte) */
	Stream_Write_UINT16(s, 0); /* compressedLength (2 bytes) */
}

static int rdp_security_stream_init(rdpRdp* rdp, wStream* s)
{
	if (rdp->do_crypt)
	{
		Stream_Seek(s, 12);

		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			Stream_Seek(s, 4);

		rdp->sec_flags |= SEC_ENCRYPT;

		if (rdp->do_secure_checksum)
			rdp->sec_flags |= SEC_SECURE_CHECKSUM;
	}
	else if (rdp->sec_flags != 0)
	{
		Stream_Seek(s, 4);
	}

	return 0;
}

int rdp_init_stream(rdpRdp* rdp, wStream* s)
{
	Stream_Seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
	rdp_security_stream_init(rdp, s);
	return 0;
}

wStream* rdp_send_stream_init(rdpRdp* rdp)
{
	wStream* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	rdp_init_stream(rdp, s);
	return s;
}

int rdp_init_stream_pdu(rdpRdp* rdp, wStream* s)
{
	Stream_Seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
	rdp_security_stream_init(rdp, s);
	Stream_Seek(s, RDP_SHARE_CONTROL_HEADER_LENGTH);
	return 0;
}

int rdp_init_stream_data_pdu(rdpRdp* rdp, wStream* s)
{
	Stream_Seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
	rdp_security_stream_init(rdp, s);
	Stream_Seek(s, RDP_SHARE_CONTROL_HEADER_LENGTH);
	Stream_Seek(s, RDP_SHARE_DATA_HEADER_LENGTH);
	return 0;
}

wStream* rdp_data_pdu_init(rdpRdp* rdp)
{
	wStream* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	rdp_init_stream_data_pdu(rdp, s);
	return s;
}

BOOL rdp_set_error_info(rdpRdp* rdp, UINT32 errorInfo)
{
	rdp->errorInfo = errorInfo;

	if (rdp->errorInfo != ERRINFO_SUCCESS)
	{
		ErrorInfoEventArgs e;
		rdpContext* context = rdp->instance->context;

		rdp->context->LastError = MAKE_FREERDP_ERROR(ERRINFO, errorInfo);
		rdp_print_errinfo(rdp->errorInfo);

		EventArgsInit(&e, "freerdp");
		e.code = rdp->errorInfo;
		PubSub_OnErrorInfo(context->pubSub, context, &e);
	}
	else
	{
		rdp->context->LastError = FREERDP_ERROR_SUCCESS;
	}

	return TRUE;
}

wStream* rdp_message_channel_pdu_init(rdpRdp* rdp)
{
	wStream* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	Stream_Seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
	rdp_security_stream_init(rdp, s);
	return s;
}

/**
 * Read an RDP packet header.\n
 * @param rdp rdp module
 * @param s stream
 * @param length RDP packet length
 * @param channel_id channel id
 */

BOOL rdp_read_header(rdpRdp* rdp, wStream* s, UINT16* length, UINT16* channelId)
{
	BYTE byte;
	UINT16 initiator;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = (rdp->settings->ServerMode) ? DomainMCSPDU_SendDataRequest : DomainMCSPDU_SendDataIndication;

	if (!mcs_read_domain_mcspdu_header(s, &MCSPDU, length))
	{
		if (MCSPDU != DomainMCSPDU_DisconnectProviderUltimatum)
			return FALSE;
	}

	if ((size_t) (*length - 8) > Stream_GetRemainingLength(s))
		return FALSE;

	if (MCSPDU == DomainMCSPDU_DisconnectProviderUltimatum)
	{
		int reason = 0;
		TerminateEventArgs e;
		rdpContext* context;

		if (!mcs_recv_disconnect_provider_ultimatum(rdp->mcs, s, &reason))
			return FALSE;

		if (rdp->instance == NULL)
		{
			rdp->disconnect = TRUE;
			return FALSE;
		}

		context = rdp->instance->context;

		if (rdp->errorInfo == ERRINFO_SUCCESS)
		{
			/**
			 * Some servers like Windows Server 2008 R2 do not send the error info pdu
			 * when the user logs off like they should. Map DisconnectProviderUltimatum
			 * to a ERRINFO_LOGOFF_BY_USER when the errinfo code is ERRINFO_SUCCESS.
			 */

			if (reason == MCS_Reason_provider_initiated)
				rdp_set_error_info(rdp, ERRINFO_RPC_INITIATED_DISCONNECT);
			else if (reason == MCS_Reason_user_requested)
				rdp_set_error_info(rdp, ERRINFO_LOGOFF_BY_USER);
			else
				rdp_set_error_info(rdp, ERRINFO_RPC_INITIATED_DISCONNECT);
		}

		fprintf(stderr, "DisconnectProviderUltimatum: reason: %d\n", reason);

		rdp->disconnect = TRUE;

		EventArgsInit(&e, "freerdp");
		e.code = 0;
		PubSub_OnTerminate(context->pubSub, context, &e);

		return TRUE;
	}

	if (Stream_GetRemainingLength(s) < 5)
		return FALSE;

	per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_read_integer16(s, channelId, 0); /* channelId */
	Stream_Read_UINT8(s, byte); /* dataPriority + Segmentation (0x70) */

	if (!per_read_length(s, length)) /* userData (OCTET_STRING) */
		return FALSE;

	if (*length > Stream_GetRemainingLength(s))
		return FALSE;

	return TRUE;
}

/**
 * Write an RDP packet header.\n
 * @param rdp rdp module
 * @param s stream
 * @param length RDP packet length
 * @param channel_id channel id
 */

void rdp_write_header(rdpRdp* rdp, wStream* s, UINT16 length, UINT16 channelId)
{
	int body_length;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = (rdp->settings->ServerMode) ? DomainMCSPDU_SendDataIndication : DomainMCSPDU_SendDataRequest;

	if ((rdp->sec_flags & SEC_ENCRYPT) && (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS))
	{
		int pad;

		body_length = length - RDP_PACKET_HEADER_MAX_LENGTH - 16;
		pad = 8 - (body_length % 8);

		if (pad != 8)
			length += pad;
	}

	mcs_write_domain_mcspdu_header(s, MCSPDU, length, 0);
	per_write_integer16(s, rdp->mcs->userId, MCS_BASE_CHANNEL_ID); /* initiator */
	per_write_integer16(s, channelId, 0); /* channelId */
	Stream_Write_UINT8(s, 0x70); /* dataPriority + segmentation */
	/*
	 * We always encode length in two bytes, even though we could use
	 * only one byte if length <= 0x7F. It is just easier that way,
	 * because we can leave room for fixed-length header, store all
	 * the data first and then store the header.
	 */
	length = (length - RDP_PACKET_HEADER_MAX_LENGTH) | 0x8000;
	Stream_Write_UINT16_BE(s, length); /* userData (OCTET_STRING) */
}

static UINT32 rdp_security_stream_out(rdpRdp* rdp, wStream* s, int length, UINT32 sec_flags)
{
	BYTE* data;
	UINT32 pad = 0;

	sec_flags |= rdp->sec_flags;

	if (sec_flags != 0)
	{
		rdp_write_security_header(s, sec_flags);

		if (sec_flags & SEC_ENCRYPT)
		{
			if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			{
				data = Stream_Pointer(s) + 12;

				length = length - (data - Stream_Buffer(s));
				Stream_Write_UINT16(s, 0x10); /* length */
				Stream_Write_UINT8(s, 0x1); /* TSFIPS_VERSION 1*/

				/* handle padding */
				pad = 8 - (length % 8);

				if (pad == 8)
					pad = 0;
				if (pad)
					memset(data+length, 0, pad);

				Stream_Write_UINT8(s, pad);

				security_hmac_signature(data, length, Stream_Pointer(s), rdp);
				Stream_Seek(s, 8);
				security_fips_encrypt(data, length + pad, rdp);
			}
			else
			{
				data = Stream_Pointer(s) + 8;
				length = length - (data - Stream_Buffer(s));

				if (sec_flags & SEC_SECURE_CHECKSUM)
					security_salted_mac_signature(rdp, data, length, TRUE, Stream_Pointer(s));
				else
					security_mac_signature(rdp, data, length, Stream_Pointer(s));

				Stream_Seek(s, 8);
				security_encrypt(Stream_Pointer(s), length, rdp);
			}
		}

		rdp->sec_flags = 0;
	}

	return pad;
}

static UINT32 rdp_get_sec_bytes(rdpRdp* rdp)
{
	UINT32 sec_bytes;

	if (rdp->sec_flags & SEC_ENCRYPT)
	{
		sec_bytes = 12;

		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
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

BOOL rdp_send(rdpRdp* rdp, wStream* s, UINT16 channel_id)
{
	UINT16 length;

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

	rdp_write_header(rdp, s, length, channel_id);

	length += rdp_security_stream_out(rdp, s, length, 0);

	Stream_SetPosition(s, length);
	Stream_SealLength(s);

	if (transport_write(rdp->transport, s) < 0)
		return FALSE;

	return TRUE;
}

BOOL rdp_send_pdu(rdpRdp* rdp, wStream* s, UINT16 type, UINT16 channel_id)
{
	UINT16 length;
	UINT32 sec_bytes;
	int sec_hold;

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);

	sec_bytes = rdp_get_sec_bytes(rdp);
	sec_hold = Stream_GetPosition(s);
	Stream_Seek(s, sec_bytes);

	rdp_write_share_control_header(s, length - sec_bytes, type, channel_id);

	Stream_SetPosition(s, sec_hold);
	length += rdp_security_stream_out(rdp, s, length, 0);

	Stream_SetPosition(s, length);
	Stream_SealLength(s);

	if (transport_write(rdp->transport, s) < 0)
		return FALSE;

	return TRUE;
}

BOOL rdp_send_data_pdu(rdpRdp* rdp, wStream* s, BYTE type, UINT16 channel_id)
{
	UINT16 length;
	UINT32 sec_bytes;
	int sec_hold;

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);

	sec_bytes = rdp_get_sec_bytes(rdp);
	sec_hold = Stream_GetPosition(s);
	Stream_Seek(s, sec_bytes);

	rdp_write_share_control_header(s, length - sec_bytes, PDU_TYPE_DATA, channel_id);
	rdp_write_share_data_header(s, length - sec_bytes, type, rdp->settings->ShareId);

	Stream_SetPosition(s, sec_hold);
	length += rdp_security_stream_out(rdp, s, length, 0);

	Stream_SetPosition(s, length);
	Stream_SealLength(s);

	if (transport_write(rdp->transport, s) < 0)
		return FALSE;

	return TRUE;
}

BOOL rdp_send_message_channel_pdu(rdpRdp* rdp, wStream* s, UINT16 sec_flags)
{
	UINT16 length;
	UINT32 sec_bytes;
	int sec_hold;

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

	rdp_write_header(rdp, s, length, rdp->mcs->messageChannelId);

	sec_bytes = rdp_get_sec_bytes(rdp);
	sec_hold = Stream_GetPosition(s);
	Stream_Seek(s, sec_bytes);

	Stream_SetPosition(s, sec_hold);
	length += rdp_security_stream_out(rdp, s, length, sec_flags);

	Stream_SetPosition(s, length);
	Stream_SealLength(s);

	if (transport_write(rdp->transport, s) < 0)
		return FALSE;

	return TRUE;
}

BOOL rdp_recv_server_shutdown_denied_pdu(rdpRdp* rdp, wStream* s)
{
	return TRUE;
}

BOOL rdp_recv_server_set_keyboard_indicators_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 unitId;
	UINT16 ledFlags;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, unitId); /* unitId (2 bytes) */
	Stream_Read_UINT16(s, ledFlags); /* ledFlags (2 bytes) */

	return TRUE;
}

BOOL rdp_recv_server_set_keyboard_ime_status_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 unitId;
	UINT32 imeState;
	UINT32 imeConvMode;

	if (Stream_GetRemainingLength(s) < 10)
		return FALSE;

	Stream_Read_UINT16(s, unitId); /* unitId (2 bytes) */
	Stream_Read_UINT32(s, imeState); /* imeState (4 bytes) */
	Stream_Read_UINT32(s, imeConvMode); /* imeConvMode (4 bytes) */

	return TRUE;
}

BOOL rdp_recv_set_error_info_data_pdu(rdpRdp* rdp, wStream* s)
{
	UINT32 errorInfo;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, errorInfo); /* errorInfo (4 bytes) */

	rdp_set_error_info(rdp, errorInfo);

	return TRUE;
}

BOOL rdp_recv_server_auto_reconnect_status_pdu(rdpRdp* rdp, wStream* s)
{
	UINT32 arcStatus;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, arcStatus); /* arcStatus (4 bytes) */

	return TRUE;
}

BOOL rdp_recv_server_status_info_pdu(rdpRdp* rdp, wStream* s)
{
	UINT32 statusCode;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, statusCode); /* statusCode (4 bytes) */

	return TRUE;
}

BOOL rdp_recv_monitor_layout_pdu(rdpRdp* rdp, wStream* s)
{
	UINT32 index;
	UINT32 monitorCount;
	MONITOR_DEF* monitor;
	MONITOR_DEF* monitorDefArray;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, monitorCount); /* monitorCount (4 bytes) */

	if (Stream_GetRemainingLength(s) < (monitorCount * 20))
		return FALSE;

	monitorDefArray = (MONITOR_DEF*) malloc(sizeof(MONITOR_DEF) * monitorCount);
	ZeroMemory(monitorDefArray, sizeof(MONITOR_DEF) * monitorCount);

	for (index = 0; index < monitorCount; index++)
	{
		monitor = &(monitorDefArray[index]);

		Stream_Read_UINT32(s, monitor->left); /* left (4 bytes) */
		Stream_Read_UINT32(s, monitor->top); /* top (4 bytes) */
		Stream_Read_UINT32(s, monitor->right); /* right (4 bytes) */
		Stream_Read_UINT32(s, monitor->bottom); /* bottom (4 bytes) */
		Stream_Read_UINT32(s, monitor->flags); /* flags (4 bytes) */
	}

	free(monitorDefArray);

	return TRUE;
}

BOOL rdp_write_monitor_layout_pdu(wStream* s, UINT32 monitorCount, MONITOR_DEF* monitorDefArray)
{
	UINT32 index;
	MONITOR_DEF* monitor;

	Stream_EnsureRemainingCapacity(s, 4 + (monitorCount * 20));

	Stream_Write_UINT32(s, monitorCount); /* monitorCount (4 bytes) */

	for (index = 0; index < monitorCount; index++)
	{
		monitor = &(monitorDefArray[index]);

		Stream_Write_UINT32(s, monitor->left); /* left (4 bytes) */
		Stream_Write_UINT32(s, monitor->top); /* top (4 bytes) */
		Stream_Write_UINT32(s, monitor->right); /* right (4 bytes) */
		Stream_Write_UINT32(s, monitor->bottom); /* bottom (4 bytes) */
		Stream_Write_UINT32(s, monitor->flags); /* flags (4 bytes) */
	}

	return TRUE;
}

int rdp_recv_data_pdu(rdpRdp* rdp, wStream* s)
{
	BYTE type;
	wStream* cs;
	UINT16 length;
	UINT32 shareId;
	BYTE compressedType;
	UINT16 compressedLength;

	if (!rdp_read_share_data_header(s, &length, &type, &shareId, &compressedType, &compressedLength))
		return -1;

	cs = s;

	if (compressedType & PACKET_COMPRESSED)
	{
		UINT32 DstSize = 0;
		BYTE* pDstData = NULL;
		UINT32 SrcSize = compressedLength - 18;

		if (Stream_GetRemainingLength(s) < (size_t) SrcSize)
		{
			fprintf(stderr, "bulk_decompress: not enough bytes for compressedLength %d\n", compressedLength);
			return -1;
		}

		if (bulk_decompress(rdp->bulk, Stream_Pointer(s), SrcSize, &pDstData, &DstSize, compressedType))
		{
			cs = StreamPool_Take(rdp->transport->ReceivePool, DstSize);

			Stream_SetPosition(cs, 0);
			Stream_Write(cs, pDstData, DstSize);
			Stream_SealLength(cs);
			Stream_SetPosition(cs, 0);
		}
		else
		{
			fprintf(stderr, "bulk_decompress() failed\n");
			return -1;
		}

		Stream_Seek(s, SrcSize);
	}

#ifdef WITH_DEBUG_RDP
	printf("recv %s Data PDU (0x%02X), length: %d\n",
				type < ARRAYSIZE(DATA_PDU_TYPE_STRINGS) ? DATA_PDU_TYPE_STRINGS[type] : "???", type, length);
#endif

	switch (type)
	{
		case DATA_PDU_TYPE_UPDATE:
			if (!update_recv(rdp->update, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_CONTROL:
			if (!rdp_recv_server_control_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_POINTER:
			if (!update_recv_pointer(rdp->update, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_SYNCHRONIZE:
			if (!rdp_recv_synchronize_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_PLAY_SOUND:
			if (!update_recv_play_sound(rdp->update, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_SHUTDOWN_DENIED:
			if (!rdp_recv_server_shutdown_denied_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_SAVE_SESSION_INFO:
			if (!rdp_recv_save_session_info(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_FONT_MAP:
			if (!rdp_recv_font_map_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_SET_KEYBOARD_INDICATORS:
			if (!rdp_recv_server_set_keyboard_indicators_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_SET_KEYBOARD_IME_STATUS:
			if (!rdp_recv_server_set_keyboard_ime_status_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_SET_ERROR_INFO:
			if (!rdp_recv_set_error_info_data_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_ARC_STATUS:
			if (!rdp_recv_server_auto_reconnect_status_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_STATUS_INFO:
			if (!rdp_recv_server_status_info_pdu(rdp, cs))
				return -1;
			break;

		case DATA_PDU_TYPE_MONITOR_LAYOUT:
			if (!rdp_recv_monitor_layout_pdu(rdp, cs))
				return -1;
			break;

		default:
			break;
	}

	if (cs != s)
		Stream_Release(cs);

	return 0;
}

int rdp_recv_message_channel_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 securityFlags;

	if (!rdp_read_security_header(s, &securityFlags))
		return -1;

	if (securityFlags & SEC_AUTODETECT_REQ)
	{
		/* Server Auto-Detect Request PDU */
		return rdp_recv_autodetect_packet(rdp, s);
	}

	if (securityFlags & SEC_HEARTBEAT)
	{
		/* Heartbeat PDU */
		return rdp_recv_heartbeat_packet(rdp, s);
	}

	if (securityFlags & SEC_TRANSPORT_REQ)
	{
		/* Initiate Multitransport Request PDU */
		return rdp_recv_multitransport_packet(rdp, s);
	}

	return -1;
}

int rdp_recv_out_of_sequence_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 type;
	UINT16 length;
	UINT16 channelId;

	if (!rdp_read_share_control_header(s, &length, &type, &channelId))
		return -1;

	if (type == PDU_TYPE_DATA)
	{
		return rdp_recv_data_pdu(rdp, s);
	}
	else if (type == PDU_TYPE_SERVER_REDIRECTION)
	{
		return rdp_recv_enhanced_security_redirection_packet(rdp, s);
	}
	else
	{
		return -1;
	}
}

/**
 * Decrypt an RDP packet.\n
 * @param rdp RDP module
 * @param s stream
 * @param length int
 */

BOOL rdp_decrypt(rdpRdp* rdp, wStream* s, int length, UINT16 securityFlags)
{
	BYTE cmac[8];
	BYTE wmac[8];

	if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
	{
		UINT16 len;
		BYTE version, pad;
		BYTE* sig;

		if (Stream_GetRemainingLength(s) < 12)
			return FALSE;

		Stream_Read_UINT16(s, len); /* 0x10 */
		Stream_Read_UINT8(s, version); /* 0x1 */
		Stream_Read_UINT8(s, pad);

		sig = Stream_Pointer(s);
		Stream_Seek(s, 8);	/* signature */

		length -= 12;

		if (!security_fips_decrypt(Stream_Pointer(s), length, rdp))
		{
			fprintf(stderr, "FATAL: cannot decrypt\n");
			return FALSE; /* TODO */
		}

		if (!security_fips_check_signature(Stream_Pointer(s), length - pad, sig, rdp))
		{
			fprintf(stderr, "FATAL: invalid packet signature\n");
			return FALSE; /* TODO */
		}

		Stream_Length(s) -= pad;
		return TRUE;
	}

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read(s, wmac, sizeof(wmac));
	length -= sizeof(wmac);

	if (!security_decrypt(Stream_Pointer(s), length, rdp))
		return FALSE;

	if (securityFlags & SEC_SECURE_CHECKSUM)
		security_salted_mac_signature(rdp, Stream_Pointer(s), length, FALSE, cmac);
	else
		security_mac_signature(rdp, Stream_Pointer(s), length, cmac);

	if (memcmp(wmac, cmac, sizeof(wmac)) != 0)
	{
		fprintf(stderr, "WARNING: invalid packet signature\n");
		/*
		 * Because Standard RDP Security is totally broken,
		 * and cannot protect against MITM, don't treat signature
		 * verification failure as critical. This at least enables
		 * us to work with broken RDP clients and servers that
		 * generate invalid signatures.
		 */
		//return FALSE;
	}

	return TRUE;
}

/**
 * Process an RDP packet.\n
 * @param rdp RDP module
 * @param s stream
 */

static int rdp_recv_tpkt_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 length;
	UINT16 pduType;
	UINT16 pduLength;
	UINT16 pduSource;
	UINT16 channelId = 0;
	UINT16 securityFlags;
	int nextPosition;

	if (!rdp_read_header(rdp, s, &length, &channelId))
	{
		fprintf(stderr, "Incorrect RDP header.\n");
		return -1;
	}

	if (rdp->settings->DisableEncryption)
	{
		if (!rdp_read_security_header(s, &securityFlags))
			return -1;

		if (securityFlags & (SEC_ENCRYPT | SEC_REDIRECTION_PKT))
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				fprintf(stderr, "rdp_decrypt failed\n");
				return -1;
			}
		}

		if (securityFlags & SEC_REDIRECTION_PKT)
		{
			/*
			 * [MS-RDPBCGR] 2.2.13.2.1
			 *  - no share control header, nor the 2 byte pad
			 */
			Stream_Rewind(s, 2);

			return rdp_recv_enhanced_security_redirection_packet(rdp, s);
		}
	}

	if (channelId == MCS_GLOBAL_CHANNEL_ID)
	{
		while (Stream_GetRemainingLength(s) > 3)
		{
			nextPosition = Stream_GetPosition(s);

			if (!rdp_read_share_control_header(s, &pduLength, &pduType, &pduSource))
				return -1;

			nextPosition += pduLength;

			rdp->settings->PduSource = pduSource;

			switch (pduType)
			{
				case PDU_TYPE_DATA:
					if (rdp_recv_data_pdu(rdp, s) < 0)
					{
						fprintf(stderr, "rdp_recv_data_pdu failed\n");
						return -1;
					}
					break;

				case PDU_TYPE_DEACTIVATE_ALL:
					if (!rdp_recv_deactivate_all(rdp, s))
						return -1;
					break;

				case PDU_TYPE_SERVER_REDIRECTION:
					return rdp_recv_enhanced_security_redirection_packet(rdp, s);
					break;

				default:
					fprintf(stderr, "incorrect PDU type: 0x%04X\n", pduType);
					break;
			}

			Stream_SetPosition(s, nextPosition);
		}
	}
	else if (rdp->mcs->messageChannelId && channelId == rdp->mcs->messageChannelId)
	{
		return rdp_recv_message_channel_pdu(rdp, s);
	}
	else
	{
		if (!freerdp_channel_process(rdp->instance, s, channelId))
			return -1;
	}

	return 0;
}

static int rdp_recv_fastpath_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 length;
	rdpFastPath* fastpath;

	fastpath = rdp->fastpath;

	if (!fastpath_read_header_rdp(fastpath, s, &length))
		return -1;

	if ((length == 0) || (length > Stream_GetRemainingLength(s)))
	{
		fprintf(stderr, "incorrect FastPath PDU header length %d\n", length);
		return -1;
	}

	if (fastpath->encryptionFlags & FASTPATH_OUTPUT_ENCRYPTED)
	{
		UINT16 flags = (fastpath->encryptionFlags & FASTPATH_OUTPUT_SECURE_CHECKSUM) ? SEC_SECURE_CHECKSUM : 0;

		if (!rdp_decrypt(rdp, s, length, flags))
			return -1;
	}

	return fastpath_recv_updates(rdp->fastpath, s);
}

static int rdp_recv_pdu(rdpRdp* rdp, wStream* s)
{
	if (tpkt_verify_header(s))
		return rdp_recv_tpkt_pdu(rdp, s);
	else
		return rdp_recv_fastpath_pdu(rdp, s);
}

static int rdp_recv_callback(rdpTransport* transport, wStream* s, void* extra)
{
	int status = 0;
	rdpRdp* rdp = (rdpRdp*) extra;

	/* 
	 * At any point in the connection sequence between when all
	 * MCS channels have been joined and when the RDP connection
	 * enters the active state, an auto-detect PDU can be received
	 * on the MCS message channel.
	 */
	if ((rdp->state > CONNECTION_STATE_MCS_CHANNEL_JOIN) &&
		(rdp->state < CONNECTION_STATE_ACTIVE))
	{
		if (rdp_client_connect_auto_detect(rdp, s))
			return 0;
	}

	switch (rdp->state)
	{
		case CONNECTION_STATE_NEGO:
			if (!rdp_client_connect_mcs_connect_response(rdp, s))
				status = -1;
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			if (!rdp_client_connect_mcs_attach_user_confirm(rdp, s))
				status = -1;
			break;

		case CONNECTION_STATE_MCS_CHANNEL_JOIN:
			if (!rdp_client_connect_mcs_channel_join_confirm(rdp, s))
				status = -1;
			break;

		case CONNECTION_STATE_LICENSING:
			status = rdp_client_connect_license(rdp, s);
			break;

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE:
			status = rdp_client_connect_demand_active(rdp, s);
			break;

		case CONNECTION_STATE_FINALIZATION:
			status = rdp_recv_pdu(rdp, s);

			if ((status >= 0) && (rdp->finalize_sc_pdus == FINALIZE_SC_COMPLETE))
				rdp_client_transition_to_state(rdp, CONNECTION_STATE_ACTIVE);
			break;

		case CONNECTION_STATE_ACTIVE:
			status = rdp_recv_pdu(rdp, s);
			break;

		default:
			fprintf(stderr, "Invalid state %d\n", rdp->state);
			status = -1;
			break;
	}

	return status;
}

int rdp_send_channel_data(rdpRdp* rdp, UINT16 channelId, BYTE* data, int size)
{
	return freerdp_channel_send(rdp, channelId, data, size);
}

/**
 * Set non-blocking mode information.
 * @param rdp RDP module
 * @param blocking blocking mode
 */
void rdp_set_blocking_mode(rdpRdp* rdp, BOOL blocking)
{
	rdp->transport->ReceiveCallback = rdp_recv_callback;
	rdp->transport->ReceiveExtra = rdp;
	transport_set_blocking_mode(rdp->transport, blocking);
}

int rdp_check_fds(rdpRdp* rdp)
{
	int status;

	status = transport_check_fds(rdp->transport);

	if (status == 1)
	{
		status = rdp_client_redirect(rdp); /* session redirection */
	}

	return status;
}

/**
 * Instantiate new RDP module.
 * @return new RDP module
 */

rdpRdp* rdp_new(rdpContext* context)
{
	rdpRdp* rdp;
	DWORD flags;
	BOOL newSettings = FALSE;

	rdp = (rdpRdp*) calloc(1, sizeof(rdpRdp));
	if (!rdp)
		return NULL;

	rdp->context = context;
	rdp->instance = context->instance;

	flags = 0;

	if (context->ServerMode)
		flags |= FREERDP_SETTINGS_SERVER_MODE;

	if (!context->settings)
	{
		context->settings = freerdp_settings_new(flags);
		if (!context->settings)
			goto out_free;
		newSettings = TRUE;
	}

	rdp->settings = context->settings;
	rdp->settings->instance = context->instance;
	
	if (context->instance)
		context->instance->settings = rdp->settings;

	rdp->transport = transport_new(rdp->settings);
	if (!rdp->transport)
		goto out_free_settings;
	
	rdp->transport->rdp = rdp;

	rdp->license = license_new(rdp);
	if (!rdp->license)
		goto out_free_transport;

	rdp->input = input_new(rdp);
	if (!rdp->input)
		goto out_free_license;

	rdp->update = update_new(rdp);
	if (!rdp->update)
		goto out_free_input;

	rdp->fastpath = fastpath_new(rdp);
	if (!rdp->fastpath)
		goto out_free_update;

	rdp->nego = nego_new(rdp->transport);
	if (!rdp->nego)
		goto out_free_fastpath;

	rdp->mcs = mcs_new(rdp->transport);
	if (!rdp->mcs)
		goto out_free_nego;

	rdp->redirection = redirection_new();
	if (!rdp->redirection)
		goto out_free_mcs;

	rdp->autodetect = autodetect_new();
	if (!rdp->autodetect)
		goto out_free_redirection;

	rdp->heartbeat = heartbeat_new();
	if (!rdp->heartbeat)
		goto out_free_autodetect;

	rdp->multitransport = multitransport_new();
	if (!rdp->multitransport)
		goto out_free_heartbeat;

	rdp->bulk = bulk_new(context);
	if (!rdp->bulk)
		goto out_free_multitransport;

	return rdp;

out_free_multitransport:
	multitransport_free(rdp->multitransport);
out_free_heartbeat:
	heartbeat_free(rdp->heartbeat);
out_free_autodetect:
	autodetect_free(rdp->autodetect);
out_free_redirection:
	redirection_free(rdp->redirection);
out_free_mcs:
	mcs_free(rdp->mcs);
out_free_nego:
	nego_free(rdp->nego);
out_free_fastpath:
	fastpath_free(rdp->fastpath);
out_free_update:
	update_free(rdp->update);
out_free_input:
	input_free(rdp->input);
out_free_license:
	license_free(rdp->license);
out_free_transport:
	transport_free(rdp->transport);
out_free_settings:
	if (newSettings)
		freerdp_settings_free(rdp->settings);
out_free:
	free(rdp);
	return NULL;
}

void rdp_reset(rdpRdp* rdp)
{
	rdpSettings* settings;

	settings = rdp->settings;

	bulk_reset(rdp->bulk);

	crypto_rc4_free(rdp->rc4_decrypt_key);
	rdp->rc4_decrypt_key = NULL;
	crypto_rc4_free(rdp->rc4_encrypt_key);
	rdp->rc4_encrypt_key = NULL;
	crypto_des3_free(rdp->fips_encrypt);
	rdp->fips_encrypt = NULL;
	crypto_des3_free(rdp->fips_decrypt);
	rdp->fips_decrypt = NULL;
	crypto_hmac_free(rdp->fips_hmac);
	rdp->fips_hmac = NULL;

	mcs_free(rdp->mcs);
	nego_free(rdp->nego);
	license_free(rdp->license);
	transport_free(rdp->transport);

	free(settings->ServerRandom);
	settings->ServerRandom = NULL;
	free(settings->ServerCertificate);
	settings->ServerCertificate = NULL;
	free(settings->ClientAddress);
	settings->ClientAddress = NULL;

	rdp->transport = transport_new(rdp->settings);
	rdp->transport->rdp = rdp;
	rdp->license = license_new(rdp);
	rdp->nego = nego_new(rdp->transport);
	rdp->mcs = mcs_new(rdp->transport);
	rdp->transport->layer = TRANSPORT_LAYER_TCP;
}

/**
 * Free RDP module.
 * @param rdp RDP module to be freed
 */

void rdp_free(rdpRdp* rdp)
{
	if (rdp)
	{
		crypto_rc4_free(rdp->rc4_decrypt_key);
		crypto_rc4_free(rdp->rc4_encrypt_key);
		crypto_des3_free(rdp->fips_encrypt);
		crypto_des3_free(rdp->fips_decrypt);
		crypto_hmac_free(rdp->fips_hmac);
		freerdp_settings_free(rdp->settings);
		freerdp_settings_free(rdp->settingsCopy);
		transport_free(rdp->transport);
		license_free(rdp->license);
		input_free(rdp->input);
		update_free(rdp->update);
		fastpath_free(rdp->fastpath);
		nego_free(rdp->nego);
		mcs_free(rdp->mcs);
		redirection_free(rdp->redirection);
		autodetect_free(rdp->autodetect);
		heartbeat_free(rdp->heartbeat);
		multitransport_free(rdp->multitransport);
		bulk_free(rdp->bulk);
		free(rdp);
	}
}

