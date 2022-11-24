/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Core
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/assert.h>

#include "rdp.h"

#include "state.h"
#include "info.h"
#include "utils.h"
#include "mcs.h"
#include "redirection.h"

#include <freerdp/codec/bulk.h>
#include <freerdp/crypto/per.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("core.rdp")

static const char* DATA_PDU_TYPE_STRINGS[80] = {
	"?",
	"?",      /* 0x00 - 0x01 */
	"Update", /* 0x02 */
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?", /* 0x03 - 0x0A */
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",       /* 0x0B - 0x13 */
	"Control", /* 0x14 */
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",       /* 0x15 - 0x1A */
	"Pointer", /* 0x1B */
	"Input",   /* 0x1C */
	"?",
	"?",                            /* 0x1D - 0x1E */
	"Synchronize",                  /* 0x1F */
	"?",                            /* 0x20 */
	"Refresh Rect",                 /* 0x21 */
	"Play Sound",                   /* 0x22 */
	"Suppress Output",              /* 0x23 */
	"Shutdown Request",             /* 0x24 */
	"Shutdown Denied",              /* 0x25 */
	"Save Session Info",            /* 0x26 */
	"Font List",                    /* 0x27 */
	"Font Map",                     /* 0x28 */
	"Set Keyboard Indicators",      /* 0x29 */
	"?",                            /* 0x2A */
	"Bitmap Cache Persistent List", /* 0x2B */
	"Bitmap Cache Error",           /* 0x2C */
	"Set Keyboard IME Status",      /* 0x2D */
	"Offscreen Cache Error",        /* 0x2E */
	"Set Error Info",               /* 0x2F */
	"Draw Nine Grid Error",         /* 0x30 */
	"Draw GDI+ Error",              /* 0x31 */
	"ARC Status",                   /* 0x32 */
	"?",
	"?",
	"?",              /* 0x33 - 0x35 */
	"Status Info",    /* 0x36 */
	"Monitor Layout", /* 0x37 */
	"FrameAcknowledge",
	"?",
	"?", /* 0x38 - 0x40 */
	"?",
	"?",
	"?",
	"?",
	"?",
	"?" /* 0x41 - 0x46 */
};

const char* data_pdu_type_to_string(UINT8 type)
{
	if (type >= ARRAYSIZE(DATA_PDU_TYPE_STRINGS))
		return "???";
	return DATA_PDU_TYPE_STRINGS[type];
}

static BOOL rdp_read_flow_control_pdu(wStream* s, UINT16* type, UINT16* channel_id);
static BOOL rdp_write_share_control_header(wStream* s, UINT16 length, UINT16 type,
                                           UINT16 channel_id);
static BOOL rdp_write_share_data_header(wStream* s, UINT16 length, BYTE type, UINT32 share_id);

/**
 * @brief Read RDP Security Header.
 * msdn{cc240579}
 *
 * @param s stream
 * @param flags security flags
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL rdp_read_security_header(wStream* s, UINT16* flags, UINT16* length)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(flags);

	/* Basic Security Header */
	if ((length && (*length < 4)))
	{
		WLog_WARN(TAG, "invalid security header length, have %" PRIu16 ", must be >= 4", *length);
		return FALSE;
	}
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, *flags); /* flags */
	Stream_Seek(s, 2);             /* flagsHi (unused) */

	if (length)
		*length -= 4;

	return TRUE;
}

/**
 * Write RDP Security Header.
 * msdn{cc240579}
 * @param s stream
 * @param flags security flags
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL rdp_write_security_header(wStream* s, UINT16 flags)
{
	WINPR_ASSERT(s);

	if (Stream_GetRemainingCapacity(s) < 4)
		return FALSE;

	/* Basic Security Header */
	Stream_Write_UINT16(s, flags); /* flags */
	Stream_Write_UINT16(s, 0);     /* flagsHi (unused) */
	return TRUE;
}

BOOL rdp_read_share_control_header(wStream* s, UINT16* tpktLength, UINT16* remainingLength,
                                   UINT16* type, UINT16* channel_id)
{
	UINT16 len;
	UINT16 tmp;

	WINPR_ASSERT(s);
	WINPR_ASSERT(type);
	WINPR_ASSERT(channel_id);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	/* Share Control Header */
	Stream_Read_UINT16(s, len); /* totalLength */

	/* If length is 0x8000 then we actually got a flow control PDU that we should ignore
	 http://msdn.microsoft.com/en-us/library/cc240576.aspx */
	if (len == 0x8000)
	{
		if (!rdp_read_flow_control_pdu(s, type, channel_id))
			return FALSE;
		*channel_id = 0;
		if (tpktLength)
			*tpktLength = 8; /* Flow control PDU is 8 bytes */
		if (remainingLength)
			*remainingLength = 0;
		return TRUE;
	}

	if (len < 4U)
	{
		WLog_ERR(TAG, "Invalid share control header, length is %" PRIu16 ", must be >4", len);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, len - 2))
		return FALSE;
	if (tpktLength)
		*tpktLength = len;

	Stream_Read_UINT16(s, tmp); /* pduType */
	*type = tmp & 0x0F;         /* type is in the 4 least significant bits */
	if (len > 5)
	{
		Stream_Read_UINT16(s, *channel_id); /* pduSource */
		if (remainingLength)
			*remainingLength = len - 6;
	}
	else
	{
		*channel_id = 0; /* Windows XP can send such short DEACTIVATE_ALL PDUs. */
		if (remainingLength)
			*remainingLength = len - 4;
	}

	return TRUE;
}

BOOL rdp_write_share_control_header(wStream* s, UINT16 length, UINT16 type, UINT16 channel_id)
{
	WINPR_ASSERT(s);

	if (length < RDP_PACKET_HEADER_MAX_LENGTH)
		return FALSE;
	if (Stream_GetRemainingCapacity(s) < 6)
		return FALSE;
	length -= RDP_PACKET_HEADER_MAX_LENGTH;
	/* Share Control Header */
	Stream_Write_UINT16(s, length);      /* totalLength */
	Stream_Write_UINT16(s, type | 0x10); /* pduType */
	Stream_Write_UINT16(s, channel_id);  /* pduSource */
	return TRUE;
}

BOOL rdp_read_share_data_header(wStream* s, UINT16* length, BYTE* type, UINT32* shareId,
                                BYTE* compressedType, UINT16* compressedLength)
{
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return FALSE;

	/* Share Data Header */
	Stream_Read_UINT32(s, *shareId);          /* shareId (4 bytes) */
	Stream_Seek_UINT8(s);                     /* pad1 (1 byte) */
	Stream_Seek_UINT8(s);                     /* streamId (1 byte) */
	Stream_Read_UINT16(s, *length);           /* uncompressedLength (2 bytes) */
	Stream_Read_UINT8(s, *type);              /* pduType2, Data PDU Type (1 byte) */
	Stream_Read_UINT8(s, *compressedType);    /* compressedType (1 byte) */
	Stream_Read_UINT16(s, *compressedLength); /* compressedLength (2 bytes) */
	return TRUE;
}

BOOL rdp_write_share_data_header(wStream* s, UINT16 length, BYTE type, UINT32 share_id)
{
	const size_t headerLen = RDP_PACKET_HEADER_MAX_LENGTH + RDP_SHARE_CONTROL_HEADER_LENGTH +
	                         RDP_SHARE_DATA_HEADER_LENGTH;

	WINPR_ASSERT(s);

	if (length < headerLen)
		return FALSE;
	length -= headerLen;
	if (Stream_GetRemainingCapacity(s) < 12)
		return FALSE;

	/* Share Data Header */
	Stream_Write_UINT32(s, share_id);  /* shareId (4 bytes) */
	Stream_Write_UINT8(s, 0);          /* pad1 (1 byte) */
	Stream_Write_UINT8(s, STREAM_LOW); /* streamId (1 byte) */
	Stream_Write_UINT16(s, length);    /* uncompressedLength (2 bytes) */
	Stream_Write_UINT8(s, type);       /* pduType2, Data PDU Type (1 byte) */
	Stream_Write_UINT8(s, 0);          /* compressedType (1 byte) */
	Stream_Write_UINT16(s, 0);         /* compressedLength (2 bytes) */
	return TRUE;
}

static BOOL rdp_security_stream_init(rdpRdp* rdp, wStream* s, BOOL sec_header)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	if (rdp->do_crypt)
	{
		if (!Stream_SafeSeek(s, 12))
			return FALSE;

		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
		{
			if (!Stream_SafeSeek(s, 4))
				return FALSE;
		}

		rdp->sec_flags |= SEC_ENCRYPT;

		if (rdp->do_secure_checksum)
			rdp->sec_flags |= SEC_SECURE_CHECKSUM;
	}
	else if (rdp->sec_flags != 0 || sec_header)
	{
		if (!Stream_SafeSeek(s, 4))
			return FALSE;
	}

	return TRUE;
}

wStream* rdp_send_stream_init(rdpRdp* rdp)
{
	wStream* s;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->transport);

	s = transport_send_stream_init(rdp->transport, 4096);

	if (!s)
		return NULL;

	if (!Stream_SafeSeek(s, RDP_PACKET_HEADER_MAX_LENGTH))
		goto fail;

	if (!rdp_security_stream_init(rdp, s, FALSE))
		goto fail;

	return s;
fail:
	Stream_Release(s);
	return NULL;
}

