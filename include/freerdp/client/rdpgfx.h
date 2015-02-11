/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

typedef int (*pcRdpgfxResetGraphics)(RdpgfxClientContext* context, RDPGFX_RESET_GRAPHICS_PDU* resetGraphics);
typedef int (*pcRdpgfxStartFrame)(RdpgfxClientContext* context, RDPGFX_START_FRAME_PDU* startFrame);
typedef int (*pcRdpgfxEndFrame)(RdpgfxClientContext* context, RDPGFX_END_FRAME_PDU* endFrame);
typedef int (*pcRdpgfxSurfaceCommand)(RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd);
typedef int (*pcRdpgfxDeleteEncodingContext)(RdpgfxClientContext* context, RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext);
typedef int (*pcRdpgfxCreateSurface)(RdpgfxClientContext* context, RDPGFX_CREATE_SURFACE_PDU* createSurface);
typedef int (*pcRdpgfxDeleteSurface)(RdpgfxClientContext* context, RDPGFX_DELETE_SURFACE_PDU* deleteSurface);
typedef int (*pcRdpgfxSolidFill)(RdpgfxClientContext* context, RDPGFX_SOLID_FILL_PDU* solidFill);
typedef int (*pcRdpgfxSurfaceToSurface)(RdpgfxClientContext* context, RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface);
typedef int (*pcRdpgfxSurfaceToCache)(RdpgfxClientContext* context, RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache);
typedef int (*pcRdpgfxCacheToSurface)(RdpgfxClientContext* context, RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface);
typedef int (*pcRdpgfxCacheImportOffer)(RdpgfxClientContext* context, RDPGFX_CACHE_IMPORT_OFFER_PDU* cacheImportOffer);
typedef int (*pcRdpgfxCacheImportReply)(RdpgfxClientContext* context, RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply);
typedef int (*pcRdpgfxEvictCacheEntry)(RdpgfxClientContext* context, RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry);
typedef int (*pcRdpgfxMapSurfaceToOutput)(RdpgfxClientContext* context, RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput);
typedef int (*pcRdpgfxMapSurfaceToWindow)(RdpgfxClientContext* context, RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow);

typedef int (*pcRdpgfxSetSurfaceData)(RdpgfxClientContext* context, UINT16 surfaceId, void* pData);
typedef void* (*pcRdpgfxGetSurfaceData)(RdpgfxClientContext* context, UINT16 surfaceId);
typedef int (*pcRdpgfxGetSurfaceIds)(RdpgfxClientContext* context, UINT16** ppSurfaceIds);
typedef int (*pcRdpgfxSetCacheSlotData)(RdpgfxClientContext* context, UINT16 cacheSlot, void* pData);
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
