/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2016 Jiang Zihao <zihao.jiang@yahoo.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/log.h>

#include "rdpgfx_common.h"
#include "rdpgfx_main.h"

#define TAG CHANNELS_TAG("rdpgfx.server")
#define RDPGFX_RESET_GRAPHICS_PDU_SIZE 340

#define checkCapsAreExchanged(context) \
	checkCapsAreExchangedInt(context, __FILE__, __func__, __LINE__)
static BOOL checkCapsAreExchangedInt(RdpgfxServerContext* context, const char* file,
                                     const char* fkt, size_t line)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	const DWORD level = WLOG_TRACE;
	if (WLog_IsLevelActive(context->priv->log, level))
	{
		WLog_PrintMessage(context->priv->log, WLOG_MESSAGE_TEXT, level, line, file, fkt,
		                  "activeCapSet{Version=0x%08" PRIx32 ", flags=0x%08" PRIx32 "}",
		                  context->priv->activeCapSet.version, context->priv->activeCapSet.flags);
	}
	return context->priv->activeCapSet.version > 0;
}

/**
 * Function description
 * Calculate packet size from data length.
 * It would be data length + header.
 *
 * @param dataLen estimated data length without header
 *
 * @return new stream
 */
static INLINE UINT32 rdpgfx_pdu_length(UINT32 dataLen)
{
	return RDPGFX_HEADER_SIZE + dataLen;
}

static INLINE UINT rdpgfx_server_packet_init_header(wStream* s, UINT16 cmdId, UINT32 pduLength)
{
	RDPGFX_HEADER header;
	header.flags = 0;
	header.cmdId = cmdId;
	header.pduLength = pduLength;
	/* Write header. Note that actual length might be changed
	 * after the entire packet has been constructed. */
	return rdpgfx_write_header(s, &header);
}

/**
 * Function description
 * Complete the rdpgfx packet header.
 *
 * @param s stream
 * @param start saved start pos of the packet in the stream
 */
static INLINE BOOL rdpgfx_server_packet_complete_header(wStream* s, size_t start)
{
	const size_t current = Stream_GetPosition(s);
	const size_t cap = Stream_Capacity(s);
	if (cap < start + RDPGFX_HEADER_SIZE)
		return FALSE;
	if ((start > UINT32_MAX) || (current < start))
		return FALSE;
	/* Fill actual length */
	Stream_SetPosition(s, start + RDPGFX_HEADER_SIZE - sizeof(UINT32));
	Stream_Write_UINT32(s, (UINT32)(current - start)); /* pduLength (4 bytes) */
	Stream_SetPosition(s, current);
	return TRUE;
}

/**
 * Function description
 * Send the stream for rdpgfx server packet.
 * The packet would be compressed according to [MS-RDPEGFX].
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_server_packet_send(RdpgfxServerContext* context, wStream* s)
{
	UINT error = 0;
	UINT32 flags = 0;
	ULONG written = 0;
	BYTE* pSrcData = Stream_Buffer(s);
	const size_t SrcSize = Stream_GetPosition(s);
	if (SrcSize > UINT32_MAX)
		return ERROR_INTERNAL_ERROR;

	wStream* fs = NULL;
	/* Allocate new stream with enough capacity. Additional overhead is
	 * descriptor (1 bytes) + segmentCount (2 bytes) + uncompressedSize (4 bytes)
	 * + segmentCount * size (4 bytes) */
	fs = Stream_New(NULL, SrcSize + 7 + (SrcSize / ZGFX_SEGMENTED_MAXSIZE + 1) * 4);

	if (!fs)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	if (zgfx_compress_to_stream(context->priv->zgfx, fs, pSrcData, (UINT32)SrcSize, &flags) < 0)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "zgfx_compress_to_stream failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	const size_t pos = Stream_GetPosition(fs);

	WINPR_ASSERT(pos <= UINT32_MAX);
	if (!WTSVirtualChannelWrite(context->priv->rdpgfx_channel, Stream_BufferAs(fs, char),
	                            (UINT32)pos, &written))
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "WTSVirtualChannelWrite failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	if (written < Stream_GetPosition(fs))
	{
		WLog_Print(context->priv->log, WLOG_WARN,
		           "Unexpected bytes written: %" PRIu32 "/%" PRIuz "", written,
		           Stream_GetPosition(fs));
	}

	error = CHANNEL_RC_OK;
out:
	Stream_Free(fs, TRUE);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 * Create new stream for single rdpgfx packet. The new stream length
 * would be required data length + header. The header will be written
 * to the stream before return, but the pduLength field might be
 * changed in rdpgfx_server_single_packet_send.
 *
 * @param cmdId The CommandID to write
 * @param dataLen estimated data length without header
 *
 * @return new stream
 */
static wStream* rdpgfx_server_single_packet_new(wLog* log, UINT16 cmdId, UINT32 dataLen)
{
	UINT error = 0;
	wStream* s = NULL;
	UINT32 pduLength = rdpgfx_pdu_length(dataLen);
	s = Stream_New(NULL, pduLength);

	if (!s)
	{
		WLog_Print(log, WLOG_ERROR, "Stream_New failed!");
		goto error;
	}

	if ((error = rdpgfx_server_packet_init_header(s, cmdId, pduLength)))
	{
		WLog_Print(log, WLOG_ERROR, "Failed to init header with error %" PRIu32 "!", error);
		goto error;
	}

	return s;
error:
	Stream_Free(s, TRUE);
	return NULL;
}

