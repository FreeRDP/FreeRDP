/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

int rdpgfx_send_caps_advertise_pdu(RDPGFX_CHANNEL_CALLBACK* callback)
{
	int status;
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

	WLog_Print(gfx->log, WLOG_DEBUG, "SendCapsAdvertisePdu");

	s = Stream_New(NULL, header.pduLength);

	rdpgfx_write_header(s, &header);

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

	status = callback->channel->Write(callback->channel, (UINT32) Stream_Length(s), Stream_Buffer(s), NULL);

	Stream_Free(s, TRUE);

	return status;
}

int rdpgfx_recv_caps_confirm_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CAPSET capsSet;
	UINT32 capsDataLength;
	RDPGFX_CAPS_CONFIRM_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	pdu.capsSet = &capsSet;

	if (Stream_GetRemainingLength(s) < 12)
		return -1;

	Stream_Read_UINT32(s, capsSet.version); /* version (4 bytes) */
	Stream_Read_UINT32(s, capsDataLength); /* capsDataLength (4 bytes) */
	Stream_Read_UINT32(s, capsSet.flags); /* capsData (4 bytes) */
	
	/*TODO: interpret this answer*/

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvCapsConfirmPdu: version: 0x%04X flags: 0x%04X",
			capsSet.version, capsSet.flags);

	return 1;
}

int rdpgfx_send_frame_acknowledge_pdu(RDPGFX_CHANNEL_CALLBACK* callback, RDPGFX_FRAME_ACKNOWLEDGE_PDU* pdu)
{
	int status;
	wStream* s;
	RDPGFX_HEADER header;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_FRAMEACKNOWLEDGE;
	header.pduLength = RDPGFX_HEADER_SIZE + 12;

	WLog_Print(gfx->log, WLOG_DEBUG, "SendFrameAcknowledgePdu: %d", pdu->frameId);

	s = Stream_New(NULL, header.pduLength);

	rdpgfx_write_header(s, &header);

	/* RDPGFX_FRAME_ACKNOWLEDGE_PDU */

	Stream_Write_UINT32(s, pdu->queueDepth); /* queueDepth (4 bytes) */
	Stream_Write_UINT32(s, pdu->frameId); /* frameId (4 bytes) */
	Stream_Write_UINT32(s, pdu->totalFramesDecoded); /* totalFramesDecoded (4 bytes) */

	status = callback->channel->Write(callback->channel, (UINT32) Stream_Length(s), Stream_Buffer(s), NULL);

	Stream_Free(s, TRUE);

	return status;
}

int rdpgfx_recv_reset_graphics_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	int pad;
	UINT32 index;
	MONITOR_DEF* monitor;
	RDPGFX_RESET_GRAPHICS_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 12)
		return -1;

	Stream_Read_UINT32(s, pdu.width); /* width (4 bytes) */
	Stream_Read_UINT32(s, pdu.height); /* height (4 bytes) */
	Stream_Read_UINT32(s, pdu.monitorCount); /* monitorCount (4 bytes) */

	if (Stream_GetRemainingLength(s) < (pdu.monitorCount * 20))
		return -1;

	pdu.monitorDefArray = (MONITOR_DEF*) calloc(pdu.monitorCount, sizeof(MONITOR_DEF));

	if (!pdu.monitorDefArray)
		return -1;

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
		free(pdu.monitorDefArray);
		return -1;
	}

	Stream_Seek(s, pad); /* pad (total size is 340 bytes) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvResetGraphicsPdu: width: %d height: %d count: %d",
			pdu.width, pdu.height, pdu.monitorCount);

	if (context && context->ResetGraphics)
	{
		context->ResetGraphics(context, &pdu);
	}

	free(pdu.monitorDefArray);

	return 1;
}

int rdpgfx_recv_evict_cache_entry_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_EVICT_CACHE_ENTRY_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 2)
		return -1;

	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvEvictCacheEntryPdu: cacheSlot: %d", pdu.cacheSlot);

	if (context && context->EvictCacheEntry)
	{
		context->EvictCacheEntry(context, &pdu);
	}

	return 1;
}

int rdpgfx_recv_cache_import_reply_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_CACHE_IMPORT_REPLY_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 2)
		return -1;

	Stream_Read_UINT16(s, pdu.importedEntriesCount); /* cacheSlot (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (pdu.importedEntriesCount * 2))
		return -1;

	pdu.cacheSlots = (UINT16*) calloc(pdu.importedEntriesCount, sizeof(UINT16));

	if (!pdu.cacheSlots)
		return -1;

	for (index = 0; index < pdu.importedEntriesCount; index++)
	{
		Stream_Read_UINT16(s, pdu.cacheSlots[index]); /* cacheSlot (2 bytes) */
	}

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvCacheImportReplyPdu: importedEntriesCount: %d",
			pdu.importedEntriesCount);

	if (context && context->CacheImportReply)
	{
		context->CacheImportReply(context, &pdu);
	}

	free(pdu.cacheSlots);

	return 1;
}