wStream* rdp_send_stream_pdu_init(rdpRdp* rdp)
{
	wStream* s = rdp_send_stream_init(rdp);

	if (!s)
		return NULL;

	if (!Stream_SafeSeek(s, RDP_SHARE_CONTROL_HEADER_LENGTH))
		goto fail;

	return s;
fail:
	Stream_Release(s);
	return NULL;
}

wStream* rdp_data_pdu_init(rdpRdp* rdp)
{
	wStream* s = rdp_send_stream_pdu_init(rdp);

	if (!s)
		return NULL;

	if (!Stream_SafeSeek(s, RDP_SHARE_DATA_HEADER_LENGTH))
		goto fail;

	return s;
fail:
	Stream_Release(s);
	return NULL;
}

BOOL rdp_set_error_info(rdpRdp* rdp, UINT32 errorInfo)
{
	WINPR_ASSERT(rdp);

	rdp->errorInfo = errorInfo;

	if (rdp->errorInfo != ERRINFO_SUCCESS)
	{
		rdpContext* context = rdp->context;
		WINPR_ASSERT(context);

		rdp_print_errinfo(rdp->errorInfo);

		if (context)
		{
			freerdp_set_last_error_log(context, MAKE_FREERDP_ERROR(ERRINFO, errorInfo));

			if (context->pubSub)
			{
				ErrorInfoEventArgs e = { 0 };
				EventArgsInit(&e, "freerdp");
				e.code = rdp->errorInfo;
				PubSub_OnErrorInfo(context->pubSub, context, &e);
			}
		}
		else
			WLog_ERR(TAG, "%s missing context=%p", __FUNCTION__, context);
	}
	else
	{
		freerdp_set_last_error_log(rdp->context, FREERDP_ERROR_SUCCESS);
	}

	return TRUE;
}

wStream* rdp_message_channel_pdu_init(rdpRdp* rdp)
{
	wStream* s;

	WINPR_ASSERT(rdp);

	s = transport_send_stream_init(rdp->transport, 4096);

	if (!s)
		return NULL;

	if (!Stream_SafeSeek(s, RDP_PACKET_HEADER_MAX_LENGTH))
		goto fail;

	if (!rdp_security_stream_init(rdp, s, TRUE))
		goto fail;

	return s;
fail:
	Stream_Release(s);
	return NULL;
}

/**
 * Read an RDP packet header.
 * @param rdp rdp module
 * @param s stream
 * @param length RDP packet length
 * @param channelId channel id
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL rdp_read_header(rdpRdp* rdp, wStream* s, UINT16* length, UINT16* channelId)
{
	BYTE li;
	BYTE byte;
	BYTE code;
	BYTE choice;
	UINT16 initiator;
	DomainMCSPDU MCSPDU;
	DomainMCSPDU domainMCSPDU;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	WINPR_ASSERT(s);
	MCSPDU = (rdp->settings->ServerMode) ? DomainMCSPDU_SendDataRequest
	                                     : DomainMCSPDU_SendDataIndication;

	*channelId = 0; /* Initialize in case of early abort */
	if (!tpkt_read_header(s, length))
		return FALSE;

	if (!tpdu_read_header(s, &code, &li, *length))
		return FALSE;

	if (code != X224_TPDU_DATA)
	{
		if (code == X224_TPDU_DISCONNECT_REQUEST)
		{
			utils_abort_connect(rdp);
			return TRUE;
		}

		WLog_WARN(TAG, "Unexpected X224 TPDU type %s [%08" PRIx32 "] instead of %s",
		          tpdu_type_to_string(code), code, tpdu_type_to_string(X224_TPDU_DATA));
		return FALSE;
	}

	if (!per_read_choice(s, &choice))
		return FALSE;

	domainMCSPDU = (DomainMCSPDU)(choice >> 2);

	if (domainMCSPDU != MCSPDU)
	{
		if (domainMCSPDU != DomainMCSPDU_DisconnectProviderUltimatum)
		{
			WLog_WARN(TAG, "Received %s instead of %s", mcs_domain_pdu_string(domainMCSPDU),
			          mcs_domain_pdu_string(MCSPDU));
			return FALSE;
		}
	}

	MCSPDU = domainMCSPDU;

	if (*length < 8U)
	{
		WLog_WARN(TAG, "TPDU invalid length, got %" PRIu16 ", expected at least 8", *length);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, *length - 8))
		return FALSE;

	if (MCSPDU == DomainMCSPDU_DisconnectProviderUltimatum)
	{
		int reason = 0;
		TerminateEventArgs e = { 0 };
		rdpContext* context;

		if (!mcs_recv_disconnect_provider_ultimatum(rdp->mcs, s, &reason))
			return FALSE;

		context = rdp->context;
		context->disconnectUltimatum = reason;

		if (rdp->errorInfo == ERRINFO_SUCCESS)
		{
			/**
			 * Some servers like Windows Server 2008 R2 do not send the error info pdu
			 * when the user logs off like they should. Map DisconnectProviderUltimatum
			 * to a ERRINFO_LOGOFF_BY_USER when the errinfo code is ERRINFO_SUCCESS.
			 */
			if (reason == Disconnect_Ultimatum_provider_initiated)
				rdp_set_error_info(rdp, ERRINFO_RPC_INITIATED_DISCONNECT);
			else if (reason == Disconnect_Ultimatum_user_requested)
				rdp_set_error_info(rdp, ERRINFO_LOGOFF_BY_USER);
			else
				rdp_set_error_info(rdp, ERRINFO_RPC_INITIATED_DISCONNECT);
		}

		WLog_DBG(TAG, "DisconnectProviderUltimatum: reason: %d", reason);
		utils_abort_connect(rdp);
		EventArgsInit(&e, "freerdp");
		e.code = 0;
		PubSub_OnTerminate(rdp->pubSub, context, &e);
		return TRUE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 5))
		return FALSE;

	if (!per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID)) /* initiator (UserId) */
		return FALSE;

	if (!per_read_integer16(s, channelId, 0)) /* channelId */
		return FALSE;

	Stream_Read_UINT8(s, byte); /* dataPriority + Segmentation (0x70) */

	if (!per_read_length(s, length)) /* userData (OCTET_STRING) */
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, *length))
		return FALSE;

	return TRUE;
}

/**
 * Write an RDP packet header.
 * @param rdp rdp module
 * @param s stream
 * @param length RDP packet length
 * @param channelId channel id
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL rdp_write_header(rdpRdp* rdp, wStream* s, UINT16 length, UINT16 channelId)
{
	DomainMCSPDU MCSPDU;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	WINPR_ASSERT(s);
	WINPR_ASSERT(length >= RDP_PACKET_HEADER_MAX_LENGTH);

	MCSPDU = (rdp->settings->ServerMode) ? DomainMCSPDU_SendDataIndication
	                                     : DomainMCSPDU_SendDataRequest;

	if ((rdp->sec_flags & SEC_ENCRYPT) &&
	    (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS))
	{
		const UINT16 body_length = length - RDP_PACKET_HEADER_MAX_LENGTH;
		const UINT16 pad = 8 - (body_length % 8);

		if (pad != 8)
			length += pad;
	}

	if (!mcs_write_domain_mcspdu_header(s, MCSPDU, length, 0))
		return FALSE;
	if (!per_write_integer16(s, rdp->mcs->userId, MCS_BASE_CHANNEL_ID)) /* initiator */
		return FALSE;
	if (!per_write_integer16(s, channelId, 0)) /* channelId */
		return FALSE;
	if (!Stream_EnsureRemainingCapacity(s, 3))
		return FALSE;
	Stream_Write_UINT8(s, 0x70); /* dataPriority + segmentation */
	/*
	 * We always encode length in two bytes, even though we could use
	 * only one byte if length <= 0x7F. It is just easier that way,
	 * because we can leave room for fixed-length header, store all
	 * the data first and then store the header.
	 */
	length = (length - RDP_PACKET_HEADER_MAX_LENGTH) | 0x8000;
	Stream_Write_UINT16_BE(s, length); /* userData (OCTET_STRING) */
	return TRUE;
}

static BOOL rdp_security_stream_out(rdpRdp* rdp, wStream* s, int length, UINT32 sec_flags,
                                    UINT32* pad)
{
	BYTE* data;
	BOOL status;
	sec_flags |= rdp->sec_flags;
	*pad = 0;

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
				Stream_Write_UINT8(s, 0x1);   /* TSFIPS_VERSION 1*/
				/* handle padding */
				*pad = 8 - (length % 8);

				if (*pad == 8)
					*pad = 0;

				if (*pad)
					memset(data + length, 0, *pad);

				Stream_Write_UINT8(s, *pad);

				if (!security_hmac_signature(data, length, Stream_Pointer(s), rdp))
					return FALSE;

				Stream_Seek(s, 8);
				security_fips_encrypt(data, length + *pad, rdp);
			}
			else
			{
				data = Stream_Pointer(s) + 8;
				length = length - (data - Stream_Buffer(s));

				if (sec_flags & SEC_SECURE_CHECKSUM)
					status =
					    security_salted_mac_signature(rdp, data, length, TRUE, Stream_Pointer(s));
				else
					status = security_mac_signature(rdp, data, length, Stream_Pointer(s));

				if (!status)
					return FALSE;

				Stream_Seek(s, 8);

				if (!security_encrypt(Stream_Pointer(s), length, rdp))
					return FALSE;
			}
		}

		rdp->sec_flags = 0;
	}

	return TRUE;
}