/**
 * Function description
 * Send the stream for single rdpgfx packet.
 * The header will be filled with actual length.
 * The packet would be compressed according to [MS-RDPEGFX].
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static INLINE UINT rdpgfx_server_single_packet_send(RdpgfxServerContext* context, wStream* s)
{
	/* Fill actual length */
	rdpgfx_server_packet_complete_header(s, 0);
	return rdpgfx_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_caps_confirm_pdu(RdpgfxServerContext* context,
                                         const RDPGFX_CAPS_CONFIRM_PDU* capsConfirm)
{
	wStream* s = NULL;
	RDPGFX_CAPSET* capsSet = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(capsConfirm);

	capsSet = capsConfirm->capsSet;
	WINPR_ASSERT(capsSet);

	s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_CAPSCONFIRM,
	                                    RDPGFX_CAPSET_BASE_SIZE + capsSet->length);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	WLog_DBG(TAG, "CAPS version=0x%04" PRIx32 ", flags=0x%04" PRIx32 ", length=%" PRIu32,
	         capsSet->version, capsSet->flags, capsSet->length);
	Stream_Write_UINT32(s, capsSet->version); /* version (4 bytes) */
	Stream_Write_UINT32(s, capsSet->length);  /* capsDataLength (4 bytes) */

	if (capsSet->length >= 4)
	{
		Stream_Write_UINT32(s, capsSet->flags); /* capsData (4 bytes) */
		Stream_Zero(s, capsSet->length - 4);
	}
	else
		Stream_Zero(s, capsSet->length);

	context->priv->activeCapSet = *capsSet;
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_reset_graphics_pdu(RdpgfxServerContext* context,
                                           const RDPGFX_RESET_GRAPHICS_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;

	wStream* s = NULL;

	/* Check monitorCount. This ensures total size within 340 bytes) */
	if (pdu->monitorCount >= 16)
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "Monitor count MUST be less than or equal to 16: %" PRIu32 "",
		           pdu->monitorCount);
		return ERROR_INVALID_DATA;
	}

	s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_RESETGRAPHICS,
	                                    RDPGFX_RESET_GRAPHICS_PDU_SIZE - RDPGFX_HEADER_SIZE);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, pdu->width);        /* width (4 bytes) */
	Stream_Write_UINT32(s, pdu->height);       /* height (4 bytes) */
	Stream_Write_UINT32(s, pdu->monitorCount); /* monitorCount (4 bytes) */

	for (UINT32 index = 0; index < pdu->monitorCount; index++)
	{
		const MONITOR_DEF* monitor = &(pdu->monitorDefArray[index]);
		Stream_Write_INT32(s, monitor->left);    /* left (4 bytes) */
		Stream_Write_INT32(s, monitor->top);     /* top (4 bytes) */
		Stream_Write_INT32(s, monitor->right);   /* right (4 bytes) */
		Stream_Write_INT32(s, monitor->bottom);  /* bottom (4 bytes) */
		Stream_Write_UINT32(s, monitor->flags);  /* flags (4 bytes) */
	}

	/* pad (total size must be 340 bytes) */
	Stream_SetPosition(s, RDPGFX_RESET_GRAPHICS_PDU_SIZE);
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_evict_cache_entry_pdu(RdpgfxServerContext* context,
                                              const RDPGFX_EVICT_CACHE_ENTRY_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s =
	    rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_EVICTCACHEENTRY, 2);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->cacheSlot); /* cacheSlot (2 bytes) */
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_cache_import_reply_pdu(RdpgfxServerContext* context,
                                               const RDPGFX_CACHE_IMPORT_REPLY_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	WINPR_ASSERT(context);
	WINPR_ASSERT(pdu);

	WLog_DBG(TAG, "reply with %" PRIu16 " entries", pdu->importedEntriesCount);
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_CACHEIMPORTREPLY,
	                                             2 + 2 * pdu->importedEntriesCount);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	/* importedEntriesCount (2 bytes) */
	Stream_Write_UINT16(s, pdu->importedEntriesCount);

	for (UINT16 index = 0; index < pdu->importedEntriesCount; index++)
	{
		Stream_Write_UINT16(s, pdu->cacheSlots[index]); /* cacheSlot (2 bytes) */
	}

	return rdpgfx_server_single_packet_send(context, s);
}

static UINT
rdpgfx_process_cache_import_offer_pdu(RdpgfxServerContext* context,
                                      const RDPGFX_CACHE_IMPORT_OFFER_PDU* cacheImportOffer)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	WINPR_ASSERT(context);
	WINPR_ASSERT(cacheImportOffer);

	RDPGFX_CACHE_IMPORT_REPLY_PDU reply = { 0 };
	WLog_DBG(TAG, "received %" PRIu16 " entries, reply with %" PRIu16 " entries",
	         cacheImportOffer->cacheEntriesCount, reply.importedEntriesCount);
	return IFCALLRESULT(CHANNEL_RC_OK, context->CacheImportReply, context, &reply);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_create_surface_pdu(RdpgfxServerContext* context,
                                           const RDPGFX_CREATE_SURFACE_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_CREATESURFACE, 7);

	WINPR_ASSERT(context);
	WINPR_ASSERT(pdu);
	WINPR_ASSERT((pdu->pixelFormat == GFX_PIXEL_FORMAT_XRGB_8888) ||
	             (pdu->pixelFormat == GFX_PIXEL_FORMAT_ARGB_8888));

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId);  /* surfaceId (2 bytes) */
	Stream_Write_UINT16(s, pdu->width);      /* width (2 bytes) */
	Stream_Write_UINT16(s, pdu->height);     /* height (2 bytes) */
	Stream_Write_UINT8(s, pdu->pixelFormat); /* RDPGFX_PIXELFORMAT (1 byte) */
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_delete_surface_pdu(RdpgfxServerContext* context,
                                           const RDPGFX_DELETE_SURFACE_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_DELETESURFACE, 2);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId); /* surfaceId (2 bytes) */
	return rdpgfx_server_single_packet_send(context, s);
}

static INLINE BOOL rdpgfx_write_start_frame_pdu(wStream* s, const RDPGFX_START_FRAME_PDU* pdu)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;
	Stream_Write_UINT32(s, pdu->timestamp); /* timestamp (4 bytes) */
	Stream_Write_UINT32(s, pdu->frameId);   /* frameId (4 bytes) */
	return TRUE;
}

