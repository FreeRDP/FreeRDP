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

#ifndef FREERDP_CHANNEL_SERVER_RDPGFX_H
#define FREERDP_CHANNEL_SERVER_RDPGFX_H

#include <freerdp/channels/rdpgfx.h>

typedef struct _rdpgfx_server_context RdpgfxServerContext;
typedef struct _rdpgfx_server_private RdpgfxServerPrivate;

typedef BOOL (*psRdpgfxServerOpen)(RdpgfxServerContext* context);
typedef BOOL (*psRdpgfxServerClose)(RdpgfxServerContext* context);

typedef UINT (*pcRdpgfxResetGraphics)(RdpgfxServerContext* context, RDPGFX_RESET_GRAPHICS_PDU* resetGraphics);
typedef UINT (*pcRdpgfxStartFrame)(RdpgfxServerContext* context, RDPGFX_START_FRAME_PDU* startFrame);
typedef UINT (*pcRdpgfxEndFrame)(RdpgfxServerContext* context, RDPGFX_END_FRAME_PDU* endFrame);
typedef UINT (*pcRdpgfxSurfaceCommand)(RdpgfxServerContext* context, RDPGFX_SURFACE_COMMAND* cmd);
typedef UINT (*pcRdpgfxDeleteEncodingContext)(RdpgfxServerContext* context, RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext);
typedef UINT (*pcRdpgfxCreateSurface)(RdpgfxServerContext* context, RDPGFX_CREATE_SURFACE_PDU* createSurface);
typedef UINT (*pcRdpgfxDeleteSurface)(RdpgfxServerContext* context, RDPGFX_DELETE_SURFACE_PDU* deleteSurface);
typedef UINT (*pcRdpgfxSolidFill)(RdpgfxServerContext* context, RDPGFX_SOLID_FILL_PDU* solidFill);
typedef UINT (*pcRdpgfxSurfaceToSurface)(RdpgfxServerContext* context, RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface);
typedef UINT (*pcRdpgfxSurfaceToCache)(RdpgfxServerContext* context, RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache);
typedef UINT (*pcRdpgfxCacheToSurface)(RdpgfxServerContext* context, RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface);
typedef UINT (*pcRdpgfxCacheImportOffer)(RdpgfxServerContext* context, RDPGFX_CACHE_IMPORT_OFFER_PDU* cacheImportOffer);
typedef UINT (*pcRdpgfxCacheImportReply)(RdpgfxServerContext* context, RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply);
typedef UINT (*pcRdpgfxEvictCacheEntry)(RdpgfxServerContext* context, RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry);
typedef UINT (*pcRdpgfxMapSurfaceToOutput)(RdpgfxServerContext* context, RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput);
typedef UINT (*pcRdpgfxMapSurfaceToWindow)(RdpgfxServerContext* context, RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow);
typedef UINT (*pcRdpgfxCapsAdvertise)(RdpgfxServerContext* context, RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise);
typedef UINT (*pcRdpgfxCapsConfirm)(RdpgfxServerContext* context, RDPGFX_CAPS_CONFIRM_PDU* capsConfirm);
typedef UINT (*pcRdpgfxFrameAcknowledge)(RdpgfxServerContext* context, RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge);
typedef UINT (*pcRdpgfxQoeFrameAcknowledge)(RdpgfxServerContext* context, RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU* qoeFrameAcknowledge);

struct _rdpgfx_server_context
{
	HANDLE vcm;
	void* custom;

	psRdpgfxServerOpen Open;
	psRdpgfxServerClose Close;

	pcRdpgfxResetGraphics ResetGraphics;
	pcRdpgfxStartFrame StartFrame;
	pcRdpgfxEndFrame EndFrame;
	pcRdpgfxSurfaceCommand SurfaceCommand;
	pcRdpgfxDeleteEncodingContext DeleteEncodingContext;
	pcRdpgfxCreateSurface CreateSurface;
	pcRdpgfxDeleteSurface DeleteSurface;
	pcRdpgfxSolidFill SolidFill;
	pcRdpgfxSurfaceToSurface SurfaceToSurface;
	pcRdpgfxSurfaceToCache SurfaceToCache;
	pcRdpgfxCacheToSurface CacheToSurface;
	pcRdpgfxCacheImportOffer CacheImportOffer;
	pcRdpgfxCacheImportReply CacheImportReply;
	pcRdpgfxEvictCacheEntry EvictCacheEntry;
	pcRdpgfxMapSurfaceToOutput MapSurfaceToOutput;
	pcRdpgfxMapSurfaceToWindow MapSurfaceToWindow;
	pcRdpgfxCapsAdvertise CapsAdvertise;
	pcRdpgfxCapsConfirm CapsConfirm;
	pcRdpgfxFrameAcknowledge FrameAcknowledge;
	pcRdpgfxQoeFrameAcknowledge QoeFrameAcknowledge;

	RdpgfxServerPrivate* priv;
	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API RdpgfxServerContext* rdpgfx_server_context_new(HANDLE vcm);
FREERDP_API void rdpgfx_server_context_free(RdpgfxServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_SERVER_RDPGFX_H */