static UINT32 rdp_get_sec_bytes(rdpRdp* rdp, UINT16 sec_flags)
{
	UINT32 sec_bytes;

	if (rdp->sec_flags & SEC_ENCRYPT)
	{
		sec_bytes = 12;

		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			sec_bytes += 4;
	}
	else if (rdp->sec_flags != 0 || sec_flags != 0)
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
 * Send an RDP packet.
 * @param rdp RDP module
 * @param s stream
 * @param channel_id channel id
 */

BOOL rdp_send(rdpRdp* rdp, wStream* s, UINT16 channel_id)
{
	BOOL rc = FALSE;
	UINT32 pad;
	UINT16 length;

	if (!s)
		return FALSE;

	if (!rdp)
		goto fail;

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);
	rdp_write_header(rdp, s, length, channel_id);

	if (!rdp_security_stream_out(rdp, s, length, 0, &pad))
		goto fail;

	length += pad;
	Stream_SetPosition(s, length);
	Stream_SealLength(s);

	if (transport_write(rdp->transport, s) < 0)
		goto fail;

	rc = TRUE;
fail:
	Stream_Release(s);
	return rc;
}

BOOL rdp_send_pdu(rdpRdp* rdp, wStream* s, UINT16 type, UINT16 channel_id)
{
	UINT16 length;
	UINT32 sec_bytes;
	size_t sec_hold;
	UINT32 pad;

	if (!rdp || !s)
		return FALSE;

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);
	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);
	sec_bytes = rdp_get_sec_bytes(rdp, 0);
	sec_hold = Stream_GetPosition(s);
	Stream_Seek(s, sec_bytes);
	if (!rdp_write_share_control_header(s, length - sec_bytes, type, channel_id))
		return FALSE;
	Stream_SetPosition(s, sec_hold);

	if (!rdp_security_stream_out(rdp, s, length, 0, &pad))
		return FALSE;

	length += pad;
	Stream_SetPosition(s, length);
	Stream_SealLength(s);

	if (transport_write(rdp->transport, s) < 0)
		return FALSE;

	return TRUE;
}

BOOL rdp_send_data_pdu(rdpRdp* rdp, wStream* s, BYTE type, UINT16 channel_id)
{
	BOOL rc = FALSE;
	size_t length;
	UINT32 sec_bytes;
	size_t sec_hold;
	UINT32 pad;

	if (!s)
		return FALSE;

	if (!rdp)
		goto fail;

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);
	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);
	sec_bytes = rdp_get_sec_bytes(rdp, 0);
	sec_hold = Stream_GetPosition(s);
	Stream_Seek(s, sec_bytes);
	if (!rdp_write_share_control_header(s, length - sec_bytes, PDU_TYPE_DATA, channel_id))
		goto fail;
	if (!rdp_write_share_data_header(s, length - sec_bytes, type, rdp->settings->ShareId))
		goto fail;
	Stream_SetPosition(s, sec_hold);

	if (!rdp_security_stream_out(rdp, s, length, 0, &pad))
		goto fail;

	length += pad;
	Stream_SetPosition(s, length);
	Stream_SealLength(s);
	WLog_DBG(TAG, "%s: sending data (type=0x%x size=%" PRIuz " channelId=%" PRIu16 ")",
	         __FUNCTION__, type, Stream_Length(s), channel_id);

	rdp->outPackets++;
	if (transport_write(rdp->transport, s) < 0)
		goto fail;

	rc = TRUE;
fail:
	Stream_Release(s);
	return rc;
}

BOOL rdp_send_message_channel_pdu(rdpRdp* rdp, wStream* s, UINT16 sec_flags)
{
	BOOL rc = FALSE;
	UINT16 length;
	UINT32 pad;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);
	rdp_write_header(rdp, s, length, rdp->mcs->messageChannelId);

	if (!rdp_security_stream_out(rdp, s, length, sec_flags, &pad))
		goto fail;

	length += pad;
	Stream_SetPosition(s, length);
	Stream_SealLength(s);

	if (transport_write(rdp->transport, s) < 0)
		goto fail;

	rc = TRUE;
fail:
	Stream_Release(s);
	return rc;
}

static BOOL rdp_recv_server_shutdown_denied_pdu(rdpRdp* rdp, wStream* s)
{
	return TRUE;
}

static BOOL rdp_recv_server_set_keyboard_indicators_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 unitId;
	UINT16 ledFlags;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	rdpContext* context = rdp->context;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->update);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, unitId);   /* unitId (2 bytes) */
	Stream_Read_UINT16(s, ledFlags); /* ledFlags (2 bytes) */
	return IFCALLRESULT(TRUE, context->update->SetKeyboardIndicators, context, ledFlags);
}

static BOOL rdp_recv_server_set_keyboard_ime_status_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 unitId;
	UINT32 imeState;
	UINT32 imeConvMode;

	if (!rdp || !rdp->input)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 10))
		return FALSE;

	Stream_Read_UINT16(s, unitId);      /* unitId (2 bytes) */
	Stream_Read_UINT32(s, imeState);    /* imeState (4 bytes) */
	Stream_Read_UINT32(s, imeConvMode); /* imeConvMode (4 bytes) */
	IFCALL(rdp->update->SetKeyboardImeStatus, rdp->context, unitId, imeState, imeConvMode);
	return TRUE;
}

static BOOL rdp_recv_set_error_info_data_pdu(rdpRdp* rdp, wStream* s)
{
	UINT32 errorInfo;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, errorInfo); /* errorInfo (4 bytes) */
	return rdp_set_error_info(rdp, errorInfo);
}

static BOOL rdp_recv_server_auto_reconnect_status_pdu(rdpRdp* rdp, wStream* s)
{
	UINT32 arcStatus;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, arcStatus); /* arcStatus (4 bytes) */
	WLog_WARN(TAG, "AutoReconnectStatus: 0x%08" PRIX32 "", arcStatus);
	return TRUE;
}

static BOOL rdp_recv_server_status_info_pdu(rdpRdp* rdp, wStream* s)
{
	UINT32 statusCode;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, statusCode); /* statusCode (4 bytes) */

	if (rdp->update->ServerStatusInfo)
		return rdp->update->ServerStatusInfo(rdp->context, statusCode);

	return TRUE;
}

static BOOL rdp_recv_monitor_layout_pdu(rdpRdp* rdp, wStream* s)
{
	UINT32 index;
	UINT32 monitorCount;
	MONITOR_DEF* monitor;
	MONITOR_DEF* monitorDefArray;
	BOOL ret = TRUE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, monitorCount); /* monitorCount (4 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20ull * monitorCount))
		return FALSE;

	monitorDefArray = (MONITOR_DEF*)calloc(monitorCount, sizeof(MONITOR_DEF));

	if (!monitorDefArray)
		return FALSE;

	for (monitor = monitorDefArray, index = 0; index < monitorCount; index++, monitor++)
	{
		Stream_Read_INT32(s, monitor->left);   /* left (4 bytes) */
		Stream_Read_INT32(s, monitor->top);    /* top (4 bytes) */
		Stream_Read_INT32(s, monitor->right);  /* right (4 bytes) */
		Stream_Read_INT32(s, monitor->bottom); /* bottom (4 bytes) */
		Stream_Read_UINT32(s, monitor->flags); /* flags (4 bytes) */
	}

	IFCALLRET(rdp->update->RemoteMonitors, ret, rdp->context, monitorCount, monitorDefArray);
	free(monitorDefArray);
	return ret;
}