static INLINE BOOL rdpgfx_write_end_frame_pdu(wStream* s, const RDPGFX_END_FRAME_PDU* pdu)
{
	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;
	Stream_Write_UINT32(s, pdu->frameId); /* frameId (4 bytes) */
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_start_frame_pdu(RdpgfxServerContext* context,
                                        const RDPGFX_START_FRAME_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_STARTFRAME,
	                                             RDPGFX_START_FRAME_PDU_SIZE);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpgfx_write_start_frame_pdu(s, pdu);
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_end_frame_pdu(RdpgfxServerContext* context, const RDPGFX_END_FRAME_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_ENDFRAME,
	                                             RDPGFX_END_FRAME_PDU_SIZE);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpgfx_write_end_frame_pdu(s, pdu);
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 * Estimate RFX_AVC420_BITMAP_STREAM structure size in stream
 *
 * @return estimated size
 */
static INLINE UINT32 rdpgfx_estimate_h264_avc420(const RDPGFX_AVC420_BITMAP_STREAM* havc420)
{
	/* H264 metadata + H264 stream. See rdpgfx_write_h264_avc420 */
	return sizeof(UINT32) /* numRegionRects */
	       + 10ULL        /* regionRects + quantQualityVals */
	             * havc420->meta.numRegionRects +
	       havc420->length;
}

/**
 * Function description
 * Estimate surface command packet size in stream without header
 *
 * @return estimated size
 */
static INLINE UINT32 rdpgfx_estimate_surface_command(const RDPGFX_SURFACE_COMMAND* cmd)
{
	RDPGFX_AVC420_BITMAP_STREAM* havc420 = NULL;
	RDPGFX_AVC444_BITMAP_STREAM* havc444 = NULL;
	UINT32 h264Size = 0;

	/* Estimate stream size according to codec. */
	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_CAPROGRESSIVE:
		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			return RDPGFX_WIRE_TO_SURFACE_PDU_2_SIZE + cmd->length;

		case RDPGFX_CODECID_AVC420:
			havc420 = (RDPGFX_AVC420_BITMAP_STREAM*)cmd->extra;
			h264Size = rdpgfx_estimate_h264_avc420(havc420);
			return RDPGFX_WIRE_TO_SURFACE_PDU_1_SIZE + h264Size;

		case RDPGFX_CODECID_AVC444:
			havc444 = (RDPGFX_AVC444_BITMAP_STREAM*)cmd->extra;
			h264Size = sizeof(UINT32); /* cbAvc420EncodedBitstream1 */
			/* avc420EncodedBitstream1 */
			havc420 = &(havc444->bitstream[0]);
			h264Size += rdpgfx_estimate_h264_avc420(havc420);

			/* avc420EncodedBitstream2 */
			if (havc444->LC == 0)
			{
				havc420 = &(havc444->bitstream[1]);
				h264Size += rdpgfx_estimate_h264_avc420(havc420);
			}

			return RDPGFX_WIRE_TO_SURFACE_PDU_1_SIZE + h264Size;

		default:
			return RDPGFX_WIRE_TO_SURFACE_PDU_1_SIZE + cmd->length;
	}
}

/**
 * Function description
 * Resolve RDPGFX_CMDID_WIRETOSURFACE_1 or RDPGFX_CMDID_WIRETOSURFACE_2
 * according to codecId
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static INLINE UINT16 rdpgfx_surface_command_cmdid(const RDPGFX_SURFACE_COMMAND* cmd)
{
	if (cmd->codecId == RDPGFX_CODECID_CAPROGRESSIVE ||
	    cmd->codecId == RDPGFX_CODECID_CAPROGRESSIVE_V2)
	{
		return RDPGFX_CMDID_WIRETOSURFACE_2;
	}

	return RDPGFX_CMDID_WIRETOSURFACE_1;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_write_h264_metablock(wLog* log, wStream* s, const RDPGFX_H264_METABLOCK* meta)
{
	RECTANGLE_16* regionRect = NULL;
	RDPGFX_H264_QUANT_QUALITY* quantQualityVal = NULL;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_EnsureRemainingCapacity(s, 4 + meta->numRegionRects * 10))
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT32(s, meta->numRegionRects); /* numRegionRects (4 bytes) */

	for (UINT32 index = 0; index < meta->numRegionRects; index++)
	{
		regionRect = &(meta->regionRects[index]);

		if ((error = rdpgfx_write_rect16(s, regionRect)))
		{
			WLog_Print(log, WLOG_ERROR, "rdpgfx_write_rect16 failed with error %" PRIu32 "!",
			           error);
			return error;
		}
	}

	for (UINT32 index = 0; index < meta->numRegionRects; index++)
	{
		quantQualityVal = &(meta->quantQualityVals[index]);
		Stream_Write_UINT8(s, WINPR_ASSERTING_INT_CAST(
		                          uint8_t, quantQualityVal->qp | (quantQualityVal->r << 6) |
		                                       (quantQualityVal->p << 7))); /* qpVal (1 byte) */
		/* qualityVal (1 byte) */
		Stream_Write_UINT8(s, quantQualityVal->qualityVal);
	}

	return error;
}