int rdpgfx_recv_create_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CREATE_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 7)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.width); /* width (2 bytes) */
	Stream_Read_UINT16(s, pdu.height); /* height (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* RDPGFX_PIXELFORMAT (1 byte) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvCreateSurfacePdu: surfaceId: %d width: %d height: %d pixelFormat: 0x%02X",
			pdu.surfaceId, pdu.width, pdu.height, pdu.pixelFormat);

	if (context && context->CreateSurface)
	{
		context->CreateSurface(context, &pdu);
	}

	return 1;
}

int rdpgfx_recv_delete_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 2)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvDeleteSurfacePdu: surfaceId: %d", pdu.surfaceId);

	if (context && context->DeleteSurface)
	{
		context->DeleteSurface(context, &pdu);
	}

	return 1;
}

int rdpgfx_recv_start_frame_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_START_FRAME_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	Stream_Read_UINT32(s, pdu.timestamp); /* timestamp (4 bytes) */
	Stream_Read_UINT32(s, pdu.frameId); /* frameId (4 bytes) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvStartFramePdu: frameId: %d timestamp: 0x%04X",
			pdu.frameId, pdu.timestamp);

	if (context && context->StartFrame)
	{
		context->StartFrame(context, &pdu);
	}

	gfx->UnacknowledgedFrames++;

	return 1;
}

int rdpgfx_recv_end_frame_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_END_FRAME_PDU pdu;
	RDPGFX_FRAME_ACKNOWLEDGE_PDU ack;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, pdu.frameId); /* frameId (4 bytes) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvEndFramePdu: frameId: %d", pdu.frameId);

	if (context && context->EndFrame)
	{
		context->EndFrame(context, &pdu);
	}

	gfx->UnacknowledgedFrames--;
	gfx->TotalDecodedFrames++;

	ack.frameId = pdu.frameId;
	ack.totalFramesDecoded = gfx->TotalDecodedFrames;

	//ack.queueDepth = SUSPEND_FRAME_ACKNOWLEDGEMENT;
	ack.queueDepth = QUEUE_DEPTH_UNAVAILABLE;

	rdpgfx_send_frame_acknowledge_pdu(callback, &ack);

	return 1;
}

int rdpgfx_recv_wire_to_surface_1_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_WIRE_TO_SURFACE_PDU_1 pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 17)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId); /* codecId (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* pixelFormat (1 byte) */

	rdpgfx_read_rect16(s, &(pdu.destRect)); /* destRect (8 bytes) */

	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */

	if (pdu.bitmapDataLength > Stream_GetRemainingLength(s))
		return -1;

	pdu.bitmapData = Stream_Pointer(s);
	Stream_Seek(s, pdu.bitmapDataLength);

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvWireToSurface1Pdu: surfaceId: %d codecId: %s (0x%04X) pixelFormat: 0x%04X "
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

	if (cmd.codecId == RDPGFX_CODECID_H264)
	{
		rdpgfx_decode(gfx, &cmd);
	}
	else
	{
		if (context && context->SurfaceCommand)
		{
			context->SurfaceCommand(context, &cmd);
		}
	}

	return 1;
}