state_run_t rdp_recv_data_pdu(rdpRdp* rdp, wStream* s)
{
	BYTE type;
	wStream* cs;
	UINT16 length;
	UINT32 shareId;
	BYTE compressedType;
	UINT16 compressedLength;

	if (!rdp_read_share_data_header(s, &length, &type, &shareId, &compressedType,
	                                &compressedLength))
	{
		WLog_ERR(TAG, "rdp_read_share_data_header() failed");
		return STATE_RUN_FAILED;
	}

	cs = s;

	if (compressedType & PACKET_COMPRESSED)
	{
		UINT32 DstSize = 0;
		const BYTE* pDstData = NULL;
		UINT16 SrcSize = compressedLength - 18;

		if ((compressedLength < 18) || (!Stream_CheckAndLogRequiredLength(TAG, s, SrcSize)))
		{
			WLog_ERR(TAG, "bulk_decompress: not enough bytes for compressedLength %" PRIu16 "",
			         compressedLength);
			return STATE_RUN_FAILED;
		}

		if (bulk_decompress(rdp->bulk, Stream_Pointer(s), SrcSize, &pDstData, &DstSize,
		                    compressedType))
		{
			cs = transport_take_from_pool(rdp->transport, DstSize);
			if (!cs)
			{
				WLog_ERR(TAG, "Couldn't take stream from pool");
				return STATE_RUN_FAILED;
			}

			Stream_SetPosition(cs, 0);
			Stream_Write(cs, pDstData, DstSize);
			Stream_SealLength(cs);
			Stream_SetPosition(cs, 0);
		}
		else
		{
			WLog_ERR(TAG, "bulk_decompress() failed");
			return STATE_RUN_FAILED;
		}

		Stream_Seek(s, SrcSize);
	}

	WLog_DBG(TAG, "recv %s Data PDU (0x%02" PRIX8 "), length: %" PRIu16 "",
	         data_pdu_type_to_string(type), type, length);

	switch (type)
	{
		case DATA_PDU_TYPE_UPDATE:
			if (!update_recv(rdp->update, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_UPDATE - update_recv() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_CONTROL:
			if (!rdp_recv_server_control_pdu(rdp, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_CONTROL - rdp_recv_server_control_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_POINTER:
			if (!update_recv_pointer(rdp->update, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_POINTER - update_recv_pointer() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_SYNCHRONIZE:
			if (!rdp_recv_server_synchronize_pdu(rdp, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_SYNCHRONIZE - rdp_recv_synchronize_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_PLAY_SOUND:
			if (!update_recv_play_sound(rdp->update, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_PLAY_SOUND - update_recv_play_sound() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_SHUTDOWN_DENIED:
			if (!rdp_recv_server_shutdown_denied_pdu(rdp, cs))
			{
				WLog_ERR(
				    TAG,
				    "DATA_PDU_TYPE_SHUTDOWN_DENIED - rdp_recv_server_shutdown_denied_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_SAVE_SESSION_INFO:
			if (!rdp_recv_save_session_info(rdp, cs))
			{
				WLog_ERR(TAG,
				         "DATA_PDU_TYPE_SAVE_SESSION_INFO - rdp_recv_save_session_info() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_FONT_MAP:
			if (!rdp_recv_font_map_pdu(rdp, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_FONT_MAP - rdp_recv_font_map_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_SET_KEYBOARD_INDICATORS:
			if (!rdp_recv_server_set_keyboard_indicators_pdu(rdp, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_SET_KEYBOARD_INDICATORS - "
				              "rdp_recv_server_set_keyboard_indicators_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_SET_KEYBOARD_IME_STATUS:
			if (!rdp_recv_server_set_keyboard_ime_status_pdu(rdp, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_SET_KEYBOARD_IME_STATUS - "
				              "rdp_recv_server_set_keyboard_ime_status_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_SET_ERROR_INFO:
			if (!rdp_recv_set_error_info_data_pdu(rdp, cs))
			{
				WLog_ERR(
				    TAG,
				    "DATA_PDU_TYPE_SET_ERROR_INFO - rdp_recv_set_error_info_data_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_ARC_STATUS:
			if (!rdp_recv_server_auto_reconnect_status_pdu(rdp, cs))
			{
				WLog_ERR(TAG, "DATA_PDU_TYPE_ARC_STATUS - "
				              "rdp_recv_server_auto_reconnect_status_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_STATUS_INFO:
			if (!rdp_recv_server_status_info_pdu(rdp, cs))
			{
				WLog_ERR(TAG,
				         "DATA_PDU_TYPE_STATUS_INFO - rdp_recv_server_status_info_pdu() failed");
				goto out_fail;
			}

			break;

		case DATA_PDU_TYPE_MONITOR_LAYOUT:
			if (!rdp_recv_monitor_layout_pdu(rdp, cs))
			{
				WLog_ERR(TAG,
				         "DATA_PDU_TYPE_MONITOR_LAYOUT - rdp_recv_monitor_layout_pdu() failed");
				goto out_fail;
			}

			break;

		default:
			WLog_WARN(TAG, "[UNHANDLED] %s Data PDU (0x%02" PRIX8 "), length: %" PRIu16 "",
			          data_pdu_type_to_string(type), type, length);
			break;
	}

	if (cs != s)
		Stream_Release(cs);

	return STATE_RUN_SUCCESS;
out_fail:

	if (cs != s)
		Stream_Release(cs);

	return STATE_RUN_FAILED;
}

state_run_t rdp_recv_message_channel_pdu(rdpRdp* rdp, wStream* s, UINT16 securityFlags)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	if (securityFlags & SEC_AUTODETECT_REQ)
	{
		/* Server Auto-Detect Request PDU */
		return autodetect_recv_request_packet(rdp->autodetect, RDP_TRANSPORT_TCP, s);
	}

	if (securityFlags & SEC_AUTODETECT_RSP)
	{
		/* Client Auto-Detect Response PDU */
		return autodetect_recv_response_packet(rdp->autodetect, RDP_TRANSPORT_TCP, s);
	}

	if (securityFlags & SEC_HEARTBEAT)
	{
		/* Heartbeat PDU */
		return rdp_recv_heartbeat_packet(rdp, s);
	}

	if (securityFlags & SEC_TRANSPORT_REQ)
	{
		return multitransport_recv_request(rdp->multitransport, s);
	}

	if (securityFlags & SEC_TRANSPORT_RSP)
	{
		return multitransport_recv_response(rdp->multitransport, s);
	}

	if (securityFlags & SEC_LICENSE_PKT)
	{
		return license_recv(rdp->license, s);
	}

	if (securityFlags & SEC_LICENSE_ENCRYPT_CS)
	{
		return license_recv(rdp->license, s);
	}

	if (securityFlags & SEC_LICENSE_ENCRYPT_SC)
	{
		return license_recv(rdp->license, s);
	}

	return STATE_RUN_SUCCESS;
}

state_run_t rdp_recv_out_of_sequence_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 type;
	UINT16 length;
	UINT16 channelId;

	if (!rdp_read_share_control_header(s, &length, NULL, &type, &channelId))
		return STATE_RUN_FAILED;

	if (type == PDU_TYPE_DATA)
	{
		return rdp_recv_data_pdu(rdp, s);
	}
	else if (type == PDU_TYPE_SERVER_REDIRECTION)
	{
		return rdp_recv_enhanced_security_redirection_packet(rdp, s);
	}
	else if (type == PDU_TYPE_FLOW_RESPONSE || type == PDU_TYPE_FLOW_STOP ||
	         type == PDU_TYPE_FLOW_TEST)
	{
		return STATE_RUN_SUCCESS;
	}
	else
	{
		return STATE_RUN_FAILED;
	}
}

BOOL rdp_read_flow_control_pdu(wStream* s, UINT16* type, UINT16* channel_id)
{
	/*
	 * Read flow control PDU - documented in FlowPDU section in T.128
	 * http://www.itu.int/rec/T-REC-T.128-199802-S/en
	 * The specification for the PDU has pad8bits listed BEFORE pduTypeFlow.
	 * However, so far pad8bits has always been observed to arrive AFTER pduTypeFlow.
	 * Switched the order of these two fields to match this observation.
	 */
	UINT8 pduType;

	WINPR_ASSERT(s);
	WINPR_ASSERT(type);
	WINPR_ASSERT(channel_id);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;
	Stream_Read_UINT8(s, pduType); /* pduTypeFlow */
	*type = pduType;
	Stream_Seek_UINT8(s);               /* pad8bits */
	Stream_Seek_UINT8(s);               /* flowIdentifier */
	Stream_Seek_UINT8(s);               /* flowNumber */
	Stream_Read_UINT16(s, *channel_id); /* pduSource */
	return TRUE;
}

/**
 * Decrypt an RDP packet.
 *
 * @param rdp RDP module
 * @param s stream
 * @param pLength A pointer to the result variable, must not be NULL
 * @param securityFlags the security flags to apply
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL rdp_decrypt(rdpRdp* rdp, wStream* s, UINT16* pLength, UINT16 securityFlags)
{
	BYTE cmac[8];
	BYTE wmac[8];
	BOOL status;
	INT32 length;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	WINPR_ASSERT(s);
	WINPR_ASSERT(pLength);

	length = *pLength;
	if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
	{
		UINT16 len;
		BYTE version, pad;
		BYTE* sig;
		INT64 padLength;

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
			return FALSE;

		Stream_Read_UINT16(s, len);    /* 0x10 */
		Stream_Read_UINT8(s, version); /* 0x1 */
		Stream_Read_UINT8(s, pad);
		sig = Stream_Pointer(s);
		Stream_Seek(s, 8); /* signature */
		length -= 12;
		padLength = length - pad;

		if ((length <= 0) || (padLength <= 0) || (padLength > UINT16_MAX))
		{
			WLog_ERR(TAG, "FATAL: invalid pad length %" PRId32, padLength);
			return FALSE;
		}

		if (!security_fips_decrypt(Stream_Pointer(s), length, rdp))
		{
			WLog_ERR(TAG, "FATAL: cannot decrypt");
			return FALSE; /* TODO */
		}

		if (!security_fips_check_signature(Stream_Pointer(s), length - pad, sig, rdp))
		{
			WLog_ERR(TAG, "FATAL: invalid packet signature");
			return FALSE; /* TODO */
		}

		Stream_SetLength(s, Stream_Length(s) - pad);
		*pLength = (UINT16)padLength;
		return TRUE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(wmac)))
		return FALSE;

	Stream_Read(s, wmac, sizeof(wmac));
	length -= sizeof(wmac);

	if (length <= 0)
	{
		WLog_ERR(TAG, "FATAL: invalid length field");
		return FALSE;
	}

	if (!security_decrypt(Stream_Pointer(s), length, rdp))
		return FALSE;

	if (securityFlags & SEC_SECURE_CHECKSUM)
		status = security_salted_mac_signature(rdp, Stream_Pointer(s), length, FALSE, cmac);
	else
		status = security_mac_signature(rdp, Stream_Pointer(s), length, cmac);

	if (!status)
		return FALSE;

	if (memcmp(wmac, cmac, sizeof(wmac)) != 0)
	{
		WLog_ERR(TAG, "WARNING: invalid packet signature");
		/*
		 * Because Standard RDP Security is totally broken,
		 * and cannot protect against MITM, don't treat signature
		 * verification failure as critical. This at least enables
		 * us to work with broken RDP clients and servers that
		 * generate invalid signatures.
		 */
		// return FALSE;
	}

	*pLength = length;
	return TRUE;
}

const char* pdu_type_to_str(UINT16 pduType)
{
	static char buffer[1024] = { 0 };
	switch (pduType)
	{
		case PDU_TYPE_DEMAND_ACTIVE:
			return "PDU_TYPE_DEMAND_ACTIVE";
		case PDU_TYPE_CONFIRM_ACTIVE:
			return "PDU_TYPE_CONFIRM_ACTIVE";
		case PDU_TYPE_DEACTIVATE_ALL:
			return "PDU_TYPE_DEACTIVATE_ALL";
		case PDU_TYPE_DATA:
			return "PDU_TYPE_DATA";
		case PDU_TYPE_SERVER_REDIRECTION:
			return "PDU_TYPE_SERVER_REDIRECTION";
		case PDU_TYPE_FLOW_TEST:
			return "PDU_TYPE_FLOW_TEST";
		case PDU_TYPE_FLOW_RESPONSE:
			return "PDU_TYPE_FLOW_RESPONSE";
		case PDU_TYPE_FLOW_STOP:
			return "PDU_TYPE_FLOW_STOP";
		default:
			_snprintf(buffer, sizeof(buffer), "UNKNOWN %04" PRIx16, pduType);
			return buffer;
	}
}

/**
 * Process an RDP packet.
 * @param rdp RDP module
 * @param s stream
 */

static state_run_t rdp_recv_tpkt_pdu(rdpRdp* rdp, wStream* s)
{
	state_run_t rc = STATE_RUN_SUCCESS;
	UINT16 length;
	UINT16 pduType;
	UINT16 pduSource;
	UINT16 channelId = 0;
	UINT16 securityFlags = 0;
	freerdp* instance;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->context);
	WINPR_ASSERT(s);

	instance = rdp->context->instance;
	WINPR_ASSERT(instance);

	if (!rdp_read_header(rdp, s, &length, &channelId))
		return STATE_RUN_FAILED;

	if (freerdp_shall_disconnect_context(rdp->context))
		return STATE_RUN_SUCCESS;

	if (rdp->autodetect->bandwidthMeasureStarted)
	{
		rdp->autodetect->bandwidthMeasureByteCount += length;
	}

	if (rdp->settings->UseRdpSecurityLayer)
	{
		if (!rdp_read_security_header(s, &securityFlags, &length))
		{
			WLog_ERR(TAG, "rdp_recv_tpkt_pdu: rdp_read_security_header() fail");
			return STATE_RUN_FAILED;
		}

		if (securityFlags & (SEC_ENCRYPT | SEC_REDIRECTION_PKT))
		{
			if (!rdp_decrypt(rdp, s, &length, securityFlags))
			{
				WLog_ERR(TAG, "rdp_decrypt failed");
				return STATE_RUN_FAILED;
			}
		}

		if (securityFlags & SEC_REDIRECTION_PKT)
		{
			/*
			 * [MS-RDPBCGR] 2.2.13.2.1
			 *  - no share control header, nor the 2 byte pad
			 */
			Stream_Rewind(s, 2);
			rdp->inPackets++;

			rc = rdp_recv_enhanced_security_redirection_packet(rdp, s);
			goto out;
		}
	}

	if (channelId == MCS_GLOBAL_CHANNEL_ID)
	{
		while (Stream_GetRemainingLength(s) > 3)
		{
			wStream subbuffer;
			wStream* sub;
			size_t diff;
			UINT16 remain;

			if (!rdp_read_share_control_header(s, NULL, &remain, &pduType, &pduSource))
			{
				WLog_ERR(TAG, "rdp_recv_tpkt_pdu: rdp_read_share_control_header() fail");
				return STATE_RUN_FAILED;
			}

			sub = Stream_StaticInit(&subbuffer, Stream_Pointer(s), remain);
			if (!Stream_SafeSeek(s, remain))
				return STATE_RUN_FAILED;

			rdp->settings->PduSource = pduSource;
			rdp->inPackets++;

			switch (pduType)
			{
				case PDU_TYPE_DATA:
					rc = rdp_recv_data_pdu(rdp, sub);
					if (state_run_failed(rc))
						return rc;
					break;

				case PDU_TYPE_DEACTIVATE_ALL:
					if (!rdp_recv_deactivate_all(rdp, sub))
					{
						WLog_ERR(TAG, "rdp_recv_tpkt_pdu: rdp_recv_deactivate_all() fail");
						return STATE_RUN_FAILED;
					}

					break;

				case PDU_TYPE_SERVER_REDIRECTION:
					return rdp_recv_enhanced_security_redirection_packet(rdp, sub);

				case PDU_TYPE_FLOW_RESPONSE:
				case PDU_TYPE_FLOW_STOP:
				case PDU_TYPE_FLOW_TEST:
					WLog_DBG(TAG, "flow message 0x%04" PRIX16 "", pduType);
					/* http://msdn.microsoft.com/en-us/library/cc240576.aspx */
					if (!Stream_SafeSeek(sub, remain))
						return STATE_RUN_FAILED;
					break;

				default:
					WLog_ERR(TAG, "incorrect PDU type: 0x%04" PRIX16 "", pduType);
					break;
			}

			diff = Stream_GetRemainingLength(sub);
			if (diff > 0)
			{
				WLog_WARN(TAG,
				          "pduType %s not properly parsed, %" PRIdz
				          " bytes remaining unhandled. Skipping.",
				          pdu_type_to_str(pduType), diff);
			}
		}
	}
	else if (rdp->mcs->messageChannelId && (channelId == rdp->mcs->messageChannelId))
	{
		if (!rdp->settings->UseRdpSecurityLayer)
			if (!rdp_read_security_header(s, &securityFlags, NULL))
				return STATE_RUN_FAILED;
		rdp->inPackets++;
		rc = rdp_recv_message_channel_pdu(rdp, s, securityFlags);
	}
	else
	{
		rdp->inPackets++;

		if (!freerdp_channel_process(instance, s, channelId, length))
			return STATE_RUN_FAILED;
	}

out:
	if (!tpkt_ensure_stream_consumed(s, length))
		return STATE_RUN_FAILED;
	return rc;
}

static state_run_t rdp_recv_fastpath_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 length;
	rdpFastPath* fastpath;
	fastpath = rdp->fastpath;

	if (!fastpath_read_header_rdp(fastpath, s, &length))
	{
		WLog_ERR(TAG, "rdp_recv_fastpath_pdu: fastpath_read_header_rdp() fail");
		return STATE_RUN_FAILED;
	}

	if ((length == 0) || (!Stream_CheckAndLogRequiredLength(TAG, s, length)))
	{
		WLog_ERR(TAG, "incorrect FastPath PDU header length %" PRIu16 "", length);
		return STATE_RUN_FAILED;
	}

	if (rdp->autodetect->bandwidthMeasureStarted)
	{
		rdp->autodetect->bandwidthMeasureByteCount += length;
	}

	if (fastpath_get_encryption_flags(fastpath) & FASTPATH_OUTPUT_ENCRYPTED)
	{
		UINT16 flags = (fastpath_get_encryption_flags(fastpath) & FASTPATH_OUTPUT_SECURE_CHECKSUM)
		                   ? SEC_SECURE_CHECKSUM
		                   : 0;

		if (!rdp_decrypt(rdp, s, &length, flags))
		{
			WLog_ERR(TAG, "rdp_recv_fastpath_pdu: rdp_decrypt() fail");
			return STATE_RUN_FAILED;
		}
	}

	return fastpath_recv_updates(rdp->fastpath, s);
}

static state_run_t rdp_recv_pdu(rdpRdp* rdp, wStream* s)
{
	const int rc = tpkt_verify_header(s);
	if (rc > 0)
		return rdp_recv_tpkt_pdu(rdp, s);
	else if (rc == 0)
		return rdp_recv_fastpath_pdu(rdp, s);
	else
		return STATE_RUN_FAILED;
}

static state_run_t rdp_handle_sc_flags(rdpRdp* rdp, wStream* s, UINT32 flag,
                                       CONNECTION_STATE nextState)
{
	const UINT32 mask = FINALIZE_SC_SYNCHRONIZE_PDU | FINALIZE_SC_CONTROL_COOPERATE_PDU |
	                    FINALIZE_SC_CONTROL_GRANTED_PDU | FINALIZE_SC_FONT_MAP_PDU;
	state_run_t status = rdp_recv_pdu(rdp, s);
	if (state_run_success(status))
	{
		const UINT32 flags = rdp->finalize_sc_pdus & mask;
		if ((flags & flag) == flag)
		{
			if (!rdp_client_transition_to_state(rdp, nextState))
				status = STATE_RUN_FAILED;
			else
				status = STATE_RUN_SUCCESS;
		}
		else
		{
			char flag_buffer[256] = { 0 };
			char mask_buffer[256] = { 0 };
			WLog_WARN(TAG, "[%s] unexpected server message, expected flag %s [have %s]",
			          rdp_get_state_string(rdp),
			          rdp_finalize_flags_to_str(flag, flag_buffer, sizeof(flag_buffer)),
			          rdp_finalize_flags_to_str(flags, mask_buffer, sizeof(mask_buffer)));
		}
	}
	return status;
}

static state_run_t rdp_client_exchange_monitor_layout(rdpRdp* rdp, wStream* s)
{
	WINPR_ASSERT(rdp);

	const UINT32 old = rdp->finalize_sc_pdus;
	state_run_t status = rdp_recv_pdu(rdp, s);

	/* This PDU is optional, so if we received a finalize PDU continue there */
	if (state_run_success(status))
	{
		const BOOL changed = old != rdp->finalize_sc_pdus;
		status = rdp_client_connect_finalize(rdp);
		if (changed && state_run_success(status))
			status = STATE_RUN_TRY_AGAIN;
	}
	return status;
}

static state_run_t rdp_recv_callback_int(rdpTransport* transport, wStream* s, void* extra)
{
	state_run_t status = STATE_RUN_SUCCESS;
	rdpRdp* rdp = (rdpRdp*)extra;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	switch (rdp_get_state(rdp))
	{
		case CONNECTION_STATE_NEGO:
			if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_CREATE_REQUEST))
				status = STATE_RUN_FAILED;
			else
				status = STATE_RUN_CONTINUE;
			break;
		case CONNECTION_STATE_NLA:
			if (nla_get_state(rdp->nla) < NLA_STATE_AUTH_INFO)
			{
				if (nla_recv_pdu(rdp->nla, s) < 1)
				{
					WLog_ERR(TAG, "%s: %s - nla_recv_pdu() fail", __FUNCTION__,
					         rdp_get_state_string(rdp));
					status = STATE_RUN_FAILED;
				}
			}
			else if (nla_get_state(rdp->nla) == NLA_STATE_POST_NEGO)
			{
				nego_recv(rdp->transport, s, (void*)rdp->nego);

				if (nego_get_state(rdp->nego) != NEGO_STATE_FINAL)
				{
					WLog_ERR(TAG, "%s: %s - nego_recv() fail", __FUNCTION__,
					         rdp_get_state_string(rdp));
					status = STATE_RUN_FAILED;
				}
				else if (!nla_set_state(rdp->nla, NLA_STATE_FINAL))
					status = STATE_RUN_FAILED;
			}

			if (state_run_success(status))
			{
				if (nla_get_state(rdp->nla) == NLA_STATE_AUTH_INFO)
				{
					transport_set_nla_mode(rdp->transport, FALSE);

					if (rdp->settings->VmConnectMode)
					{
						if (!nego_set_state(rdp->nego, NEGO_STATE_NLA))
							status = STATE_RUN_FAILED;
						else if (!nego_set_requested_protocols(rdp->nego,
						                                       PROTOCOL_HYBRID | PROTOCOL_SSL))
							status = STATE_RUN_FAILED;
						else if (!nego_send_negotiation_request(rdp->nego))
							status = STATE_RUN_FAILED;
						else if (!nla_set_state(rdp->nla, NLA_STATE_POST_NEGO))
							status = STATE_RUN_FAILED;
					}
					else
					{
						if (!nla_set_state(rdp->nla, NLA_STATE_FINAL))
							status = STATE_RUN_FAILED;
					}
				}
			}
			if (state_run_success(status))
			{

				if (nla_get_state(rdp->nla) == NLA_STATE_FINAL)
				{
					if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_CREATE_REQUEST))
						status = STATE_RUN_FAILED;
					else
						status = STATE_RUN_CONTINUE;
				}
			}
			break;

		case CONNECTION_STATE_MCS_CREATE_REQUEST:
			if (!mcs_client_begin(rdp->mcs))
			{
				WLog_ERR(TAG, "%s: %s - mcs_client_begin() fail", __FUNCTION__,
				         rdp_get_state_string(rdp));
				status = STATE_RUN_FAILED;
			}
			else if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_CREATE_RESPONSE))
				status = STATE_RUN_FAILED;
			else if (Stream_GetRemainingLength(s) > 0)
				status = STATE_RUN_CONTINUE;
			break;

		case CONNECTION_STATE_MCS_CREATE_RESPONSE:
			if (!mcs_recv_connect_response(rdp->mcs, s))
			{
				WLog_ERR(TAG, "mcs_recv_connect_response failure");
				status = STATE_RUN_FAILED;
			}
			else
			{
				if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_ERECT_DOMAIN))
					status = STATE_RUN_FAILED;
				else if (!mcs_send_erect_domain_request(rdp->mcs))
				{
					WLog_ERR(TAG, "mcs_send_erect_domain_request failure");
					status = STATE_RUN_FAILED;
				}
				else if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_ATTACH_USER))
					status = STATE_RUN_FAILED;
				else if (!mcs_send_attach_user_request(rdp->mcs))
				{
					WLog_ERR(TAG, "mcs_send_attach_user_request failure");
					status = STATE_RUN_FAILED;
				}
				else if (!rdp_client_transition_to_state(rdp,
				                                         CONNECTION_STATE_MCS_ATTACH_USER_CONFIRM))
					status = STATE_RUN_FAILED;
			}
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER_CONFIRM:
			if (!mcs_recv_attach_user_confirm(rdp->mcs, s))
			{
				WLog_ERR(TAG, "mcs_recv_attach_user_confirm failure");
				status = STATE_RUN_FAILED;
			}
			else if (!rdp_client_transition_to_state(rdp,
			                                         CONNECTION_STATE_MCS_CHANNEL_JOIN_REQUEST))
				status = STATE_RUN_FAILED;
			else if (!mcs_send_channel_join_request(rdp->mcs, rdp->mcs->userId))
			{
				WLog_ERR(TAG, "mcs_send_channel_join_request failure");
				status = STATE_RUN_FAILED;
			}
			else if (!rdp_client_transition_to_state(rdp,
			                                         CONNECTION_STATE_MCS_CHANNEL_JOIN_RESPONSE))
				status = STATE_RUN_FAILED;
			break;

		case CONNECTION_STATE_MCS_CHANNEL_JOIN_RESPONSE:
			if (!rdp_client_connect_mcs_channel_join_confirm(rdp, s))
			{
				WLog_ERR(TAG,
				         "%s: %s - "
				         "rdp_client_connect_mcs_channel_join_confirm() fail",
				         __FUNCTION__, rdp_get_state_string(rdp));
				status = STATE_RUN_FAILED;
			}

			break;

		case CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT_REQUEST:
			if (!rdp_client_connect_auto_detect(rdp, s))
			{
				if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_LICENSING))
					status = STATE_RUN_FAILED;
				else
					status = STATE_RUN_TRY_AGAIN;
			}
			break;

		case CONNECTION_STATE_LICENSING:
			status = rdp_client_connect_license(rdp, s);

			if (state_run_failed(status))
			{
				char buffer[64] = { 0 };
				WLog_DBG(TAG, "%s: %s - rdp_client_connect_license() - %s", __FUNCTION__,
				         rdp_get_state_string(rdp),
				         state_run_result_string(status, buffer, ARRAYSIZE(buffer)));
			}

			break;

		case CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_REQUEST:
			if (!rdp_client_connect_auto_detect(rdp, s))
			{
				rdp_client_transition_to_state(
				    rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE);
				status = STATE_RUN_TRY_AGAIN;
			}
			break;

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE:
			status = rdp_client_connect_demand_active(rdp, s);

			if (state_run_failed(status))
			{
				char buffer[64] = { 0 };
				WLog_DBG(TAG,
				         "%s: %s - "
				         "rdp_client_connect_demand_active() - %s",
				         __FUNCTION__, rdp_get_state_string(rdp),
				         state_run_result_string(status, buffer, ARRAYSIZE(buffer)));
			}
			else if (status != STATE_RUN_REDIRECT)
			{
				if (!rdp_client_transition_to_state(
				        rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE))
					status = STATE_RUN_FAILED;
				else
					status = STATE_RUN_CONTINUE;
			}
			break;

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE_MONITOR_LAYOUT:
			status = rdp_client_exchange_monitor_layout(rdp, s);
			break;

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE:
			status = rdp_client_connect_confirm_active(rdp, s);
			break;

		case CONNECTION_STATE_FINALIZATION_CLIENT_SYNC:
			status = rdp_handle_sc_flags(rdp, s, FINALIZE_SC_SYNCHRONIZE_PDU,
			                             CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE);
			break;
		case CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE:
			status = rdp_handle_sc_flags(rdp, s, FINALIZE_SC_CONTROL_COOPERATE_PDU,
			                             CONNECTION_STATE_FINALIZATION_CLIENT_GRANTED_CONTROL);
			break;
		case CONNECTION_STATE_FINALIZATION_CLIENT_GRANTED_CONTROL:
			status = rdp_handle_sc_flags(rdp, s, FINALIZE_SC_CONTROL_GRANTED_PDU,
			                             CONNECTION_STATE_FINALIZATION_CLIENT_FONT_MAP);
			break;
		case CONNECTION_STATE_FINALIZATION_CLIENT_FONT_MAP:
			status = rdp_handle_sc_flags(rdp, s, FINALIZE_SC_FONT_MAP_PDU, CONNECTION_STATE_ACTIVE);
			break;

		case CONNECTION_STATE_ACTIVE:
			status = rdp_recv_pdu(rdp, s);

			if (state_run_failed(status))
			{
				char buffer[64] = { 0 };
				WLog_DBG(TAG, "%s: %s - rdp_recv_pdu() - %s", __FUNCTION__,
				         rdp_get_state_string(rdp),
				         state_run_result_string(status, buffer, ARRAYSIZE(buffer)));
			}
			break;

		default:
			WLog_ERR(TAG, "%s: %s state %d", __FUNCTION__, rdp_get_state_string(rdp),
			         rdp_get_state(rdp));
			status = STATE_RUN_FAILED;
			break;
	}

	if (state_run_failed(status))
	{
		char buffer[64] = { 0 };
		WLog_ERR(TAG, "%s: %s status %s", __FUNCTION__, rdp_get_state_string(rdp),
		         state_run_result_string(status, buffer, ARRAYSIZE(buffer)));
	}
	return status;
}

state_run_t rdp_recv_callback(rdpTransport* transport, wStream* s, void* extra)
{
	char buffer[64] = { 0 };
	state_run_t rc = STATE_RUN_FAILED;
	const size_t start = Stream_GetPosition(s);
	const rdpContext* context = transport_get_context(transport);

	WINPR_ASSERT(context);
	do
	{
		const rdpRdp* rdp = context->rdp;

		if (rc == STATE_RUN_TRY_AGAIN)
			Stream_SetPosition(s, start);

		const char* old = rdp_get_state_string(rdp);
		const size_t orem = Stream_GetRemainingLength(s);
		rc = rdp_recv_callback_int(transport, s, extra);

		const char* now = rdp_get_state_string(rdp);
		const size_t rem = Stream_GetRemainingLength(s);

		WLog_VRB(TAG,
		         "(client)[%s -> %s] current return %s [feeding %" PRIuz " bytes, %" PRIuz
		         " bytes not processed]",
		         old, now, state_run_result_string(rc, buffer, sizeof(buffer)), orem, rem);
	} while ((rc == STATE_RUN_TRY_AGAIN) || (rc == STATE_RUN_CONTINUE));
	return rc;
}

BOOL rdp_send_channel_data(rdpRdp* rdp, UINT16 channelId, const BYTE* data, size_t size)
{
	return freerdp_channel_send(rdp, channelId, data, size);
}

BOOL rdp_channel_send_packet(rdpRdp* rdp, UINT16 channelId, size_t totalSize, UINT32 flags,
                             const BYTE* data, size_t chunkSize)
{
	return freerdp_channel_send_packet(rdp, channelId, totalSize, flags, data, chunkSize);
}

BOOL rdp_send_error_info(rdpRdp* rdp)
{
	wStream* s;
	BOOL status;

	if (rdp->errorInfo == ERRINFO_SUCCESS)
		return TRUE;

	s = rdp_data_pdu_init(rdp);

	if (!s)
		return FALSE;

	Stream_Write_UINT32(s, rdp->errorInfo); /* error id (4 bytes) */
	status = rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SET_ERROR_INFO, 0);
	return status;
}

int rdp_check_fds(rdpRdp* rdp)
{
	int status;
	rdpTsg* tsg;
	rdpTransport* transport;

	WINPR_ASSERT(rdp);
	transport = rdp->transport;

	tsg = transport_get_tsg(transport);
	if (tsg)
	{
		if (!tsg_check_event_handles(tsg))
		{
			WLog_ERR(TAG, "rdp_check_fds: tsg_check_event_handles()");
			return -1;
		}

		if (tsg_get_state(tsg) != TSG_STATE_PIPE_CREATED)
			return 1;
	}

	status = transport_check_fds(transport);

	if (status == 1)
	{
		if (!rdp_client_redirect(rdp)) /* session redirection */
			return -1;
	}

	if (status < 0)
		WLog_DBG(TAG, "transport_check_fds() - %i", status);

	return status;
}

BOOL freerdp_get_stats(rdpRdp* rdp, UINT64* inBytes, UINT64* outBytes, UINT64* inPackets,
                       UINT64* outPackets)
{
	if (!rdp)
		return FALSE;

	if (inBytes)
		*inBytes = rdp->inBytes;
	if (outBytes)
		*outBytes = rdp->outBytes;
	if (inPackets)
		*inPackets = rdp->inPackets;
	if (outPackets)
		*outPackets = rdp->outPackets;

	return TRUE;
}

/**
 * Instantiate new RDP module.
 * @return new RDP module
 */

rdpRdp* rdp_new(rdpContext* context)
{
	rdpRdp* rdp;
	DWORD flags;
	rdp = (rdpRdp*)calloc(1, sizeof(rdpRdp));

	if (!rdp)
		return NULL;

	InitializeCriticalSection(&rdp->critical);
	rdp->context = context;
	flags = 0;

	if (context->ServerMode)
		flags |= FREERDP_SETTINGS_SERVER_MODE;

	if (!context->settings)
	{
		context->settings = rdp->settings = freerdp_settings_new(flags);

		if (!rdp->settings)
			goto fail;
	}
	else
		rdp->settings = context->settings;

	/* Keep a backup copy of settings for later comparisons */
	freerdp_settings_free(rdp->originalSettings);
	rdp->originalSettings = freerdp_settings_clone(rdp->settings);
	if (!rdp->originalSettings)
		return FALSE;

	rdp->settings->instance = context->instance;

	context->settings = rdp->settings;
	if (context->instance)
		context->settings->instance = context->instance;
	else if (context->peer)
	{
		rdp->settings->instance = context->peer;

#if defined(WITH_FREERDP_DEPRECATED)
		context->peer->settings = rdp->settings;
#endif
	}

	rdp->transport = transport_new(context);

	if (!rdp->transport)
		goto fail;

	{
		const rdpTransportIo* io = transport_get_io_callbacks(rdp->transport);
		if (!io)
			goto fail;
		rdp->io = calloc(1, sizeof(rdpTransportIo));
		if (!rdp->io)
			goto fail;
		*rdp->io = *io;
	}

	rdp->license = license_new(rdp);

	if (!rdp->license)
		goto fail;

	rdp->input = input_new(rdp);

	if (!rdp->input)
		goto fail;

	rdp->update = update_new(rdp);

	if (!rdp->update)
		goto fail;

	rdp->fastpath = fastpath_new(rdp);

	if (!rdp->fastpath)
		goto fail;

	rdp->nego = nego_new(rdp->transport);

	if (!rdp->nego)
		goto fail;

	rdp->mcs = mcs_new(rdp->transport);

	if (!rdp->mcs)
		goto fail;

	rdp->redirection = redirection_new();

	if (!rdp->redirection)
		goto fail;

	rdp->autodetect = autodetect_new(rdp->context);

	if (!rdp->autodetect)
		goto fail;

	rdp->heartbeat = heartbeat_new();

	if (!rdp->heartbeat)
		goto fail;

	rdp->multitransport = multitransport_new(rdp, INITIATE_REQUEST_PROTOCOL_UDPFECL |
	                                                  INITIATE_REQUEST_PROTOCOL_UDPFECR);

	if (!rdp->multitransport)
		goto fail;

	rdp->bulk = bulk_new(context);

	if (!rdp->bulk)
		goto fail;

	rdp->pubSub = PubSub_New(TRUE);
	if (!rdp->pubSub)
		goto fail;

	rdp->abortEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!rdp->abortEvent)
		goto fail;
	return rdp;

fail:
	rdp_free(rdp);
	return NULL;
}

static void rdp_reset_free(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);

	rdp_free_rc4_decrypt_keys(rdp);
	rdp_free_rc4_encrypt_keys(rdp);

	winpr_Cipher_Free(rdp->fips_encrypt);
	winpr_Cipher_Free(rdp->fips_decrypt);
	rdp->fips_encrypt = NULL;
	rdp->fips_decrypt = NULL;

	mcs_free(rdp->mcs);
	nego_free(rdp->nego);
	license_free(rdp->license);
	transport_free(rdp->transport);
	fastpath_free(rdp->fastpath);

	rdp->mcs = NULL;
	rdp->nego = NULL;
	rdp->license = NULL;
	rdp->transport = NULL;
	rdp->fastpath = NULL;
}

BOOL rdp_reset(rdpRdp* rdp)
{
	BOOL rc = TRUE;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(rdp);

	context = rdp->context;
	WINPR_ASSERT(context);

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	bulk_reset(rdp->bulk);

	rdp_reset_free(rdp);

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerRandom, NULL, 0))
		rc = FALSE;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerCertificate, NULL, 0))
		rc = FALSE;

	if (!freerdp_settings_set_string(settings, FreeRDP_ClientAddress, NULL))
		rc = FALSE;

	if (!rc)
		goto fail;

	rc = FALSE;
	rdp->transport = transport_new(context);
	if (!rdp->transport)
		goto fail;

	if (rdp->io)
	{
		if (!transport_set_io_callbacks(rdp->transport, rdp->io))
			goto fail;
	}

	rdp->nego = nego_new(rdp->transport);
	if (!rdp->nego)
		goto fail;

	rdp->mcs = mcs_new(rdp->transport);
	if (!rdp->mcs)
		goto fail;

	if (!transport_set_layer(rdp->transport, TRANSPORT_LAYER_TCP))
		goto fail;

	rdp->license = license_new(rdp);
	if (!rdp->license)
		goto fail;

	rdp->fastpath = fastpath_new(rdp);
	if (!rdp->fastpath)
		goto fail;

	rdp->errorInfo = 0;
	rc = rdp_finalize_reset_flags(rdp, TRUE);