/**
 * Function description
 * Write RFX_AVC420_BITMAP_STREAM structure to stream
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static INLINE UINT rdpgfx_write_h264_avc420(wLog* log, wStream* s,
                                            RDPGFX_AVC420_BITMAP_STREAM* havc420)
{
	UINT error = CHANNEL_RC_OK;

	if ((error = rdpgfx_write_h264_metablock(log, s, &(havc420->meta))))
	{
		WLog_Print(log, WLOG_ERROR, "rdpgfx_write_h264_metablock failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	if (!Stream_EnsureRemainingCapacity(s, havc420->length))
		return ERROR_OUTOFMEMORY;

	Stream_Write(s, havc420->data, havc420->length);
	return error;
}

/**
 * Function description
 * Write RDPGFX_CMDID_WIRETOSURFACE_1 or RDPGFX_CMDID_WIRETOSURFACE_2
 * to the stream according to RDPGFX_SURFACE_COMMAND message
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_write_surface_command(wLog* log, wStream* s, const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_AVC420_BITMAP_STREAM* havc420 = NULL;
	RDPGFX_AVC444_BITMAP_STREAM* havc444 = NULL;
	UINT8 pixelFormat = 0;

	switch (cmd->format)
	{
		case PIXEL_FORMAT_BGRX32:
			pixelFormat = GFX_PIXEL_FORMAT_XRGB_8888;
			break;

		case PIXEL_FORMAT_BGRA32:
			pixelFormat = GFX_PIXEL_FORMAT_ARGB_8888;
			break;

		default:
			WLog_Print(log, WLOG_ERROR, "Format %s not supported!",
			           FreeRDPGetColorFormatName(cmd->format));
			return ERROR_INVALID_DATA;
	}

	if (cmd->codecId == RDPGFX_CODECID_CAPROGRESSIVE ||
	    cmd->codecId == RDPGFX_CODECID_CAPROGRESSIVE_V2)
	{
		if (!Stream_EnsureRemainingCapacity(s, 13 + cmd->length))
			return ERROR_INTERNAL_ERROR;
		/* Write RDPGFX_CMDID_WIRETOSURFACE_2 format for CAPROGRESSIVE */
		Stream_Write_UINT16(
		    s, WINPR_ASSERTING_INT_CAST(uint16_t, cmd->surfaceId)); /* surfaceId (2 bytes) */
		Stream_Write_UINT16(
		    s, WINPR_ASSERTING_INT_CAST(uint16_t, cmd->codecId)); /* codecId (2 bytes) */
		Stream_Write_UINT32(s, cmd->contextId); /* codecContextId (4 bytes) */
		Stream_Write_UINT8(s, pixelFormat);     /* pixelFormat (1 byte) */
		Stream_Write_UINT32(s, cmd->length);    /* bitmapDataLength (4 bytes) */
		Stream_Write(s, cmd->data, cmd->length);
	}
	else
	{
		/* Write RDPGFX_CMDID_WIRETOSURFACE_1 format for others */
		if (!Stream_EnsureRemainingCapacity(s, 17))
			return ERROR_INTERNAL_ERROR;
		Stream_Write_UINT16(
		    s, WINPR_ASSERTING_INT_CAST(uint16_t, cmd->surfaceId)); /* surfaceId (2 bytes) */
		Stream_Write_UINT16(
		    s, WINPR_ASSERTING_INT_CAST(uint16_t, cmd->codecId)); /* codecId (2 bytes) */
		Stream_Write_UINT8(s, pixelFormat);     /* pixelFormat (1 byte) */
		Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(uint16_t, cmd->left)); /* left (2 bytes) */
		Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(uint16_t, cmd->top));  /* top (2 bytes) */
		Stream_Write_UINT16(s,
		                    WINPR_ASSERTING_INT_CAST(uint16_t, cmd->right)); /* right (2 bytes) */
		Stream_Write_UINT16(s,
		                    WINPR_ASSERTING_INT_CAST(uint16_t, cmd->bottom)); /* bottom (2 bytes) */
		Stream_Write_UINT32(s, cmd->length);    /* bitmapDataLength (4 bytes) */
		const size_t bitmapDataStart = Stream_GetPosition(s);

		if (cmd->codecId == RDPGFX_CODECID_AVC420)
		{
			havc420 = (RDPGFX_AVC420_BITMAP_STREAM*)cmd->extra;
			error = rdpgfx_write_h264_avc420(log, s, havc420);

			if (error != CHANNEL_RC_OK)
			{
				WLog_Print(log, WLOG_ERROR, "rdpgfx_write_h264_avc420 failed!");
				return error;
			}
		}
		else if ((cmd->codecId == RDPGFX_CODECID_AVC444) ||
		         (cmd->codecId == RDPGFX_CODECID_AVC444v2))
		{
			havc444 = (RDPGFX_AVC444_BITMAP_STREAM*)cmd->extra;
			havc420 = &(havc444->bitstream[0]); /* avc420EncodedBitstreamInfo (4 bytes) */
			if (!Stream_EnsureRemainingCapacity(s, 4))
				return ERROR_INTERNAL_ERROR;
			Stream_Write_UINT32(s, havc444->cbAvc420EncodedBitstream1 |
			                           ((uint32_t)havc444->LC << 30UL));
			/* avc420EncodedBitstream1 */
			error = rdpgfx_write_h264_avc420(log, s, havc420);

			if (error != CHANNEL_RC_OK)
			{
				WLog_Print(log, WLOG_ERROR, "rdpgfx_write_h264_avc420 failed!");
				return error;
			}

			/* avc420EncodedBitstream2 */
			if (havc444->LC == 0)
			{
				havc420 = &(havc444->bitstream[1]);
				error = rdpgfx_write_h264_avc420(log, s, havc420);

				if (error != CHANNEL_RC_OK)
				{
					WLog_Print(log, WLOG_ERROR, "rdpgfx_write_h264_avc420 failed!");
					return error;
				}
			}
		}
		else
		{
			if (!Stream_EnsureRemainingCapacity(s, cmd->length))
				return ERROR_INTERNAL_ERROR;
			Stream_Write(s, cmd->data, cmd->length);
		}

		/* Fill actual bitmap data length */
		const size_t bitmapDataLength = Stream_GetPosition(s) - bitmapDataStart;
		if (bitmapDataLength > UINT32_MAX)
			return ERROR_INTERNAL_ERROR;

		Stream_SetPosition(s, bitmapDataStart - sizeof(UINT32));
		if (!Stream_EnsureRemainingCapacity(s, 4))
			return ERROR_INTERNAL_ERROR;
		Stream_Write_UINT32(s, (UINT32)bitmapDataLength); /* bitmapDataLength (4 bytes) */
		if (!Stream_SafeSeek(s, bitmapDataLength))
			return ERROR_INTERNAL_ERROR;
	}

	return error;
}

/**
 * Function description
 * Send RDPGFX_CMDID_WIRETOSURFACE_1 or RDPGFX_CMDID_WIRETOSURFACE_2
 * message according to codecId
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_surface_command(RdpgfxServerContext* context,
                                        const RDPGFX_SURFACE_COMMAND* cmd)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	UINT error = CHANNEL_RC_OK;
	wStream* s = NULL;
	s = rdpgfx_server_single_packet_new(context->priv->log, rdpgfx_surface_command_cmdid(cmd),
	                                    rdpgfx_estimate_surface_command(cmd));

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rdpgfx_write_surface_command(context->priv->log, s, cmd);

	if (error != CHANNEL_RC_OK)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_write_surface_command failed!");
		goto error;
	}

	return rdpgfx_server_single_packet_send(context, s);
error:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 * Send RDPGFX_CMDID_WIRETOSURFACE_1 or RDPGFX_CMDID_WIRETOSURFACE_2
 * message according to codecId.
 * Prepend/append start/end frame message in same packet if exists.
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_surface_frame_command(RdpgfxServerContext* context,
                                              const RDPGFX_SURFACE_COMMAND* cmd,
                                              const RDPGFX_START_FRAME_PDU* startFrame,
                                              const RDPGFX_END_FRAME_PDU* endFrame)

{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	UINT error = CHANNEL_RC_OK;
	UINT32 size = rdpgfx_pdu_length(rdpgfx_estimate_surface_command(cmd));

	if (startFrame)
	{
		size += rdpgfx_pdu_length(RDPGFX_START_FRAME_PDU_SIZE);
	}

	if (endFrame)
	{
		size += rdpgfx_pdu_length(RDPGFX_END_FRAME_PDU_SIZE);
	}

	wStream* s = Stream_New(NULL, size);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	/* Write start frame if exists */
	if (startFrame)
	{
		const size_t position = Stream_GetPosition(s);
		error = rdpgfx_server_packet_init_header(s, RDPGFX_CMDID_STARTFRAME, 0);

		if (error != CHANNEL_RC_OK)
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "Failed to init header with error %" PRIu32 "!", error);
			goto error;
		}

		if (!rdpgfx_write_start_frame_pdu(s, startFrame) ||
		    !rdpgfx_server_packet_complete_header(s, position))
			goto error;
	}

	/* Write RDPGFX_CMDID_WIRETOSURFACE_1 or RDPGFX_CMDID_WIRETOSURFACE_2 */
	const size_t pos = Stream_GetPosition(s);
	error = rdpgfx_server_packet_init_header(s, rdpgfx_surface_command_cmdid(cmd),
	                                         0); // Actual length will be filled later

	if (error != CHANNEL_RC_OK)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Failed to init header with error %" PRIu32 "!",
		           error);
		goto error;
	}

	error = rdpgfx_write_surface_command(context->priv->log, s, cmd);

	if (error != CHANNEL_RC_OK)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_write_surface_command failed!");
		goto error;
	}

	if (!rdpgfx_server_packet_complete_header(s, pos))
		goto error;

	/* Write end frame if exists */
	if (endFrame)
	{
		const size_t position = Stream_GetPosition(s);
		error = rdpgfx_server_packet_init_header(s, RDPGFX_CMDID_ENDFRAME, 0);

		if (error != CHANNEL_RC_OK)
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "Failed to init header with error %" PRIu32 "!", error);
			goto error;
		}

		if (!rdpgfx_write_end_frame_pdu(s, endFrame) ||
		    !rdpgfx_server_packet_complete_header(s, position))
			goto error;
	}

	return rdpgfx_server_packet_send(context, s);
error:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_delete_encoding_context_pdu(RdpgfxServerContext* context,
                                                    const RDPGFX_DELETE_ENCODING_CONTEXT_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s =
	    rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_DELETEENCODINGCONTEXT, 6);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId);      /* surfaceId (2 bytes) */
	Stream_Write_UINT32(s, pdu->codecContextId); /* codecContextId (4 bytes) */
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_solid_fill_pdu(RdpgfxServerContext* context,
                                       const RDPGFX_SOLID_FILL_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	UINT error = CHANNEL_RC_OK;
	RECTANGLE_16* fillRect = NULL;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_SOLIDFILL,
	                                             8 + 8 * pdu->fillRectCount);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId); /* surfaceId (2 bytes) */

	/* fillPixel (4 bytes) */
	if ((error = rdpgfx_write_color32(s, &(pdu->fillPixel))))
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "rdpgfx_write_color32 failed with error %" PRIu32 "!", error);
		goto error;
	}

	Stream_Write_UINT16(s, pdu->fillRectCount); /* fillRectCount (2 bytes) */

	for (UINT16 index = 0; index < pdu->fillRectCount; index++)
	{
		fillRect = &(pdu->fillRects[index]);

		if ((error = rdpgfx_write_rect16(s, fillRect)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpgfx_write_rect16 failed with error %" PRIu32 "!", error);
			goto error;
		}
	}

	return rdpgfx_server_single_packet_send(context, s);
error:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_surface_to_surface_pdu(RdpgfxServerContext* context,
                                               const RDPGFX_SURFACE_TO_SURFACE_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	UINT error = CHANNEL_RC_OK;
	RDPGFX_POINT16* destPt = NULL;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_SURFACETOSURFACE,
	                                             14 + 4 * pdu->destPtsCount);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceIdSrc);  /* surfaceIdSrc (2 bytes) */
	Stream_Write_UINT16(s, pdu->surfaceIdDest); /* surfaceIdDest (2 bytes) */

	/* rectSrc (8 bytes ) */
	if ((error = rdpgfx_write_rect16(s, &(pdu->rectSrc))))
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "rdpgfx_write_rect16 failed with error %" PRIu32 "!", error);
		goto error;
	}

	Stream_Write_UINT16(s, pdu->destPtsCount); /* destPtsCount (2 bytes) */

	for (UINT16 index = 0; index < pdu->destPtsCount; index++)
	{
		destPt = &(pdu->destPts[index]);

		if ((error = rdpgfx_write_point16(s, destPt)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpgfx_write_point16 failed with error %" PRIu32 "!", error);
			goto error;
		}
	}

	return rdpgfx_server_single_packet_send(context, s);
error:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_surface_to_cache_pdu(RdpgfxServerContext* context,
                                             const RDPGFX_SURFACE_TO_CACHE_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	UINT error = CHANNEL_RC_OK;
	wStream* s =
	    rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_SURFACETOCACHE, 20);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId); /* surfaceId (2 bytes) */
	Stream_Write_UINT64(s, pdu->cacheKey);  /* cacheKey (8 bytes) */
	Stream_Write_UINT16(s, pdu->cacheSlot); /* cacheSlot (2 bytes) */

	/* rectSrc (8 bytes ) */
	if ((error = rdpgfx_write_rect16(s, &(pdu->rectSrc))))
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "rdpgfx_write_rect16 failed with error %" PRIu32 "!", error);
		goto error;
	}

	return rdpgfx_server_single_packet_send(context, s);
error:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_cache_to_surface_pdu(RdpgfxServerContext* context,
                                             const RDPGFX_CACHE_TO_SURFACE_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	UINT error = CHANNEL_RC_OK;
	RDPGFX_POINT16* destPt = NULL;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_CACHETOSURFACE,
	                                             6 + 4 * pdu->destPtsCount);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->cacheSlot);    /* cacheSlot (2 bytes) */
	Stream_Write_UINT16(s, pdu->surfaceId);    /* surfaceId (2 bytes) */
	Stream_Write_UINT16(s, pdu->destPtsCount); /* destPtsCount (2 bytes) */

	for (UINT16 index = 0; index < pdu->destPtsCount; index++)
	{
		destPt = &(pdu->destPts[index]);

		if ((error = rdpgfx_write_point16(s, destPt)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpgfx_write_point16 failed with error %" PRIu32 "", error);
			goto error;
		}
	}

	return rdpgfx_server_single_packet_send(context, s);
error:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_map_surface_to_output_pdu(RdpgfxServerContext* context,
                                                  const RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s =
	    rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_MAPSURFACETOOUTPUT, 12);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId);     /* surfaceId (2 bytes) */
	Stream_Write_UINT16(s, 0);                  /* reserved (2 bytes). Must be 0 */
	Stream_Write_UINT32(s, pdu->outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Write_UINT32(s, pdu->outputOriginY); /* outputOriginY (4 bytes) */
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_map_surface_to_window_pdu(RdpgfxServerContext* context,
                                                  const RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s =
	    rdpgfx_server_single_packet_new(context->priv->log, RDPGFX_CMDID_MAPSURFACETOWINDOW, 18);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId);    /* surfaceId (2 bytes) */
	Stream_Write_UINT64(s, pdu->windowId);     /* windowId (8 bytes) */
	Stream_Write_UINT32(s, pdu->mappedWidth);  /* mappedWidth (4 bytes) */
	Stream_Write_UINT32(s, pdu->mappedHeight); /* mappedHeight (4 bytes) */
	return rdpgfx_server_single_packet_send(context, s);
}

