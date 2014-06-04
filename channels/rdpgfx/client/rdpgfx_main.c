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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	gfx->ThinClient = TRUE;
	gfx->SmallCache = FALSE;
	gfx->H264 = FALSE;

	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_CAPSADVERTISE;

	pdu.capsSetCount = 2;
	pdu.capsSets = (RDPGFX_CAPSET*) capsSets;

	capsSet = &capsSets[0];

	capsSet->version = RDPGFX_CAPVERSION_8;
	capsSet->flags = 0;

	if (gfx->ThinClient)
		capsSet->flags |= RDPGFX_CAPS_FLAG_THINCLIENT;

	if (gfx->SmallCache)
		capsSet->flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

	capsSet = &capsSets[1];

	capsSet->version = RDPGFX_CAPVERSION_81;
	capsSet->flags = 0;

	if (gfx->ThinClient)
		capsSet->flags |= RDPGFX_CAPS_FLAG_THINCLIENT;

	if (gfx->SmallCache)
		capsSet->flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

	if (gfx->H264)
		capsSet->flags |= RDPGFX_CAPS_FLAG_H264ENABLED;

	header.pduLength = RDPGFX_HEADER_SIZE + 2 + (pdu.capsSetCount * RDPGFX_CAPSET_SIZE);

	fprintf(stderr, "RdpGfxSendCapsAdvertisePdu: %d\n", header.pduLength);

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

	pdu.capsSet = &capsSet;

	Stream_Read_UINT32(s, capsSet.version); /* version (4 bytes) */
	Stream_Read_UINT32(s, capsDataLength); /* capsDataLength (4 bytes) */
	Stream_Read_UINT32(s, capsSet.flags); /* capsData (4 bytes) */

	fprintf(stderr, "RdpGfxRecvCapsConfirmPdu: version: 0x%04X flags: 0x%04X\n",
			capsSet.version, capsSet.flags);

	return 1;
}

int rdpgfx_send_frame_acknowledge_pdu(RDPGFX_CHANNEL_CALLBACK* callback, RDPGFX_FRAME_ACKNOWLEDGE_PDU* pdu)
{
	int status;
	wStream* s;
	RDPGFX_PLUGIN* gfx;
	RDPGFX_HEADER header;

	gfx = (RDPGFX_PLUGIN*) callback->plugin;

	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_FRAMEACKNOWLEDGE;
	header.pduLength = RDPGFX_HEADER_SIZE + 12;

	fprintf(stderr, "RdpGfxSendFrameAcknowledgePdu: %d\n", pdu->frameId);

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
	UINT32 index;
	MONITOR_DEF* monitor;
	RDPGFX_RESET_GRAPHICS_PDU pdu;

	Stream_Read_UINT32(s, pdu.width); /* width (4 bytes) */
	Stream_Read_UINT32(s, pdu.height); /* height (4 bytes) */
	Stream_Read_UINT32(s, pdu.monitorCount); /* monitorCount (4 bytes) */

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

	/* pad (total size is 340 bytes) */

	fprintf(stderr, "RdpGfxRecvResetGraphicsPdu: width: %d height: %d count: %d\n",
			pdu.width, pdu.height, pdu.monitorCount);

	return 1;
}

int rdpgfx_recv_evict_cache_entry_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_EVICT_CACHE_ENTRY_PDU pdu;

	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */

	fprintf(stderr, "RdpGfxRecvEvictCacheEntryPdu: cacheSlot: %d\n", pdu.cacheSlot);

	return 1;
}

int rdpgfx_recv_cache_import_reply_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_CACHE_IMPORT_REPLY_PDU pdu;

	Stream_Read_UINT16(s, pdu.importedEntriesCount); /* cacheSlot (2 bytes) */

	pdu.cacheSlots = (UINT16*) calloc(pdu.importedEntriesCount, sizeof(UINT16));

	if (!pdu.cacheSlots)
		return -1;

	for (index = 0; index < pdu.importedEntriesCount; index++)
	{
		Stream_Read_UINT16(s, pdu.cacheSlots[index]); /* cacheSlot (2 bytes) */
	}

	fprintf(stderr, "RdpGfxRecvCacheImportReplyPdu: importedEntriesCount: %d\n",
			pdu.importedEntriesCount);

	return 1;
}

int rdpgfx_recv_create_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CREATE_SURFACE_PDU pdu;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.width); /* width (2 bytes) */
	Stream_Read_UINT16(s, pdu.height); /* height (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* RDPGFX_PIXELFORMAT (1 byte) */

	fprintf(stderr, "RdpGfxRecvCreateSurfacePdu: surfaceId: %d width: %d height: %d pixelFormat: %d\n",
			pdu.surfaceId, pdu.width, pdu.height, pdu.pixelFormat);

	return 1;
}