fail:
	return rc;
}

/**
 * Free RDP module.
 * @param rdp RDP module to be freed
 */

void rdp_free(rdpRdp* rdp)
{
	if (rdp)
	{
		DeleteCriticalSection(&rdp->critical);
		rdp_reset_free(rdp);

		freerdp_settings_free(rdp->settings);
		freerdp_settings_free(rdp->originalSettings);

		input_free(rdp->input);
		update_free(rdp->update);
		nla_free(rdp->nla);
		redirection_free(rdp->redirection);
		autodetect_free(rdp->autodetect);
		heartbeat_free(rdp->heartbeat);
		multitransport_free(rdp->multitransport);
		bulk_free(rdp->bulk);
		free(rdp->io);
		PubSub_Free(rdp->pubSub);
		if (rdp->abortEvent)
			CloseHandle(rdp->abortEvent);
		free(rdp);
	}
}

BOOL rdp_io_callback_set_event(rdpRdp* rdp, BOOL set)
{
	if (!rdp)
		return FALSE;
	return transport_io_callback_set_event(rdp->transport, set);
}

const rdpTransportIo* rdp_get_io_callbacks(rdpRdp* rdp)
{
	if (!rdp)
		return NULL;
	return rdp->io;
}

BOOL rdp_set_io_callbacks(rdpRdp* rdp, const rdpTransportIo* io_callbacks)
{
	if (!rdp)
		return FALSE;
	free(rdp->io);
	rdp->io = NULL;
	if (io_callbacks)
	{
		rdp->io = malloc(sizeof(rdpTransportIo));
		if (!rdp->io)
			return FALSE;
		*rdp->io = *io_callbacks;
		return transport_set_io_callbacks(rdp->transport, rdp->io);
	}
	return TRUE;
}

