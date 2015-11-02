/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_CLIENT_RDPGFX_H
#define FREERDP_CHANNEL_CLIENT_RDPGFX_H

#include <freerdp/channels/rdpgfx.h>

/**
 * Client Interface
 */

typedef struct _rdpgfx_client_context RdpgfxClientContext;

typedef UINT (*pcRdpgfxResetGraphics)(RdpgfxClientContext* context, RDPGFX_RESET_GRAPHICS_PDU* resetGraphics);
typedef UINT (*pcRdpgfxStartFrame)(RdpgfxClientContext* context, RDPGFX_START_FRAME_PDU* startFrame);
typedef UINT (*pcRdpgfxEndFrame)(RdpgfxClientContext* context, RDPGFX_END_FRAME_PDU* endFrame);
typedef UINT (*pcRdpgfxSurfaceCommand)(RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd);
typedef UINT (*pcRdpgfxDeleteEncodingContext)(RdpgfxClientContext* context, RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext);
typedef UINT (*pcRdpgfxCreateSurface)(RdpgfxClientContext* context, RDPGFX_CREATE_SURFACE_PDU* createSurface);
typedef UINT (*pcRdpgfxDeleteSurface)(RdpgfxClientContext* context, RDPGFX_DELETE_SURFACE_PDU* deleteSurface);
typedef UINT (*pcRdpgfxSolidFill)(RdpgfxClientContext* context, RDPGFX_SOLID_FILL_PDU* solidFill);
typedef UINT (*pcRdpgfxSurfaceToSurface)(RdpgfxClientContext* context, RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface);
typedef UINT (*pcRdpgfxSurfaceToCache)(RdpgfxClientContext* context, RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache);
typedef UINT (*pcRdpgfxCacheToSurface)(RdpgfxClientContext* context, RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface);
typedef UINT (*pcRdpgfxCacheImportOffer)(RdpgfxClientContext* context, RDPGFX_CACHE_IMPORT_OFFER_PDU* cacheImportOffer);
typedef UINT (*pcRdpgfxCacheImportReply)(RdpgfxClientContext* context, RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply);
typedef UINT (*pcRdpgfxEvictCacheEntry)(RdpgfxClientContext* context, RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry);
typedef UINT (*pcRdpgfxMapSurfaceToOutput)(RdpgfxClientContext* context, RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput);
typedef UINT (*pcRdpgfxMapSurfaceToWindow)(RdpgfxClientContext* context, RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow);

typedef UINT (*pcRdpgfxSetSurfaceData)(RdpgfxClientContext* context, UINT16 surfaceId, void* pData);
typedef void* (*pcRdpgfxGetSurfaceData)(RdpgfxClientContext* context, UINT16 surfaceId);
typedef UINT (*pcRdpgfxGetSurfaceIds)(RdpgfxClientContext* context, UINT16** ppSurfaceIds, UINT16* count);
typedef UINT (*pcRdpgfxSetCacheSlotData)(RdpgfxClientContext* context, UINT16 cacheSlot, void* pData);
typedef void* (*pcRdpgfxGetCacheSlotData)(RdpgfxClientContext* context, UINT16 cacheSlot);

struct _rdpgfx_client_context
{
	void* handle;
	void* custom;

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

	pcRdpgfxGetSurfaceIds GetSurfaceIds;
	pcRdpgfxSetSurfaceData SetSurfaceData;
	pcRdpgfxGetSurfaceData GetSurfaceData;
	pcRdpgfxSetCacheSlotData SetCacheSlotData;
	pcRdpgfxGetCacheSlotData GetCacheSlotData;
};

#endif /* FREERDP_CHANNEL_CLIENT_RDPGFX_H */