int rdpgfx_recv_delete_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_SURFACE_PDU pdu;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */

	fprintf(stderr, "RdpGfxRecvDeleteSurfacePdu: surfaceId: %d\n", pdu.surfaceId);

	return 1;
}

int rdpgfx_recv_start_frame_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_START_FRAME_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	Stream_Read_UINT32(s, pdu.timestamp); /* timestamp (4 bytes) */
	Stream_Read_UINT32(s, pdu.frameId); /* frameId (4 bytes) */

	fprintf(stderr, "RdpGfxRecvStartFramePdu: frameId: %d timestamp: 0x%04X\n",
			pdu.frameId, pdu.timestamp);

	gfx->UnacknowledgedFrames++;

	return 1;
}

int rdpgfx_recv_end_frame_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_END_FRAME_PDU pdu;
	RDPGFX_FRAME_ACKNOWLEDGE_PDU ack;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	Stream_Read_UINT32(s, pdu.frameId); /* frameId (4 bytes) */

	fprintf(stderr, "RdpGfxRecvEndFramePdu: frameId: %d\n", pdu.frameId);

	gfx->UnacknowledgedFrames--;
	gfx->TotalDecodedFrames++;

	ack.frameId = pdu.frameId;
	ack.totalFramesDecoded = gfx->TotalDecodedFrames;

	ack.queueDepth = SUSPEND_FRAME_ACKNOWLEDGEMENT;
	//ack.queueDepth = QUEUE_DEPTH_UNAVAILABLE;
	//ack.queueDepth = gfx->UnacknowledgedFrames;

	rdpgfx_send_frame_acknowledge_pdu(callback, &ack);

	return 1;
}

int rdpgfx_recv_wire_to_surface_1_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_WIRE_TO_SURFACE_PDU_1 pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId); /* codecId (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* pixelFormat (1 byte) */

	rdpgfx_read_rect16(s, &(pdu.destRect)); /* destRect (8 bytes) */

	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */

	pdu.bitmapData = Stream_Pointer(s);

	fprintf(stderr, "RdpGfxRecvWireToSurface1Pdu: surfaceId: %d codecId: %s (0x%04X) pixelFormat: 0x%04X "
			"destRect: left: %d top: %d right: %d bottom: %d bitmapDataLength: %d\n",
			pdu.surfaceId, rdpgfx_get_codec_id_string(pdu.codecId), pdu.codecId, pdu.pixelFormat,
			pdu.destRect.left, pdu.destRect.top, pdu.destRect.right, pdu.destRect.bottom,
			pdu.bitmapDataLength);

	cmd.surfaceId = pdu.surfaceId;
	cmd.codecId = pdu.codecId;
	cmd.codecContextId = 0;
	cmd.pixelFormat = pdu.pixelFormat;
	rdpgfx_copy_rect16(&(cmd.destRect), &(pdu.destRect));
	cmd.bitmapDataLength = pdu.bitmapDataLength;
	cmd.bitmapData = pdu.bitmapData;

	rdpgfx_decode(gfx, &cmd);

	return 1;
}

int rdpgfx_recv_wire_to_surface_2_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_WIRE_TO_SURFACE_PDU_2 pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*) callback->plugin;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId); /* codecId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId); /* codecContextId (4 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* pixelFormat (1 byte) */

	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */

	pdu.bitmapData = Stream_Pointer(s);

	fprintf(stderr, "RdpGfxRecvWireToSurface2Pdu: surfaceId: %d codecId: 0x%04X "
			"codecContextId: %d pixelFormat: 0x%04X bitmapDataLength: %d\n",
			pdu.surfaceId, pdu.codecId, pdu.codecContextId, pdu.pixelFormat, pdu.bitmapDataLength);

	cmd.surfaceId = pdu.surfaceId;
	cmd.codecId = pdu.codecId;
	cmd.codecContextId = pdu.codecContextId;
	cmd.pixelFormat = pdu.pixelFormat;
	ZeroMemory(&(cmd.destRect), sizeof(RDPGFX_RECT16));
	cmd.bitmapDataLength = pdu.bitmapDataLength;
	cmd.bitmapData = pdu.bitmapData;

	rdpgfx_decode(gfx, &cmd);

	return 1;
}