BOOL rdp_set_io_callback_context(rdpRdp* rdp, void* usercontext)
{
	WINPR_ASSERT(rdp);
	rdp->ioContext = usercontext;
	return TRUE;
}

void* rdp_get_io_callback_context(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	return rdp->ioContext;
}

const char* rdp_finalize_flags_to_str(UINT32 flags, char* buffer, size_t size)
{
	char number[32] = { 0 };
	const UINT32 mask = ~(FINALIZE_SC_SYNCHRONIZE_PDU | FINALIZE_SC_CONTROL_COOPERATE_PDU |
	                      FINALIZE_SC_CONTROL_GRANTED_PDU | FINALIZE_SC_FONT_MAP_PDU |
	                      FINALIZE_CS_SYNCHRONIZE_PDU | FINALIZE_CS_CONTROL_COOPERATE_PDU |
	                      FINALIZE_CS_CONTROL_REQUEST_PDU | FINALIZE_CS_PERSISTENT_KEY_LIST_PDU |
	                      FINALIZE_CS_FONT_LIST_PDU | FINALIZE_DEACTIVATE_REACTIVATE);

	if (flags & FINALIZE_SC_SYNCHRONIZE_PDU)
		winpr_str_append("FINALIZE_SC_SYNCHRONIZE_PDU", buffer, size, "|");
	if (flags & FINALIZE_SC_CONTROL_COOPERATE_PDU)
		winpr_str_append("FINALIZE_SC_CONTROL_COOPERATE_PDU", buffer, size, "|");
	if (flags & FINALIZE_SC_CONTROL_GRANTED_PDU)
		winpr_str_append("FINALIZE_SC_CONTROL_GRANTED_PDU", buffer, size, "|");
	if (flags & FINALIZE_SC_FONT_MAP_PDU)
		winpr_str_append("FINALIZE_SC_FONT_MAP_PDU", buffer, size, "|");
	if (flags & FINALIZE_CS_SYNCHRONIZE_PDU)
		winpr_str_append("FINALIZE_CS_SYNCHRONIZE_PDU", buffer, size, "|");
	if (flags & FINALIZE_CS_CONTROL_COOPERATE_PDU)
		winpr_str_append("FINALIZE_CS_CONTROL_COOPERATE_PDU", buffer, size, "|");
	if (flags & FINALIZE_CS_CONTROL_REQUEST_PDU)
		winpr_str_append("FINALIZE_CS_CONTROL_REQUEST_PDU", buffer, size, "|");
	if (flags & FINALIZE_CS_PERSISTENT_KEY_LIST_PDU)
		winpr_str_append("FINALIZE_CS_PERSISTENT_KEY_LIST_PDU", buffer, size, "|");
	if (flags & FINALIZE_CS_FONT_LIST_PDU)
		winpr_str_append("FINALIZE_CS_FONT_LIST_PDU", buffer, size, "|");
	if (flags & FINALIZE_DEACTIVATE_REACTIVATE)
		winpr_str_append("FINALIZE_DEACTIVATE_REACTIVATE", buffer, size, "|");
	if (flags & mask)
		winpr_str_append("UNKNOWN_FLAG", buffer, size, "|");
	if (flags == 0)
		winpr_str_append("NO_FLAG_SET", buffer, size, "|");
	_snprintf(number, sizeof(number), " [0x%04" PRIx16 "]", flags);
	winpr_str_append(number, buffer, size, "");
	return buffer;
}

