/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <assert.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>
#include <freerdp/channels/log.h>

#include "rdpgfx_common.h"
#include "rdpgfx_codec.h"

#include "rdpgfx_main.h"

#define TAG CHANNELS_TAG("rdpgfx.client")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_caps_advertise_pdu(RDPGFX_CHANNEL_CALLBACK* callback)
{
	UINT error;
	wStream* s;
	UINT16 index;
	RDPGFX_PLUGIN* gfx;
	RDPGFX_HEADER header;
	RDPGFX_CAPSET* capsSet;
	RDPGFX_CAPSET capsSets[2];
	RDPGFX_CAPS_ADVERTISE_PDU pdu;

	gfx = (RDPGFX_PLUGIN*) callback->plugin;

	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_CAPSADVERTISE;

	pdu.capsSetCount = 0;
	pdu.capsSets = (RDPGFX_CAPSET*) capsSets;

	capsSet = &capsSets[pdu.capsSetCount++];
	capsSet->version = RDPGFX_CAPVERSION_8;
	capsSet->flags = 0;

	if (gfx->ThinClient)
		capsSet->flags |= RDPGFX_CAPS_FLAG_THINCLIENT;

	if (gfx->SmallCache)
		capsSet->flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

	capsSet = &capsSets[pdu.capsSetCount++];
	capsSet->version = RDPGFX_CAPVERSION_81;
	capsSet->flags = 0;

	if (gfx->ThinClient)
		capsSet->flags |= RDPGFX_CAPS_FLAG_THINCLIENT;

	if (gfx->SmallCache)
		capsSet->flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

	if (gfx->H264)
		capsSet->flags |= RDPGFX_CAPS_FLAG_H264ENABLED;

	header.pduLength = RDPGFX_HEADER_SIZE + 2 + (pdu.capsSetCount * RDPGFX_CAPSET_SIZE);

	WLog_DBG(TAG, "SendCapsAdvertisePdu");

	s = Stream_New(NULL, header.pduLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
	{
		WLog_ERR(TAG, "rdpgfx_write_header failed with error %lu!", error);
		return error;
	}

	/* RDPGFX_CAPS_ADVERTISE_PDU */

	Stream_Write_UINT16(s, pdu.capsSetCount); /* capsSetCount (2 bytes) */

	for (index = 0; index < pdu.capsSetCount; index++)
	{
		capsSet = &(pdu.capsSets[index]);
		Stream_Write_UINT32(s, capsSet->version); /* version (4 bytes) */
		Stream_Write_UINT32(s, 4); /* capsDataLength (4 bytes) */
		Stream_Write_UINT32(s, capsSet->flags); /* capsData (4 bytes) */
	}

	Stream_SealLength(s);

	error = callback->channel->Write(callback->channel, (UINT32) Stream_Length(s), Stream_Buffer(s), NULL);

	Stream_Free(s, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_caps_confirm_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CAPSET capsSet;
	UINT32 capsDataLength;
	RDPGFX_CAPS_CONFIRM_PDU pdu;

	pdu.capsSet = &capsSet;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, capsSet.version); /* version (4 bytes) */
	Stream_Read_UINT32(s, capsDataLength); /* capsDataLength (4 bytes) */
	Stream_Read_UINT32(s, capsSet.flags); /* capsData (4 bytes) */

	WLog_DBG(TAG, "RecvCapsConfirmPdu: version: 0x%04X flags: 0x%04X",
			capsSet.version, capsSet.flags);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_frame_acknowledge_pdu(RDPGFX_CHANNEL_CALLBACK* callback, RDPGFX_FRAME_ACKNOWLEDGE_PDU* pdu)
{
	UINT error;
	wStream* s;
	RDPGFX_HEADER header;

	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_FRAMEACKNOWLEDGE;
	header.pduLength = RDPGFX_HEADER_SIZE + 12;

	WLog_DBG(TAG, "SendFrameAcknowledgePdu: %d", pdu->frameId);

	s = Stream_New(NULL, header.pduLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
	{
		WLog_ERR(TAG, "rdpgfx_write_header failed with error %lu!", error);
		return error;
	}

	/* RDPGFX_FRAME_ACKNOWLEDGE_PDU */

	Stream_Write_UINT32(s, pdu->queueDepth); /* queueDepth (4 bytes) */
	Stream_Write_UINT32(s, pdu->frameId); /* frameId (4 bytes) */
	Stream_Write_UINT32(s, pdu->totalFramesDecoded); /* totalFramesDecoded (4 bytes) */

	error = callback->channel->Write(callback->channel, (UINT32) Stream_Length(s), Stream_Buffer(s), NULL);

	Stream_Free(s, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_reset_graphics_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	int pad;
	UINT32 index;
	MONITOR_DEF* monitor;
	RDPGFX_RESET_GRAPHICS_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.width); /* width (4 bytes) */
	Stream_Read_UINT32(s, pdu.height); /* height (4 bytes) */
	Stream_Read_UINT32(s, pdu.monitorCount); /* monitorCount (4 bytes) */

	if (Stream_GetRemainingLength(s) < (pdu.monitorCount * 20))
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	pdu.monitorDefArray = (MONITOR_DEF*) calloc(pdu.monitorCount, sizeof(MONITOR_DEF));

	if (!pdu.monitorDefArray)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.monitorCount; index++)
	{
		monitor = &(pdu.monitorDefArray[index]);
		Stream_Read_UINT32(s, monitor->left); /* left (4 bytes) */
		Stream_Read_UINT32(s, monitor->top); /* top (4 bytes) */
		Stream_Read_UINT32(s, monitor->right); /* right (4 bytes) */
		Stream_Read_UINT32(s, monitor->bottom); /* bottom (4 bytes) */
		Stream_Read_UINT32(s, monitor->flags); /* flags (4 bytes) */
	}

	pad = 340 - (RDPGFX_HEADER_SIZE + 12 + (pdu.monitorCount * 20));

	if (Stream_GetRemainingLength(s) < (size_t) pad)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		free(pdu.monitorDefArray);
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Seek(s, pad); /* pad (total size is 340 bytes) */

	WLog_DBG(TAG, "RecvResetGraphicsPdu: width: %d height: %d count: %d",
			pdu.width, pdu.height, pdu.monitorCount);

	if (context)
	{
		IFCALLRET(context->ResetGraphics, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->ResetGraphics failed with error %lu", error);
	}

	free(pdu.monitorDefArray);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_evict_cache_entry_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_EVICT_CACHE_ENTRY_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */

	WLog_DBG(TAG, "RecvEvictCacheEntryPdu: cacheSlot: %d", pdu.cacheSlot);

	if (context)
	{
		IFCALLRET(context->EvictCacheEntry, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->EvictCacheEntry failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_recv_cache_import_reply_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_CACHE_IMPORT_REPLY_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.importedEntriesCount); /* cacheSlot (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (pdu.importedEntriesCount * 2))
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	pdu.cacheSlots = (UINT16*) calloc(pdu.importedEntriesCount, sizeof(UINT16));

	if (!pdu.cacheSlots)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.importedEntriesCount; index++)
	{
		Stream_Read_UINT16(s, pdu.cacheSlots[index]); /* cacheSlot (2 bytes) */
	}

	WLog_DBG(TAG, "RecvCacheImportReplyPdu: importedEntriesCount: %d",
			pdu.importedEntriesCount);

	if (context)
	{
		IFCALLRET(context->CacheImportReply, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->CacheImportReply failed with error %lu", error);
	}

	free(pdu.cacheSlots);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_create_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CREATE_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 7)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.width); /* width (2 bytes) */
	Stream_Read_UINT16(s, pdu.height); /* height (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* RDPGFX_PIXELFORMAT (1 byte) */

	WLog_DBG(TAG, "RecvCreateSurfacePdu: surfaceId: %d width: %d height: %d pixelFormat: 0x%02X",
			pdu.surfaceId, pdu.width, pdu.height, pdu.pixelFormat);

	if (context)
	{
		IFCALLRET(context->CreateSurface, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->CreateSurface failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_recv_delete_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */

	WLog_DBG(TAG, "RecvDeleteSurfacePdu: surfaceId: %d", pdu.surfaceId);

	if (context)
	{
		IFCALLRET(context->DeleteSurface, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->DeleteSurface failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_start_frame_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_START_FRAME_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.timestamp); /* timestamp (4 bytes) */
	Stream_Read_UINT32(s, pdu.frameId); /* frameId (4 bytes) */

	WLog_DBG(TAG, "RecvStartFramePdu: frameId: %d timestamp: 0x%04X",
			pdu.frameId, pdu.timestamp);

	if (context)
	{
		IFCALLRET(context->StartFrame, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->StartFrame failed with error %lu", error);
	}

	gfx->UnacknowledgedFrames++;

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_end_frame_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_END_FRAME_PDU pdu;
	RDPGFX_FRAME_ACKNOWLEDGE_PDU ack;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.frameId); /* frameId (4 bytes) */

	WLog_DBG(TAG, "RecvEndFramePdu: frameId: %d", pdu.frameId);

	if (context)
	{
		IFCALLRET(context->EndFrame, error, context, &pdu);
		if (error)
		{
			WLog_ERR(TAG, "context->EndFrame failed with error %lu", error);
			return error;
		}
	}

	gfx->UnacknowledgedFrames--;
	gfx->TotalDecodedFrames++;

	ack.frameId = pdu.frameId;
	ack.totalFramesDecoded = gfx->TotalDecodedFrames;

	if (gfx->suspendFrameAcks)
	{
		ack.queueDepth = SUSPEND_FRAME_ACKNOWLEDGEMENT;

		if (gfx->TotalDecodedFrames == 1)
		if ((error = rdpgfx_send_frame_acknowledge_pdu(callback, &ack)))
			WLog_ERR(TAG, "rdpgfx_send_frame_acknowledge_pdu failed with error %lu", error);
	}
	else
	{
		ack.queueDepth = QUEUE_DEPTH_UNAVAILABLE;
		if ((error = rdpgfx_send_frame_acknowledge_pdu(callback, &ack)))
			WLog_ERR(TAG, "rdpgfx_send_frame_acknowledge_pdu failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_wire_to_surface_1_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_WIRE_TO_SURFACE_PDU_1 pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	UINT error;

	if (Stream_GetRemainingLength(s) < 17)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId); /* codecId (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* pixelFormat (1 byte) */

	if ((error = rdpgfx_read_rect16(s, &(pdu.destRect)))) /* destRect (8 bytes) */
	{
		WLog_ERR(TAG, "rdpgfx_read_rect16 failed with error %lu", error);
		return error;
	}

	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */

	if (pdu.bitmapDataLength > Stream_GetRemainingLength(s))
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	pdu.bitmapData = Stream_Pointer(s);
	Stream_Seek(s, pdu.bitmapDataLength);

	WLog_DBG(TAG, "RecvWireToSurface1Pdu: surfaceId: %d codecId: %s (0x%04X) pixelFormat: 0x%04X "
			"destRect: left: %d top: %d right: %d bottom: %d bitmapDataLength: %d",
			(int) pdu.surfaceId, rdpgfx_get_codec_id_string(pdu.codecId), pdu.codecId, pdu.pixelFormat,
			pdu.destRect.left, pdu.destRect.top, pdu.destRect.right, pdu.destRect.bottom,
			pdu.bitmapDataLength);

	cmd.surfaceId = pdu.surfaceId;
	cmd.codecId = pdu.codecId;
	cmd.contextId = 0;
	cmd.format = pdu.pixelFormat;
	cmd.left = pdu.destRect.left;
	cmd.top = pdu.destRect.top;
	cmd.right = pdu.destRect.right;
	cmd.bottom = pdu.destRect.bottom;
	cmd.width = cmd.right - cmd.left;
	cmd.height = cmd.bottom - cmd.top;
	cmd.length = pdu.bitmapDataLength;
	cmd.data = pdu.bitmapData;

	if ((error = rdpgfx_decode(gfx, &cmd)))
		WLog_ERR(TAG, "rdpgfx_decode failed with error %lu!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_wire_to_surface_2_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_WIRE_TO_SURFACE_PDU_2 pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 13)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId); /* codecId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId); /* codecContextId (4 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* pixelFormat (1 byte) */

	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */

	pdu.bitmapData = Stream_Pointer(s);
	Stream_Seek(s, pdu.bitmapDataLength);

	WLog_DBG(TAG, "RecvWireToSurface2Pdu: surfaceId: %d codecId: %s (0x%04X) "
			"codecContextId: %d pixelFormat: 0x%04X bitmapDataLength: %d",
			(int) pdu.surfaceId, rdpgfx_get_codec_id_string(pdu.codecId), pdu.codecId,
			pdu.codecContextId, pdu.pixelFormat, pdu.bitmapDataLength);

	cmd.surfaceId = pdu.surfaceId;
	cmd.codecId = pdu.codecId;
	cmd.contextId = pdu.codecContextId;
	cmd.format = pdu.pixelFormat;
	cmd.left = 0;
	cmd.top = 0;
	cmd.right = 0;
	cmd.bottom = 0;
	cmd.width = 0;
	cmd.height = 0;
	cmd.length = pdu.bitmapDataLength;
	cmd.data = pdu.bitmapData;

	if (context)
	{
		IFCALLRET(context->SurfaceCommand, error, context, &cmd);
		if (error)
			WLog_ERR(TAG, "context->SurfaceCommand failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_delete_encoding_context_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_ENCODING_CONTEXT_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId); /* codecContextId (4 bytes) */

	WLog_DBG(TAG, "RecvDeleteEncodingContextPdu: surfaceId: %d codecContextId: %d",
			pdu.surfaceId, pdu.codecContextId);

	if (context)
	{
		IFCALLRET(context->DeleteEncodingContext, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->DeleteEncodingContext failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_recv_solid_fill_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_RECT16* fillRect;
	RDPGFX_SOLID_FILL_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	if ((error = rdpgfx_read_color32(s, &(pdu.fillPixel)))) /* fillPixel (4 bytes) */
	{
		WLog_ERR(TAG, "rdpgfx_read_color32 failed with error %lu!", error);
		return error;
	}
	Stream_Read_UINT16(s, pdu.fillRectCount); /* fillRectCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (pdu.fillRectCount * 8))
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	pdu.fillRects = (RDPGFX_RECT16*) calloc(pdu.fillRectCount, sizeof(RDPGFX_RECT16));

	if (!pdu.fillRects)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.fillRectCount; index++)
	{
		fillRect = &(pdu.fillRects[index]);
		if ((error = rdpgfx_read_rect16(s, fillRect)))
		{
			WLog_ERR(TAG, "rdpgfx_read_rect16 failed with error %lu!", error);
			free(pdu.fillRects);
			return error;
		}
	}

	WLog_DBG(TAG, "RecvSolidFillPdu: surfaceId: %d fillRectCount: %d",
			pdu.surfaceId, pdu.fillRectCount);

	if (context)
	{
		IFCALLRET(context->SolidFill, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->SolidFill failed with error %lu", error);
	}
	
	free(pdu.fillRects);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_surface_to_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_POINT16* destPt;
	RDPGFX_SURFACE_TO_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error;

	if (Stream_GetRemainingLength(s) < 14)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceIdSrc); /* surfaceIdSrc (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceIdDest); /* surfaceIdDest (2 bytes) */
	if ((error = rdpgfx_read_rect16(s, &(pdu.rectSrc)))) /* rectSrc (8 bytes ) */
	{
		WLog_ERR(TAG, "rdpgfx_read_rect16 failed with error %lu!", error);
		return error;
	}

	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (pdu.destPtsCount * 4))
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	pdu.destPts = (RDPGFX_POINT16*) calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.destPtsCount; index++)
	{
		destPt = &(pdu.destPts[index]);
		if ((error = rdpgfx_read_point16(s, destPt)))
		{
			WLog_ERR(TAG, "rdpgfx_read_point16 failed with error %lu!", error);
			free(pdu.destPts);
			return error;
		}
	}

	WLog_DBG(TAG, "RecvSurfaceToSurfacePdu: surfaceIdSrc: %d surfaceIdDest: %d "
			"left: %d top: %d right: %d bottom: %d destPtsCount: %d",
			pdu.surfaceIdSrc, pdu.surfaceIdDest,
			pdu.rectSrc.left, pdu.rectSrc.top, pdu.rectSrc.right, pdu.rectSrc.bottom,
			pdu.destPtsCount);

	if (context)
	{
		IFCALLRET(context->SurfaceToSurface, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->SurfaceToSurface failed with error %lu", error);
	}

	free(pdu.destPts);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_surface_to_cache_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_TO_CACHE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error;

	if (Stream_GetRemainingLength(s) < 20)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.cacheKey); /* cacheKey (8 bytes) */
	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */
	if ((error = rdpgfx_read_rect16(s, &(pdu.rectSrc)))) /* rectSrc (8 bytes ) */
	{
		WLog_ERR(TAG, "rdpgfx_read_rect16 failed with error %lu!", error);
		return error;
	}

	WLog_DBG(TAG, "RecvSurfaceToCachePdu: surfaceId: %d cacheKey: 0x%08X cacheSlot: %d "
			"left: %d top: %d right: %d bottom: %d",
			pdu.surfaceId, (int) pdu.cacheKey, pdu.cacheSlot,
			pdu.rectSrc.left, pdu.rectSrc.top,
			pdu.rectSrc.right, pdu.rectSrc.bottom);

	if (context)
	{
		IFCALLRET(context->SurfaceToCache, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->SurfaceToCache failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_recv_cache_to_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_POINT16* destPt;
	RDPGFX_CACHE_TO_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (pdu.destPtsCount * 4))
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	pdu.destPts = (RDPGFX_POINT16*) calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.destPtsCount; index++)
	{
		destPt = &(pdu.destPts[index]);
		if ((error = rdpgfx_read_point16(s, destPt)))
		{
			WLog_ERR(TAG, "rdpgfx_read_point16 failed with error %lu", error);
			free(pdu.destPts);
			return error;
		}
	}

	WLog_DBG(TAG, "RdpGfxRecvCacheToSurfacePdu: cacheSlot: %d surfaceId: %d destPtsCount: %d",
			pdu.cacheSlot, (int) pdu.surfaceId, pdu.destPtsCount);

	if (context)
	{
		IFCALLRET(context->CacheToSurface, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->CacheToSurface failed with error %lu", error);
	}

	free(pdu.destPts);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_map_surface_to_output_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.reserved); /* reserved (2 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginY); /* outputOriginY (4 bytes) */

	WLog_DBG(TAG, "RecvMapSurfaceToOutputPdu: surfaceId: %d outputOriginX: %d outputOriginY: %d",
			(int) pdu.surfaceId, pdu.outputOriginX, pdu.outputOriginY);

	if (context)
	{
		IFCALLRET(context->MapSurfaceToOutput, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->MapSurfaceToOutput failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_recv_map_surface_to_window_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_WINDOW_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 18)
	{
		WLog_ERR(TAG, "not enought data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.windowId); /* windowId (8 bytes) */
	Stream_Read_UINT32(s, pdu.mappedWidth); /* mappedWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.mappedHeight); /* mappedHeight (4 bytes) */

	WLog_DBG(TAG, "RecvMapSurfaceToWindowPdu: surfaceId: %d windowId: 0x%04X mappedWidth: %d mappedHeight: %d",
			pdu.surfaceId, (int) pdu.windowId, pdu.mappedWidth, pdu.mappedHeight);

	if (context && context->MapSurfaceToWindow)
	{
		IFCALLRET(context->MapSurfaceToWindow, error, context, &pdu);
		if (error)
			WLog_ERR(TAG, "context->MapSurfaceToWindow failed with error %lu", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	int beg, end;
	RDPGFX_HEADER header;
	UINT error;

	beg = Stream_GetPosition(s);

	if ((error = rdpgfx_read_header(s, &header)))
	{
		WLog_ERR(TAG, "rdpgfx_read_header failed with error %lu!", error);
		return error;
	}

#if 1
	WLog_DBG(TAG, "cmdId: %s (0x%04X) flags: 0x%04X pduLength: %d",
			rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId, header.flags, header.pduLength);
#endif

	switch (header.cmdId)
	{
		case RDPGFX_CMDID_WIRETOSURFACE_1:
			if ((error = rdpgfx_recv_wire_to_surface_1_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_wire_to_surface_1_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_WIRETOSURFACE_2:
			if ((error = rdpgfx_recv_wire_to_surface_2_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_wire_to_surface_2_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_DELETEENCODINGCONTEXT:
			if ((error = rdpgfx_recv_delete_encoding_context_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_delete_encoding_context_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_SOLIDFILL:
			if ((error = rdpgfx_recv_solid_fill_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_solid_fill_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_SURFACETOSURFACE:
			if ((error = rdpgfx_recv_surface_to_surface_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_surface_to_surface_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_SURFACETOCACHE:
			if ((error = rdpgfx_recv_surface_to_cache_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_surface_to_cache_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_CACHETOSURFACE:
			if ((error = rdpgfx_recv_cache_to_surface_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_cache_to_surface_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_EVICTCACHEENTRY:
			if ((error = rdpgfx_recv_evict_cache_entry_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_evict_cache_entry_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_CREATESURFACE:
			if ((error = rdpgfx_recv_create_surface_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_create_surface_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_DELETESURFACE:
			if ((error = rdpgfx_recv_delete_surface_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_delete_surface_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_STARTFRAME:
			if ((error = rdpgfx_recv_start_frame_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_start_frame_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_ENDFRAME:
			if ((error = rdpgfx_recv_end_frame_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_end_frame_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_RESETGRAPHICS:
			if ((error = rdpgfx_recv_reset_graphics_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_reset_graphics_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_MAPSURFACETOOUTPUT:
			if ((error = rdpgfx_recv_map_surface_to_output_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_map_surface_to_output_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_CACHEIMPORTREPLY:
			if ((error = rdpgfx_recv_cache_import_reply_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_cache_import_reply_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_CAPSCONFIRM:
			if ((error = rdpgfx_recv_caps_confirm_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_caps_confirm_pdu failed with error %lu!", error);
			break;

		case RDPGFX_CMDID_MAPSURFACETOWINDOW:
			if ((error = rdpgfx_recv_map_surface_to_window_pdu(callback, s)))
				WLog_ERR(TAG, "rdpgfx_recv_map_surface_to_window_pdu failed with error %lu!", error);
			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			break;
	}

	if (error)
	{
		WLog_ERR(TAG,  "Error while parsing GFX cmdId: %s (0x%04X)",
				 rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId);
		return error;
	}

	end = Stream_GetPosition(s);

	if (end != (beg + header.pduLength))
	{
		WLog_ERR(TAG,  "Unexpected gfx pdu end: Actual: %d, Expected: %d",
				 end, (beg + header.pduLength));
		Stream_SetPosition(s, (beg + header.pduLength));
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	wStream* s;
	int status = 0;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	UINT error = CHANNEL_RC_OK;

	status = zgfx_decompress(gfx->zgfx, Stream_Pointer(data), Stream_GetRemainingLength(data), &pDstData, &DstSize, 0);

	if (status < 0)
	{
		WLog_ERR(TAG, "zgfx_decompress failure! status: %d", status);
		return ERROR_INTERNAL_ERROR;
	}

	s = Stream_New(pDstData, DstSize);
	if (!s)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	while (((size_t) Stream_GetPosition(s)) < Stream_Length(s))
	{
		if ((error = rdpgfx_recv_pdu(callback, s)))
		{
			WLog_ERR(TAG, "rdpgfx_recv_pdu failed with error %lu!", error);
			break;
		}
	}

	Stream_Free(s, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	WLog_DBG(TAG, "OnOpen");

	return rdpgfx_send_caps_advertise_pdu(callback);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	int count;
	int index;
	ULONG_PTR* pKeys = NULL;
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	WLog_DBG(TAG, "OnClose");

	free(callback);

	gfx->UnacknowledgedFrames = 0;
	gfx->TotalDecodedFrames = 0;

	if (gfx->zgfx)
	{
		zgfx_context_free(gfx->zgfx);
		gfx->zgfx = zgfx_context_new(FALSE);

		if (!gfx->zgfx)
			return CHANNEL_RC_NO_MEMORY;
	}

	count = HashTable_GetKeys(gfx->SurfaceTable, &pKeys);

	for (index = 0; index < count; index++)
	{
		RDPGFX_DELETE_SURFACE_PDU pdu;

		pdu.surfaceId = ((UINT16) pKeys[index]) - 1;

		if (context && context->DeleteSurface)
		{
			context->DeleteSurface(context, &pdu);
		}
	}

	free(pKeys);

	for (index = 0; index < gfx->MaxCacheSlot; index++)
	{
		if (gfx->CacheSlots[index])
		{
			RDPGFX_EVICT_CACHE_ENTRY_PDU pdu;

			pdu.cacheSlot = (UINT16) index;

			if (context && context->EvictCacheEntry)
			{
				context->EvictCacheEntry(context, &pdu);
			}

			gfx->CacheSlots[index] = NULL;
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback;
	RDPGFX_LISTENER_CALLBACK* listener_callback = (RDPGFX_LISTENER_CALLBACK*) pListenerCallback;

	callback = (RDPGFX_CHANNEL_CALLBACK*) calloc(1, sizeof(RDPGFX_CHANNEL_CALLBACK));

	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = rdpgfx_on_data_received;
	callback->iface.OnOpen = rdpgfx_on_open;
	callback->iface.OnClose = rdpgfx_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT error;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) pPlugin;

	gfx->listener_callback = (RDPGFX_LISTENER_CALLBACK*) calloc(1, sizeof(RDPGFX_LISTENER_CALLBACK));

	if (!gfx->listener_callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	gfx->listener_callback->iface.OnNewChannelConnection = rdpgfx_on_new_channel_connection;
	gfx->listener_callback->plugin = pPlugin;
	gfx->listener_callback->channel_mgr = pChannelMgr;

	error = pChannelMgr->CreateListener(pChannelMgr, RDPGFX_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*) gfx->listener_callback, &(gfx->listener));

	gfx->listener->pInterface = gfx->iface.pInterface;

	WLog_DBG(TAG, "Initialize");

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_plugin_terminated(IWTSPlugin* pPlugin)
{
	int count;
	int index;
	ULONG_PTR* pKeys = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) pPlugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	WLog_DBG(TAG, "Terminated");

	if (gfx->listener_callback)
	{
		free(gfx->listener_callback);
		gfx->listener_callback = NULL;
	}

	if (gfx->zgfx)
	{
		zgfx_context_free(gfx->zgfx);
		gfx->zgfx = NULL;
	}

	count = HashTable_GetKeys(gfx->SurfaceTable, &pKeys);

	for (index = 0; index < count; index++)
	{
		RDPGFX_DELETE_SURFACE_PDU pdu;

		pdu.surfaceId = ((UINT16) pKeys[index]) - 1;

		if (context)
		{
			IFCALLRET(context->DeleteSurface, error, context, &pdu);
			if (error)
			{
				WLog_ERR(TAG, "context->DeleteSurface failed with error %lu", error);
				free(pKeys);
				free(context);
				free(gfx);
				return error;
			}
		}
	}

	free(pKeys);

	HashTable_Free(gfx->SurfaceTable);

	for (index = 0; index < gfx->MaxCacheSlot; index++)
	{
		if (gfx->CacheSlots[index])
		{
			RDPGFX_EVICT_CACHE_ENTRY_PDU pdu;

			pdu.cacheSlot = (UINT16) index;

			if (context)
			{
				IFCALLRET(context->EvictCacheEntry, error, context, &pdu);
				if (error)
				{
					WLog_ERR(TAG, "context->EvictCacheEntry failed with error %lu", error);
					free(context);
					free(gfx);
					return error;
				}
			}

			gfx->CacheSlots[index] = NULL;
		}
	}

	free(context);

	free(gfx);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_set_surface_data(RdpgfxClientContext* context, UINT16 surfaceId, void* pData)
{
	ULONG_PTR key;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	key = ((ULONG_PTR) surfaceId) + 1;

	if (pData)
		HashTable_Add(gfx->SurfaceTable, (void*) key, pData);
	else
		HashTable_Remove(gfx->SurfaceTable, (void*) key);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_get_surface_ids(RdpgfxClientContext* context, UINT16** ppSurfaceIds, UINT16* count_out)
{
	int count;
	int index;
	UINT16* pSurfaceIds;
	ULONG_PTR* pKeys = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	count = HashTable_GetKeys(gfx->SurfaceTable, &pKeys);

	if (count < 1)
	{
		*count_out = 0;
		return CHANNEL_RC_OK;
	}

	pSurfaceIds = (UINT16*) malloc(count * sizeof(UINT16));

	if (!pSurfaceIds)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < count; index++)
	{
		pSurfaceIds[index] = pKeys[index] - 1;
	}

	free(pKeys);
	*ppSurfaceIds = pSurfaceIds;
	*count_out = (UINT16)count;

	return CHANNEL_RC_OK;
}

static void* rdpgfx_get_surface_data(RdpgfxClientContext* context, UINT16 surfaceId)
{
	ULONG_PTR key;
	void* pData = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	key = ((ULONG_PTR) surfaceId) + 1;

	pData = HashTable_GetItemValue(gfx->SurfaceTable, (void*) key);

	return pData;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_set_cache_slot_data(RdpgfxClientContext* context, UINT16 cacheSlot, void* pData)
{
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	if (cacheSlot >= gfx->MaxCacheSlot)
		return ERROR_INVALID_INDEX;

	gfx->CacheSlots[cacheSlot] = pData;

	return CHANNEL_RC_OK;
}

void* rdpgfx_get_cache_slot_data(RdpgfxClientContext* context, UINT16 cacheSlot)
{
	void* pData = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	if (cacheSlot >= gfx->MaxCacheSlot)
		return NULL;

	pData = gfx->CacheSlots[cacheSlot];

	return pData;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		rdpgfx_DVCPluginEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_PLUGIN* gfx;
	RdpgfxClientContext* context;

	gfx = (RDPGFX_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "rdpgfx");

	if (!gfx)
	{
		gfx = (RDPGFX_PLUGIN*) calloc(1, sizeof(RDPGFX_PLUGIN));

		if (!gfx)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		gfx->settings = (rdpSettings*) pEntryPoints->GetRdpSettings(pEntryPoints);

		gfx->iface.Initialize = rdpgfx_plugin_initialize;
		gfx->iface.Connected = NULL;
		gfx->iface.Disconnected = NULL;
		gfx->iface.Terminated = rdpgfx_plugin_terminated;

		gfx->SurfaceTable = HashTable_New(TRUE);

		if (!gfx->SurfaceTable)
		{
			free (gfx);
			WLog_ERR(TAG, "HashTable_New failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		gfx->ThinClient = gfx->settings->GfxThinClient;
		gfx->SmallCache = gfx->settings->GfxSmallCache;
		gfx->Progressive = gfx->settings->GfxProgressive;
		gfx->ProgressiveV2 = gfx->settings->GfxProgressiveV2;
		gfx->H264 = gfx->settings->GfxH264;

		if (gfx->H264)
			gfx->SmallCache = TRUE;

		if (gfx->SmallCache)
			gfx->ThinClient = FALSE;

		gfx->MaxCacheSlot = (gfx->ThinClient) ? 4096 : 25600;


		context = (RdpgfxClientContext*) calloc(1, sizeof(RdpgfxClientContext));

		if (!context)
		{
			free(gfx);
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		context->handle = (void*) gfx;

		context->GetSurfaceIds = rdpgfx_get_surface_ids;
		context->SetSurfaceData = rdpgfx_set_surface_data;
		context->GetSurfaceData = rdpgfx_get_surface_data;
		context->SetCacheSlotData = rdpgfx_set_cache_slot_data;
		context->GetCacheSlotData = rdpgfx_get_cache_slot_data;

		gfx->iface.pInterface = (void*) context;

		gfx->zgfx = zgfx_context_new(FALSE);

		if (!gfx->zgfx)
		{
			free(gfx);
			free(context);
			WLog_ERR(TAG, "zgfx_context_new failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpgfx", (IWTSPlugin*) gfx);
	}

	return error;
}