int rdpgfx_recv_delete_encoding_context_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_ENCODING_CONTEXT_PDU pdu;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId); /* codecContextId (4 bytes) */

	fprintf(stderr, "RdpGfxRecvDeleteEncodingContextPdu: surfaceId: %d codecContextId: %d\n",
			pdu.surfaceId, pdu.codecContextId);

	return 1;
}

int rdpgfx_recv_solid_fill_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_RECT16* fillRect;
	RDPGFX_SOLID_FILL_PDU pdu;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */

	rdpgfx_read_color32(s, &(pdu.fillPixel)); /* fillPixel (4 bytes) */

	Stream_Read_UINT16(s, pdu.fillRectCount); /* fillRectCount (2 bytes) */

	pdu.fillRects = (RDPGFX_RECT16*) calloc(pdu.fillRectCount, sizeof(RDPGFX_RECT16));

	if (!pdu.fillRects)
		return -1;

	for (index = 0; index < pdu.fillRectCount; index++)
	{
		fillRect = &(pdu.fillRects[index]);
		rdpgfx_read_rect16(s, fillRect);
	}

	fprintf(stderr, "RdpGfxRecvSolidFillPdu: surfaceId: %d fillRectCount: %d\n",
			pdu.surfaceId, pdu.fillRectCount);

	return 1;
}

int rdpgfx_recv_surface_to_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_POINT16* destPt;
	RDPGFX_SURFACE_TO_SURFACE_PDU pdu;

	Stream_Read_UINT16(s, pdu.surfaceIdSrc); /* surfaceIdSrc (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceIdDest); /* surfaceIdDest (2 bytes) */

	rdpgfx_read_rect16(s, &(pdu.rectSrc)); /* rectSrc (8 bytes ) */

	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	pdu.destPts = (RDPGFX_POINT16*) calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
		return -1;

	for (index = 0; index < pdu.destPtsCount; index++)
	{
		destPt = &(pdu.destPts[index]);
		rdpgfx_read_point16(s, destPt);
	}

	fprintf(stderr, "RdpGfxRecvSurfaceToSurfacePdu: surfaceIdSrc: %d surfaceIdDest: %d "
			"left: %d top: %d right: %d bottom: %d destPtsCount: %d\n",
			pdu.surfaceIdSrc, pdu.surfaceIdDest,
			pdu.rectSrc.left, pdu.rectSrc.top, pdu.rectSrc.right, pdu.rectSrc.bottom,
			pdu.destPtsCount);

	return 1;
}

int rdpgfx_recv_surface_to_cache_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_TO_CACHE_PDU pdu;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.cacheKey); /* cacheKey (8 bytes) */
	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */
	rdpgfx_read_rect16(s, &(pdu.rectSrc)); /* rectSrc (8 bytes ) */

	fprintf(stderr, "RdpGfxRecvSurfaceToCachePdu: surfaceId: %d cacheKey: 0x%08X cacheSlot: %d "
			"left: %d top: %d right: %d bottom: %d\n",
			pdu.surfaceId, pdu.cacheKey, pdu.cacheSlot,
			pdu.rectSrc.left, pdu.rectSrc.top,
			pdu.rectSrc.right, pdu.rectSrc.bottom);

	return 1;
}

int rdpgfx_recv_cache_to_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_POINT16* destPt;
	RDPGFX_CACHE_TO_SURFACE_PDU pdu;

	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	pdu.destPts = (RDPGFX_POINT16*) calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
		return -1;

	for (index = 0; index < pdu.destPtsCount; index++)
	{
		destPt = &(pdu.destPts[index]);
		rdpgfx_read_point16(s, destPt);
	}

	fprintf(stderr, "RdpGfxRecvCacheToSurfacePdu: cacheSlot: %d surfaceId: %d destPtsCount: %d\n",
			pdu.cacheSlot, pdu.surfaceId, pdu.destPtsCount);

	return 1;
}

int rdpgfx_recv_map_surface_to_output_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU pdu;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.reserved); /* reserved (2 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginY); /* outputOriginY (4 bytes) */

	fprintf(stderr, "RdpGfxRecvMapSurfaceToOutputPdu: surfaceId: %d outputOriginX: %d outputOriginY: %d\n",
			pdu.surfaceId, pdu.outputOriginX, pdu.outputOriginY);

	return 1;
}