BOOL rdp_finalize_reset_flags(rdpRdp* rdp, BOOL clearAll)
{
	WINPR_ASSERT(rdp);
	WLog_DBG(TAG, "[%s] reset finalize_sc_pdus", rdp_get_state_string(rdp));
	if (clearAll)
		rdp->finalize_sc_pdus = 0;
	else
		rdp->finalize_sc_pdus &= FINALIZE_DEACTIVATE_REACTIVATE;
	return TRUE;
}

BOOL rdp_finalize_set_flag(rdpRdp* rdp, UINT32 flag)
{
	char buffer[1024] = { 0 };

	WINPR_ASSERT(rdp);

	WLog_DBG(TAG, "[%s] received flag %s", rdp_get_state_string(rdp),
	         rdp_finalize_flags_to_str(flag, buffer, sizeof(buffer)));
	rdp->finalize_sc_pdus |= flag;
	return TRUE;
}

BOOL rdp_finalize_is_flag_set(rdpRdp* rdp, UINT32 flag)
{
	WINPR_ASSERT(rdp);
	return (rdp->finalize_sc_pdus & flag) == flag;
}

BOOL rdp_reset_rc4_encrypt_keys(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	rdp_free_rc4_encrypt_keys(rdp);
	rdp->rc4_encrypt_key = winpr_RC4_New(rdp->encrypt_key, rdp->rc4_key_len);

	rdp->encrypt_use_count = 0;
	return rdp->rc4_encrypt_key != NULL;
}