int rdpgfx_recv_wire_to_surface_2_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_WIRE_TO_SURFACE_PDU_2 pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 13)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId); /* codecId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId); /* codecContextId (4 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* pixelFormat (1 byte) */

	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */

	pdu.bitmapData = Stream_Pointer(s);
	Stream_Seek(s, pdu.bitmapDataLength);

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvWireToSurface2Pdu: surfaceId: %d codecId: 0x%04X "
			"codecContextId: %d pixelFormat: 0x%04X bitmapDataLength: %d",
			(int) pdu.surfaceId, pdu.codecId, pdu.codecContextId, pdu.pixelFormat, pdu.bitmapDataLength);

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

	if (context && context->SurfaceCommand)
	{
		context->SurfaceCommand(context, &cmd);
	}

	return 1;
}

int rdpgfx_recv_delete_encoding_context_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_ENCODING_CONTEXT_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 6)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId); /* codecContextId (4 bytes) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvDeleteEncodingContextPdu: surfaceId: %d codecContextId: %d",
			pdu.surfaceId, pdu.codecContextId);

	if (context && context->DeleteEncodingContext)
	{
		context->DeleteEncodingContext(context, &pdu);
	}

	return 1;
}

int rdpgfx_recv_solid_fill_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_RECT16* fillRect;
	RDPGFX_SOLID_FILL_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	rdpgfx_read_color32(s, &(pdu.fillPixel)); /* fillPixel (4 bytes) */
	Stream_Read_UINT16(s, pdu.fillRectCount); /* fillRectCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (pdu.fillRectCount * 8))
		return -1;

	pdu.fillRects = (RDPGFX_RECT16*) calloc(pdu.fillRectCount, sizeof(RDPGFX_RECT16));

	if (!pdu.fillRects)
		return -1;

	for (index = 0; index < pdu.fillRectCount; index++)
	{
		fillRect = &(pdu.fillRects[index]);
		rdpgfx_read_rect16(s, fillRect);
	}

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvSolidFillPdu: surfaceId: %d fillRectCount: %d",
			pdu.surfaceId, pdu.fillRectCount);

	if (context && context->SolidFill)
	{
		context->SolidFill(context, &pdu);
	}
	
	free(pdu.fillRects);

	return 1;
}

int rdpgfx_recv_surface_to_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_POINT16* destPt;
	RDPGFX_SURFACE_TO_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 14)
			return -1;

	Stream_Read_UINT16(s, pdu.surfaceIdSrc); /* surfaceIdSrc (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceIdDest); /* surfaceIdDest (2 bytes) */
	rdpgfx_read_rect16(s, &(pdu.rectSrc)); /* rectSrc (8 bytes ) */
	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (pdu.destPtsCount * 4))
			return -1;

	pdu.destPts = (RDPGFX_POINT16*) calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
		return -1;

	for (index = 0; index < pdu.destPtsCount; index++)
	{
		destPt = &(pdu.destPts[index]);
		rdpgfx_read_point16(s, destPt);
	}

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvSurfaceToSurfacePdu: surfaceIdSrc: %d surfaceIdDest: %d "
			"left: %d top: %d right: %d bottom: %d destPtsCount: %d",
			pdu.surfaceIdSrc, pdu.surfaceIdDest,
			pdu.rectSrc.left, pdu.rectSrc.top, pdu.rectSrc.right, pdu.rectSrc.bottom,
			pdu.destPtsCount);

	if (context && context->SurfaceToSurface)
	{
		context->SurfaceToSurface(context, &pdu);
	}

	free(pdu.destPts);

	return 1;
}