static UINT
rdpgfx_send_map_surface_to_scaled_window_pdu(RdpgfxServerContext* context,
                                             const RDPGFX_MAP_SURFACE_TO_SCALED_WINDOW_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log,
	                                             RDPGFX_CMDID_MAPSURFACETOSCALEDWINDOW, 26);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId);    /* surfaceId (2 bytes) */
	Stream_Write_UINT64(s, pdu->windowId);     /* windowId (8 bytes) */
	Stream_Write_UINT32(s, pdu->mappedWidth);  /* mappedWidth (4 bytes) */
	Stream_Write_UINT32(s, pdu->mappedHeight); /* mappedHeight (4 bytes) */
	Stream_Write_UINT32(s, pdu->targetWidth);  /* targetWidth (4 bytes) */
	Stream_Write_UINT32(s, pdu->targetHeight); /* targetHeight  (4 bytes) */
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_frame_acknowledge_pdu(RdpgfxServerContext* context, wStream* s)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	RDPGFX_FRAME_ACKNOWLEDGE_PDU pdu;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, pdu.queueDepth);         /* queueDepth (4 bytes) */
	Stream_Read_UINT32(s, pdu.frameId);            /* frameId (4 bytes) */
	Stream_Read_UINT32(s, pdu.totalFramesDecoded); /* totalFramesDecoded (4 bytes) */

	if (context)
	{
		IFCALLRET(context->FrameAcknowledge, error, context, &pdu);

		if (error)
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "context->FrameAcknowledge failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_cache_import_offer_pdu(RdpgfxServerContext* context, wStream* s)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;

	RDPGFX_CACHE_IMPORT_OFFER_PDU pdu = { 0 };
	RDPGFX_CACHE_ENTRY_METADATA* cacheEntry = NULL;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return ERROR_INVALID_DATA;

	/* cacheEntriesCount (2 bytes) */
	Stream_Read_UINT16(s, pdu.cacheEntriesCount);

	/* 2.2.2.16 RDPGFX_CACHE_IMPORT_OFFER_PDU */
	if (pdu.cacheEntriesCount >= 5462)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Invalid cacheEntriesCount: %" PRIu16 "",
		           pdu.cacheEntriesCount);
		return ERROR_INVALID_DATA;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSize(TAG, s, pdu.cacheEntriesCount, 12ull))
		return ERROR_INVALID_DATA;

	for (UINT16 index = 0; index < pdu.cacheEntriesCount; index++)
	{
		cacheEntry = &(pdu.cacheEntries[index]);
		Stream_Read_UINT64(s, cacheEntry->cacheKey);     /* cacheKey (8 bytes) */
		Stream_Read_UINT32(s, cacheEntry->bitmapLength); /* bitmapLength (4 bytes) */
	}

	if (context)
	{
		IFCALLRET(context->CacheImportOffer, error, context, &pdu);

		if (error)
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "context->CacheImportOffer failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_caps_advertise_pdu(RdpgfxServerContext* context, wStream* s)
{
	RDPGFX_CAPSET* capsSets = NULL;
	RDPGFX_CAPS_ADVERTISE_PDU pdu = { 0 };
	UINT error = ERROR_INVALID_DATA;

	if (!context)
		return ERROR_BAD_ARGUMENTS;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.capsSetCount); /* capsSetCount (2 bytes) */
	if (pdu.capsSetCount > 0)
	{
		capsSets = calloc(pdu.capsSetCount, (RDPGFX_CAPSET_BASE_SIZE + 4));
		if (!capsSets)
			return ERROR_OUTOFMEMORY;
	}

	pdu.capsSets = capsSets;

	for (UINT16 index = 0; index < pdu.capsSetCount; index++)
	{
		RDPGFX_CAPSET* capsSet = &(pdu.capsSets[index]);

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			goto fail;

		Stream_Read_UINT32(s, capsSet->version); /* version (4 bytes) */
		Stream_Read_UINT32(s, capsSet->length);  /* capsDataLength (4 bytes) */

		if (capsSet->length >= 4)
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				goto fail;

			Stream_Peek_UINT32(s, capsSet->flags); /* capsData (4 bytes) */
		}

		if (!Stream_SafeSeek(s, capsSet->length))
			goto fail;
	}

	error = ERROR_BAD_CONFIGURATION;
	IFCALLRET(context->CapsAdvertise, error, context, &pdu);

	if (error)
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "context->CapsAdvertise failed with error %" PRIu32 "", error);

fail:
	free(capsSets);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_qoe_frame_acknowledge_pdu(RdpgfxServerContext* context, wStream* s)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU pdu;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, pdu.frameId);     /* frameId (4 bytes) */
	Stream_Read_UINT32(s, pdu.timestamp);   /* timestamp (4 bytes) */
	Stream_Read_UINT16(s, pdu.timeDiffSE);  /* timeDiffSE (2 bytes) */
	Stream_Read_UINT16(s, pdu.timeDiffEDR); /* timeDiffEDR (2 bytes) */

	if (context)
	{
		IFCALLRET(context->QoeFrameAcknowledge, error, context, &pdu);

		if (error)
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "context->QoeFrameAcknowledge failed with error %" PRIu32 "", error);
	}

	return error;
}