void rdp_free_rc4_encrypt_keys(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	winpr_RC4_Free(rdp->rc4_encrypt_key);
	rdp->rc4_encrypt_key = NULL;
}

void rdp_free_rc4_decrypt_keys(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	winpr_RC4_Free(rdp->rc4_decrypt_key);
	rdp->rc4_decrypt_key = NULL;
}

BOOL rdp_reset_rc4_decrypt_keys(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	rdp_free_rc4_decrypt_keys(rdp);
	rdp->rc4_decrypt_key = winpr_RC4_New(rdp->decrypt_key, rdp->rc4_key_len);

	rdp->decrypt_use_count = 0;
	return rdp->rc4_decrypt_key != NULL;
}

const char* rdp_security_flag_string(UINT32 securityFlags, char* buffer, size_t size)
{
	if (securityFlags & SEC_EXCHANGE_PKT)
		winpr_str_append("SEC_EXCHANGE_PKT", buffer, size, "|");
	if (securityFlags & SEC_TRANSPORT_REQ)
		winpr_str_append("SEC_TRANSPORT_REQ", buffer, size, "|");
	if (securityFlags & SEC_TRANSPORT_RSP)
		winpr_str_append("SEC_TRANSPORT_RSP", buffer, size, "|");
	if (securityFlags & SEC_ENCRYPT)
		winpr_str_append("SEC_ENCRYPT", buffer, size, "|");
	if (securityFlags & SEC_RESET_SEQNO)
		winpr_str_append("SEC_RESET_SEQNO", buffer, size, "|");
	if (securityFlags & SEC_IGNORE_SEQNO)
		winpr_str_append("SEC_IGNORE_SEQNO", buffer, size, "|");
	if (securityFlags & SEC_INFO_PKT)
		winpr_str_append("SEC_INFO_PKT", buffer, size, "|");
	if (securityFlags & SEC_LICENSE_PKT)
		winpr_str_append("SEC_LICENSE_PKT", buffer, size, "|");
	if (securityFlags & SEC_LICENSE_ENCRYPT_CS)
		winpr_str_append("SEC_LICENSE_ENCRYPT_CS", buffer, size, "|");
	if (securityFlags & SEC_LICENSE_ENCRYPT_SC)
		winpr_str_append("SEC_LICENSE_ENCRYPT_SC", buffer, size, "|");
	if (securityFlags & SEC_REDIRECTION_PKT)
		winpr_str_append("SEC_REDIRECTION_PKT", buffer, size, "|");
	if (securityFlags & SEC_SECURE_CHECKSUM)
		winpr_str_append("SEC_SECURE_CHECKSUM", buffer, size, "|");
	if (securityFlags & SEC_AUTODETECT_REQ)
		winpr_str_append("SEC_AUTODETECT_REQ", buffer, size, "|");
	if (securityFlags & SEC_AUTODETECT_RSP)
		winpr_str_append("SEC_AUTODETECT_RSP", buffer, size, "|");
	if (securityFlags & SEC_HEARTBEAT)
		winpr_str_append("SEC_HEARTBEAT", buffer, size, "|");
	if (securityFlags & SEC_FLAGSHI_VALID)
		winpr_str_append("SEC_FLAGSHI_VALID", buffer, size, "|");

	return buffer;
}