int rdpgfx_recv_surface_to_cache_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_TO_CACHE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 20)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.cacheKey); /* cacheKey (8 bytes) */
	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */
	rdpgfx_read_rect16(s, &(pdu.rectSrc)); /* rectSrc (8 bytes ) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvSurfaceToCachePdu: surfaceId: %d cacheKey: 0x%08X cacheSlot: %d "
			"left: %d top: %d right: %d bottom: %d",
			pdu.surfaceId, (int) pdu.cacheKey, pdu.cacheSlot,
			pdu.rectSrc.left, pdu.rectSrc.top,
			pdu.rectSrc.right, pdu.rectSrc.bottom);

	if (context && context->SurfaceToCache)
	{
		context->SurfaceToCache(context, &pdu);
	}

	return 1;
}

int rdpgfx_recv_cache_to_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_POINT16* destPt;
	RDPGFX_CACHE_TO_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 6)
		return -1;

	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t) (pdu.destPtsCount * 4))
		return -1;

	pdu.destPts = (RDPGFX_POINT16*) calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
		return -1;

	for (index = 0; index < pdu.destPtsCount; index++)
	{
		destPt = &(pdu.destPts[index]);
		rdpgfx_read_point16(s, destPt);
	}

	WLog_Print(gfx->log, WLOG_DEBUG, "RdpGfxRecvCacheToSurfacePdu: cacheSlot: %d surfaceId: %d destPtsCount: %d",
			pdu.cacheSlot, (int) pdu.surfaceId, pdu.destPtsCount);

	if (context && context->CacheToSurface)
	{
		context->CacheToSurface(context, &pdu);
	}

	free(pdu.destPts);

	return 1;
}

int rdpgfx_recv_map_surface_to_output_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 12)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.reserved); /* reserved (2 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginY); /* outputOriginY (4 bytes) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvMapSurfaceToOutputPdu: surfaceId: %d outputOriginX: %d outputOriginY: %d",
			(int) pdu.surfaceId, pdu.outputOriginX, pdu.outputOriginY);

	if (context && context->MapSurfaceToOutput)
	{
		context->MapSurfaceToOutput(context, &pdu);
	}

	return 1;
}

int rdpgfx_recv_map_surface_to_window_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_WINDOW_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	if (Stream_GetRemainingLength(s) < 18)
		return -1;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.windowId); /* windowId (8 bytes) */
	Stream_Read_UINT32(s, pdu.mappedWidth); /* mappedWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.mappedHeight); /* mappedHeight (4 bytes) */

	WLog_Print(gfx->log, WLOG_DEBUG, "RecvMapSurfaceToWindowPdu: surfaceId: %d windowId: 0x%04X mappedWidth: %d mappedHeight: %d",
			pdu.surfaceId, (int) pdu.windowId, pdu.mappedWidth, pdu.mappedHeight);

	if (context && context->MapSurfaceToWindow)
	{
		context->MapSurfaceToWindow(context, &pdu);
	}

	return 1;
}

int rdpgfx_recv_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	int status;
	int beg, end;
	RDPGFX_HEADER header;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	beg = Stream_GetPosition(s);

	status = rdpgfx_read_header(s, &header);

	if (status < 0)
		return -1;

#if 1
	WLog_Print(gfx->log, WLOG_DEBUG, "cmdId: %s (0x%04X) flags: 0x%04X pduLength: %d",
			rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId, header.flags, header.pduLength);