static UINT
rdpgfx_send_map_surface_to_scaled_output_pdu(RdpgfxServerContext* context,
                                             const RDPGFX_MAP_SURFACE_TO_SCALED_OUTPUT_PDU* pdu)
{
	if (!checkCapsAreExchanged(context))
		return CHANNEL_RC_NOT_INITIALIZED;
	wStream* s = rdpgfx_server_single_packet_new(context->priv->log,
	                                             RDPGFX_CMDID_MAPSURFACETOSCALEDOUTPUT, 20);

	if (!s)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "rdpgfx_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, pdu->surfaceId);     /* surfaceId (2 bytes) */
	Stream_Write_UINT16(s, 0);                  /* reserved (2 bytes). Must be 0 */
	Stream_Write_UINT32(s, pdu->outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Write_UINT32(s, pdu->outputOriginY); /* outputOriginY (4 bytes) */
	Stream_Write_UINT32(s, pdu->targetWidth);   /* targetWidth (4 bytes) */
	Stream_Write_UINT32(s, pdu->targetHeight);  /* targetHeight (4 bytes) */
	return rdpgfx_server_single_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_server_receive_pdu(RdpgfxServerContext* context, wStream* s)
{
	size_t beg = 0;
	size_t end = 0;
	RDPGFX_HEADER header;
	UINT error = CHANNEL_RC_OK;
	beg = Stream_GetPosition(s);

	if ((error = rdpgfx_read_header(s, &header)))
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "rdpgfx_read_header failed with error %" PRIu32 "!", error);
		return error;
	}

#ifdef WITH_DEBUG_RDPGFX
	WLog_DBG(TAG, "cmdId: %s (0x%04" PRIX16 ") flags: 0x%04" PRIX16 " pduLength: %" PRIu32 "",
	         rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId, header.flags, header.pduLength);
#endif

	switch (header.cmdId)
	{
		case RDPGFX_CMDID_FRAMEACKNOWLEDGE:
			if ((error = rdpgfx_recv_frame_acknowledge_pdu(context, s)))
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "rdpgfx_recv_frame_acknowledge_pdu "
				           "failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CACHEIMPORTOFFER:
			if ((error = rdpgfx_recv_cache_import_offer_pdu(context, s)))
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "rdpgfx_recv_cache_import_offer_pdu "
				           "failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CAPSADVERTISE:
			if ((error = rdpgfx_recv_caps_advertise_pdu(context, s)))
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "rdpgfx_recv_caps_advertise_pdu "
				           "failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_QOEFRAMEACKNOWLEDGE:
			if ((error = rdpgfx_recv_qoe_frame_acknowledge_pdu(context, s)))
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "rdpgfx_recv_qoe_frame_acknowledge_pdu "
				           "failed with error %" PRIu32 "!",
				           error);

			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			break;
	}

	if (error)
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "Error while parsing GFX cmdId: %s (0x%04" PRIX16 ")",
		           rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId);
		return error;
	}

	end = Stream_GetPosition(s);

	if (end != (beg + header.pduLength))
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "Unexpected gfx pdu end: Actual: %" PRIuz ", Expected: %" PRIuz "", end,
		           (beg + header.pduLength));
		Stream_SetPosition(s, (beg + header.pduLength));
	}

	return error;
}

static BOOL rdpgfx_server_close(RdpgfxServerContext* context);

static DWORD WINAPI rdpgfx_server_thread_func(LPVOID arg)
{
	RdpgfxServerContext* context = (RdpgfxServerContext*)arg;
	WINPR_ASSERT(context);

	RdpgfxServerPrivate* priv = context->priv;
	DWORD status = 0;
	DWORD nCount = 0;
	HANDLE events[8] = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(priv);

	if (priv->ownThread)
	{
		WINPR_ASSERT(priv->stopEvent);
		events[nCount++] = priv->stopEvent;
	}

	WINPR_ASSERT(priv->channelEvent);
	events[nCount++] = priv->channelEvent;

	/* Main virtual channel loop. RDPGFX do not need version negotiation */
	while (TRUE)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}

		/* Stop Event */
		if (status == WAIT_OBJECT_0)
			break;

		if ((error = rdpgfx_server_handle_messages(context)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "rdpgfx_server_handle_messages failed with error %" PRIu32 "", error);
			break;
		}
	}

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error, "rdpgfx_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

static BOOL rdpgfx_server_open(RdpgfxServerContext* context)
{
	WINPR_ASSERT(context);
	RdpgfxServerPrivate* priv = context->priv;
	void* buffer = NULL;

	WINPR_ASSERT(priv);

	if (!priv->isOpened)
	{
		PULONG pSessionId = NULL;
		DWORD BytesReturned = 0;
		priv->SessionId = WTS_CURRENT_SESSION;
		UINT32 channelId = 0;
		BOOL status = TRUE;

		if (WTSQuerySessionInformationA(context->vcm, WTS_CURRENT_SESSION, WTSSessionId,
		                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "WTSQuerySessionInformationA failed!");
			return FALSE;
		}

		priv->SessionId = (DWORD)*pSessionId;
		WTSFreeMemory(pSessionId);
		priv->rdpgfx_channel = WTSVirtualChannelOpenEx(priv->SessionId, RDPGFX_DVC_CHANNEL_NAME,
		                                               WTS_CHANNEL_OPTION_DYNAMIC);

		if (!priv->rdpgfx_channel)
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "WTSVirtualChannelOpenEx failed!");
			return FALSE;
		}

		channelId = WTSChannelGetIdByHandle(priv->rdpgfx_channel);

		IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
		if (!status)
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "context->ChannelIdAssigned failed!");
			goto fail;
		}

		/* Query for channel event handle */
		if (!WTSVirtualChannelQuery(priv->rdpgfx_channel, WTSVirtualEventHandle, &buffer,
		                            &BytesReturned) ||
		    (BytesReturned != sizeof(HANDLE)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "WTSVirtualChannelQuery failed "
			           "or invalid returned size(%" PRIu32 ")",
			           BytesReturned);

			if (buffer)
				WTSFreeMemory(buffer);

			goto fail;
		}

		priv->channelEvent = *(HANDLE*)buffer;
		WTSFreeMemory(buffer);

		if (!(priv->zgfx = zgfx_context_new(TRUE)))
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "Create zgfx context failed!");
			goto fail;
		}

		priv->isReady = FALSE;
		const RDPGFX_CAPSET empty = { 0 };
		priv->activeCapSet = empty;
		if (priv->ownThread)
		{
			if (!(priv->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
			{
				WLog_Print(context->priv->log, WLOG_ERROR, "CreateEvent failed!");
				goto fail;
			}

			if (!(priv->thread =
			          CreateThread(NULL, 0, rdpgfx_server_thread_func, (void*)context, 0, NULL)))
			{
				WLog_Print(context->priv->log, WLOG_ERROR, "CreateThread failed!");
				goto fail;
			}
		}

		priv->isOpened = TRUE;
		return TRUE;
	}

	WLog_Print(context->priv->log, WLOG_ERROR, "RDPGFX channel is already opened!");
	return FALSE;
fail:
	rdpgfx_server_close(context);
	return FALSE;
}

BOOL rdpgfx_server_close(RdpgfxServerContext* context)
{
	WINPR_ASSERT(context);

	RdpgfxServerPrivate* priv = context->priv;
	WINPR_ASSERT(priv);

	if (priv->ownThread && priv->thread)
	{
		(void)SetEvent(priv->stopEvent);

		if (WaitForSingleObject(priv->thread, INFINITE) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", GetLastError());
			return FALSE;
		}

		(void)CloseHandle(priv->thread);
		(void)CloseHandle(priv->stopEvent);
		priv->thread = NULL;
		priv->stopEvent = NULL;
	}

	zgfx_context_free(priv->zgfx);
	priv->zgfx = NULL;

	if (priv->rdpgfx_channel)
	{
		(void)WTSVirtualChannelClose(priv->rdpgfx_channel);
		priv->rdpgfx_channel = NULL;
	}

	priv->channelEvent = NULL;
	priv->isOpened = FALSE;
	priv->isReady = FALSE;
	const RDPGFX_CAPSET empty = { 0 };
	priv->activeCapSet = empty;
	return TRUE;
}