int rdpgfx_recv_map_surface_to_window_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_WINDOW_PDU pdu;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.windowId); /* windowId (8 bytes) */
	Stream_Read_UINT32(s, pdu.mappedWidth); /* mappedWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.mappedHeight); /* mappedHeight (4 bytes) */

	fprintf(stderr, "RdpGfxRecvMapSurfaceToWindowPdu: surfaceId: %d windowId: 0x%04X mappedWidth: %d mappedHeight: %d\n",
			pdu.surfaceId, pdu.windowId, pdu.mappedWidth, pdu.mappedHeight);

	return 1;
}

int rdpgfx_recv_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	int status;
	RDPGFX_HEADER header;

	rdpgfx_read_header(s, &header);

#if 1
	printf("cmdId: %s (0x%04X) flags: 0x%04X pduLength: %d\n",
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
			fprintf(stderr, "Unknown GFX cmdId: 0x%04X\n", header.cmdId);
			break;
	}

	return 0;
}

static int rdpgfx_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, UINT32 cbSize, BYTE* pBuffer)
{
	wStream* s;
	int status = 0;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	RDPGFX_PLUGIN* gfx = NULL;
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	gfx = (RDPGFX_PLUGIN*) callback->plugin;

	status = zgfx_decompress(gfx->zgfx, pBuffer, cbSize, &pDstData, &DstSize, 0);

	if (status < 0)
	{
		printf("zgfx_decompress failure! status: %d\n", status);
		return 0;
	}

	s = Stream_New(pDstData, DstSize);

	status = rdpgfx_recv_pdu(callback, s);

	Stream_Free(s, TRUE);

	return status;
}

static int rdpgfx_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	fprintf(stderr, "RdpGfxOnOpen\n");

	rdpgfx_send_caps_advertise_pdu(callback);

	return 0;
}

static int rdpgfx_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*) pChannelCallback;

	fprintf(stderr, "RdpGfxOnClose\n");

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

	callback->iface.OnDataReceived = rdpgfx_on_data_received;
	callback->iface.OnOpen = rdpgfx_on_open;
	callback->iface.OnClose = rdpgfx_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	fprintf(stderr, "RdpGfxOnNewChannelConnection\n");

	return 0;
}

static int rdpgfx_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	int status;
	RDPGFX_PLUGIN* rdpgfx = (RDPGFX_PLUGIN*) pPlugin;

	rdpgfx->listener_callback = (RDPGFX_LISTENER_CALLBACK*) calloc(1, sizeof(RDPGFX_LISTENER_CALLBACK));

	if (!rdpgfx->listener_callback)
		return -1;

	rdpgfx->listener_callback->iface.OnNewChannelConnection = rdpgfx_on_new_channel_connection;
	rdpgfx->listener_callback->plugin = pPlugin;
	rdpgfx->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, RDPGFX_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*) rdpgfx->listener_callback, &(rdpgfx->listener));

	rdpgfx->listener->pInterface = rdpgfx->iface.pInterface;

	fprintf(stderr, "RdpGfxInitialize: %d\n", status);

	return status;
}

static int rdpgfx_plugin_terminated(IWTSPlugin* pPlugin)
{
	RDPGFX_PLUGIN* rdpgfx = (RDPGFX_PLUGIN*) pPlugin;

	if (rdpgfx->listener_callback)
		free(rdpgfx->listener_callback);

	zgfx_context_free(rdpgfx->zgfx);

	free(rdpgfx);

	return 0;
}

/**
 * Channel Client Interface
 */

UINT32 rdpgfx_get_version(RdpgfxClientContext* context)
{
	//RDPGFX_PLUGIN* rdpgfx = (RDPGFX_PLUGIN*) context->handle;
	return 0;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		rdpgfx_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int error = 0;
	RDPGFX_PLUGIN* rdpgfx;
	RdpgfxClientContext* context;

	rdpgfx = (RDPGFX_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "rdpgfx");

	if (!rdpgfx)
	{
		rdpgfx = (RDPGFX_PLUGIN*) calloc(1, sizeof(RDPGFX_PLUGIN));

		if (!rdpgfx)
			return -1;

		rdpgfx->iface.Initialize = rdpgfx_plugin_initialize;
		rdpgfx->iface.Connected = NULL;
		rdpgfx->iface.Disconnected = NULL;
		rdpgfx->iface.Terminated = rdpgfx_plugin_terminated;

		context = (RdpgfxClientContext*) calloc(1, sizeof(RdpgfxClientContext));

		if (!context)
			return -1;

		context->handle = (void*) rdpgfx;
		context->GetVersion = rdpgfx_get_version;

		rdpgfx->iface.pInterface = (void*) context;

		rdpgfx->zgfx = zgfx_context_new(FALSE);

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpgfx", (IWTSPlugin*) rdpgfx);
	}

	return error;
}