#endif

	switch (header.cmdId)
	{
		case RDPGFX_CMDID_WIRETOSURFACE_1:
			status = rdpgfx_recv_wire_to_surface_1_pdu(callback, s);
			break;

		case RDPGFX_CMDID_WIRETOSURFACE_2:
			status = rdpgfx_recv_wire_to_surface_2_pdu(callback, s);
			break;

		case RDPGFX_CMDID_DELETEENCODINGCONTEXT:
			status = rdpgfx_recv_delete_encoding_context_pdu(callback, s);
			break;

		case RDPGFX_CMDID_SOLIDFILL:
			status = rdpgfx_recv_solid_fill_pdu(callback, s);
			break;

		case RDPGFX_CMDID_SURFACETOSURFACE:
			status = rdpgfx_recv_surface_to_surface_pdu(callback, s);
			break;

		case RDPGFX_CMDID_SURFACETOCACHE:
			status = rdpgfx_recv_surface_to_cache_pdu(callback, s);
			break;

		case RDPGFX_CMDID_CACHETOSURFACE:
			status = rdpgfx_recv_cache_to_surface_pdu(callback, s);
			break;

		case RDPGFX_CMDID_EVICTCACHEENTRY:
			status = rdpgfx_recv_evict_cache_entry_pdu(callback, s);
			break;

		case RDPGFX_CMDID_CREATESURFACE:
			status = rdpgfx_recv_create_surface_pdu(callback, s);
			break;

		case RDPGFX_CMDID_DELETESURFACE:
			status = rdpgfx_recv_delete_surface_pdu(callback, s);
			break;

		case RDPGFX_CMDID_STARTFRAME:
			status = rdpgfx_recv_start_frame_pdu(callback, s);
			break;

		case RDPGFX_CMDID_ENDFRAME:
			status = rdpgfx_recv_end_frame_pdu(callback, s);
			break;

		case RDPGFX_CMDID_RESETGRAPHICS:
			status = rdpgfx_recv_reset_graphics_pdu(callback, s);
			break;

		case RDPGFX_CMDID_MAPSURFACETOOUTPUT:
			status = rdpgfx_recv_map_surface_to_output_pdu(callback, s);
			break;

		case RDPGFX_CMDID_CACHEIMPORTREPLY:
			status = rdpgfx_recv_cache_import_reply_pdu(callback, s);
			break;

		case RDPGFX_CMDID_CAPSCONFIRM:
			status = rdpgfx_recv_caps_confirm_pdu(callback, s);
			break;

		case RDPGFX_CMDID_MAPSURFACETOWINDOW:
			status = rdpgfx_recv_map_surface_to_window_pdu(callback, s);
			break;

		default:
			status = -1;
			break;
	}

	if (status < 0)
	{
		WLog_ERR(TAG,  "Error while parsing GFX cmdId: %s (0x%04X)",
				 rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId);
		return -1;
	}

	end = Stream_GetPosition(s);

	if (end != (beg + header.pduLength))
	{
		WLog_ERR(TAG,  "Unexpected gfx pdu end: Actual: %d, Expected: %d",
				 end, (beg + header.pduLength));
		Stream_SetPosition(s, (beg + header.pduLength));
	}

	return status;
}

static int rdpgfx_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	wStream* s;
	int status = 0;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	status = zgfx_decompress(gfx->zgfx, Stream_Pointer(data), Stream_GetRemainingLength(data), &pDstData, &DstSize, 0);

	if (status < 0)
	{
		WLog_DBG(TAG, "zgfx_decompress failure! status: %d", status);
		return 0;
	}

	s = Stream_New(pDstData, DstSize);

	while (((size_t) Stream_GetPosition(s)) < Stream_Length(s))
	{
		status = rdpgfx_recv_pdu(callback, s);

		if (status < 0)
			break;
	}

	Stream_Free(s, TRUE);
	
	//free(Stream_Buffer(data));
	//Stream_Free(data,TRUE);

	return status;
}

static int rdpgfx_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	WLog_Print(gfx->log, WLOG_DEBUG, "OnOpen");

	rdpgfx_send_caps_advertise_pdu(callback);

	return 0;
}

static int rdpgfx_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	WLog_Print(gfx->log, WLOG_DEBUG, "OnClose");

	free(callback);

	return 0;
}