static BOOL rdpgfx_server_initialize(RdpgfxServerContext* context, BOOL externalThread)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	if (context->priv->isOpened)
	{
		WLog_Print(context->priv->log, WLOG_WARN,
		           "Application error: RDPEGFX channel already initialized, "
		           "calling in this state is not possible!");
		return FALSE;
	}

	context->priv->ownThread = !externalThread;
	return TRUE;
}

RdpgfxServerContext* rdpgfx_server_context_new(HANDLE vcm)
{
	RdpgfxServerContext* context = (RdpgfxServerContext*)calloc(1, sizeof(RdpgfxServerContext));

	if (!context)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	context->vcm = vcm;
	context->Initialize = rdpgfx_server_initialize;
	context->Open = rdpgfx_server_open;
	context->Close = rdpgfx_server_close;
	context->ResetGraphics = rdpgfx_send_reset_graphics_pdu;
	context->StartFrame = rdpgfx_send_start_frame_pdu;
	context->EndFrame = rdpgfx_send_end_frame_pdu;
	context->SurfaceCommand = rdpgfx_send_surface_command;
	context->SurfaceFrameCommand = rdpgfx_send_surface_frame_command;
	context->DeleteEncodingContext = rdpgfx_send_delete_encoding_context_pdu;
	context->CreateSurface = rdpgfx_send_create_surface_pdu;
	context->DeleteSurface = rdpgfx_send_delete_surface_pdu;
	context->SolidFill = rdpgfx_send_solid_fill_pdu;
	context->SurfaceToSurface = rdpgfx_send_surface_to_surface_pdu;
	context->SurfaceToCache = rdpgfx_send_surface_to_cache_pdu;
	context->CacheToSurface = rdpgfx_send_cache_to_surface_pdu;
	context->CacheImportOffer = rdpgfx_process_cache_import_offer_pdu;
	context->CacheImportReply = rdpgfx_send_cache_import_reply_pdu;
	context->EvictCacheEntry = rdpgfx_send_evict_cache_entry_pdu;
	context->MapSurfaceToOutput = rdpgfx_send_map_surface_to_output_pdu;
	context->MapSurfaceToWindow = rdpgfx_send_map_surface_to_window_pdu;
	context->MapSurfaceToScaledOutput = rdpgfx_send_map_surface_to_scaled_output_pdu;
	context->MapSurfaceToScaledWindow = rdpgfx_send_map_surface_to_scaled_window_pdu;
	context->CapsAdvertise = NULL;
	context->CapsConfirm = rdpgfx_send_caps_confirm_pdu;
	context->FrameAcknowledge = NULL;
	context->QoeFrameAcknowledge = NULL;
	RdpgfxServerPrivate* priv = context->priv =
	    (RdpgfxServerPrivate*)calloc(1, sizeof(RdpgfxServerPrivate));

	if (!priv)
	{
		WLog_ERR(TAG, "calloc failed!");
		goto fail;
	}

	priv->log = WLog_Get(TAG);
	if (!priv->log)
		goto fail;

	/* Create shared input stream */
	priv->input_stream = Stream_New(NULL, 4);

	if (!priv->input_stream)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "Stream_New failed!");
		goto fail;
	}

	priv->isOpened = FALSE;
	priv->isReady = FALSE;
	priv->ownThread = TRUE;

	const RDPGFX_CAPSET empty = { 0 };
	priv->activeCapSet = empty;
	return context;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	rdpgfx_server_context_free(context);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void rdpgfx_server_context_free(RdpgfxServerContext* context)
{
	if (!context)
		return;

	rdpgfx_server_close(context);

	if (context->priv)
		Stream_Free(context->priv->input_stream, TRUE);

	free(context->priv);
	free(context);
}

HANDLE rdpgfx_server_get_event_handle(RdpgfxServerContext* context)
{
	if (!context)
		return NULL;
	if (!context->priv)
		return NULL;
	return context->priv->channelEvent;
}

/*
 * Handle rpdgfx messages - server side
 *
 * @param Server side context
 *
 * @return 0 on success
 *         ERROR_NO_DATA if no data could be read this time
 *         otherwise a Win32 error code
 */
UINT rdpgfx_server_handle_messages(RdpgfxServerContext* context)
{
	DWORD BytesReturned = 0;
	void* buffer = NULL;
	UINT ret = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);

	RdpgfxServerPrivate* priv = context->priv;
	wStream* s = priv->input_stream;

	/* Check whether the dynamic channel is ready */
	if (!priv->isReady)
	{
		if (WTSVirtualChannelQuery(priv->rdpgfx_channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			if (GetLastError() == ERROR_NO_DATA)
				return ERROR_NO_DATA;

			WLog_Print(context->priv->log, WLOG_ERROR, "WTSVirtualChannelQuery failed");
			return ERROR_INTERNAL_ERROR;
		}

		priv->isReady = *((BOOL*)buffer);
		WTSFreeMemory(buffer);
	}

	/* Consume channel event only after the gfx dynamic channel is ready */
	if (priv->isReady)
	{
		Stream_SetPosition(s, 0);

		if (!WTSVirtualChannelRead(priv->rdpgfx_channel, 0, NULL, 0, &BytesReturned))
		{
			if (GetLastError() == ERROR_NO_DATA)
				return ERROR_NO_DATA;

			WLog_Print(context->priv->log, WLOG_ERROR, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		if (BytesReturned < 1)
			return CHANNEL_RC_OK;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		const size_t len = Stream_Capacity(s);
		if (len > UINT32_MAX)
			return ERROR_INTERNAL_ERROR;
		if (WTSVirtualChannelRead(priv->rdpgfx_channel, 0, Stream_BufferAs(s, char), (UINT32)len,
		                          &BytesReturned) == FALSE)
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		Stream_SetLength(s, BytesReturned);
		Stream_SetPosition(s, 0);

		while (Stream_GetPosition(s) < Stream_Length(s))
		{
			if ((ret = rdpgfx_server_receive_pdu(context, s)))
			{
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "rdpgfx_server_receive_pdu "
				           "failed with error %" PRIu32 "!",
				           ret);
				return ret;
			}
		}
	}

	return ret;
}