static int rdpgfx_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback;
	RDPGFX_LISTENER_CALLBACK* listener_callback = (RDPGFX_LISTENER_CALLBACK*) pListenerCallback;

	callback = (RDPGFX_CHANNEL_CALLBACK*) calloc(1, sizeof(RDPGFX_CHANNEL_CALLBACK));

	if (!callback)
		return -1;

	callback->iface.OnDataReceived = rdpgfx_on_data_received;
	callback->iface.OnOpen = rdpgfx_on_open;
	callback->iface.OnClose = rdpgfx_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int rdpgfx_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	int status;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) pPlugin;

	gfx->listener_callback = (RDPGFX_LISTENER_CALLBACK*) calloc(1, sizeof(RDPGFX_LISTENER_CALLBACK));

	if (!gfx->listener_callback)
		return -1;

	gfx->listener_callback->iface.OnNewChannelConnection = rdpgfx_on_new_channel_connection;
	gfx->listener_callback->plugin = pPlugin;
	gfx->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, RDPGFX_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*) gfx->listener_callback, &(gfx->listener));

	gfx->listener->pInterface = gfx->iface.pInterface;

	WLog_Print(gfx->log, WLOG_DEBUG, "Initialize");

	return status;
}

static int rdpgfx_plugin_terminated(IWTSPlugin* pPlugin)
{
	int count;
	int index;
	ULONG_PTR* pKeys = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) pPlugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	WLog_Print(gfx->log, WLOG_DEBUG, "Terminated");

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

		if (context && context->DeleteSurface)
		{
			context->DeleteSurface(context, &pdu);
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

			if (context && context->EvictCacheEntry)
			{
				context->EvictCacheEntry(context, &pdu);
			}

			gfx->CacheSlots[index] = NULL;
		}
	}

	free(context);

	free(gfx);

	return 0;
}

int rdpgfx_set_surface_data(RdpgfxClientContext* context, UINT16 surfaceId, void* pData)
{
	ULONG_PTR key;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	key = ((ULONG_PTR) surfaceId) + 1;

	if (pData)
		HashTable_Add(gfx->SurfaceTable, (void*) key, pData);
	else
		HashTable_Remove(gfx->SurfaceTable, (void*) key);

	return 1;
}

int rdpgfx_get_surface_ids(RdpgfxClientContext* context, UINT16** ppSurfaceIds)
{
	int count;
	int index;
	UINT16* pSurfaceIds;
	ULONG_PTR* pKeys = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	count = HashTable_GetKeys(gfx->SurfaceTable, &pKeys);

	if (count < 1)
		return 0;

	pSurfaceIds = (UINT16*) malloc(count * sizeof(UINT16));

	if (!pSurfaceIds)
		return -1;

	for (index = 0; index < count; index++)
	{
		pSurfaceIds[index] = pKeys[index] - 1;
	}

	free(pKeys);
	*ppSurfaceIds = pSurfaceIds;

	return count;
}

void* rdpgfx_get_surface_data(RdpgfxClientContext* context, UINT16 surfaceId)
{
	ULONG_PTR key;
	void* pData = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	key = ((ULONG_PTR) surfaceId) + 1;

	pData = HashTable_GetItemValue(gfx->SurfaceTable, (void*) key);

	return pData;
}

int rdpgfx_set_cache_slot_data(RdpgfxClientContext* context, UINT16 cacheSlot, void* pData)
{
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) context->handle;

	if (cacheSlot >= gfx->MaxCacheSlot)
		return -1;

	gfx->CacheSlots[cacheSlot] = pData;

	return 1;
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

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int status = 0;
	RDPGFX_PLUGIN* gfx;
	RdpgfxClientContext* context;

	gfx = (RDPGFX_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "rdpgfx");

	if (!gfx)
	{
		gfx = (RDPGFX_PLUGIN*) calloc(1, sizeof(RDPGFX_PLUGIN));

		if (!gfx)
			return -1;

		gfx->log = WLog_Get(TAG);
		gfx->settings = (rdpSettings*) pEntryPoints->GetRdpSettings(pEntryPoints);

		gfx->iface.Initialize = rdpgfx_plugin_initialize;
		gfx->iface.Connected = NULL;
		gfx->iface.Disconnected = NULL;
		gfx->iface.Terminated = rdpgfx_plugin_terminated;

		gfx->SurfaceTable = HashTable_New(TRUE);

		if (!gfx->SurfaceTable)
		{
			free (gfx);
			return -1;
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
			return -1;
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
			return -1;
		}

		status = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpgfx", (IWTSPlugin*) gfx);
	}

	return status;
}
