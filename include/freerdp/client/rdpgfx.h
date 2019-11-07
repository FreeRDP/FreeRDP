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

#ifndef FREERDP_CHANNEL_RDPGFX_CLIENT_RDPGFX_H
#define FREERDP_CHANNEL_RDPGFX_CLIENT_RDPGFX_H

#include <freerdp/channels/rdpgfx.h>
#include <freerdp/utils/profiler.h>

/**
 * Client Interface
 */

typedef struct _rdpgfx_client_context RdpgfxClientContext;

typedef UINT (*pcRdpgfxResetGraphics)(RdpgfxClientContext* context,
                                      const RDPGFX_RESET_GRAPHICS_PDU* resetGraphics);
typedef UINT (*pcRdpgfxStartFrame)(RdpgfxClientContext* context,
                                   const RDPGFX_START_FRAME_PDU* startFrame);
typedef UINT (*pcRdpgfxEndFrame)(RdpgfxClientContext* context,
                                 const RDPGFX_END_FRAME_PDU* endFrame);
typedef UINT (*pcRdpgfxSurfaceCommand)(RdpgfxClientContext* context,
                                       const RDPGFX_SURFACE_COMMAND* cmd);
typedef UINT (*pcRdpgfxDeleteEncodingContext)(
    RdpgfxClientContext* context, const RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext);
typedef UINT (*pcRdpgfxCreateSurface)(RdpgfxClientContext* context,
                                      const RDPGFX_CREATE_SURFACE_PDU* createSurface);
typedef UINT (*pcRdpgfxDeleteSurface)(RdpgfxClientContext* context,
                                      const RDPGFX_DELETE_SURFACE_PDU* deleteSurface);
typedef UINT (*pcRdpgfxSolidFill)(RdpgfxClientContext* context,
                                  const RDPGFX_SOLID_FILL_PDU* solidFill);
typedef UINT (*pcRdpgfxSurfaceToSurface)(RdpgfxClientContext* context,
                                         const RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface);
typedef UINT (*pcRdpgfxSurfaceToCache)(RdpgfxClientContext* context,
                                       const RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache);
typedef UINT (*pcRdpgfxCacheToSurface)(RdpgfxClientContext* context,
                                       const RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface);
typedef UINT (*pcRdpgfxCacheImportOffer)(RdpgfxClientContext* context,
                                         const RDPGFX_CACHE_IMPORT_OFFER_PDU* cacheImportOffer);
typedef UINT (*pcRdpgfxCacheImportReply)(RdpgfxClientContext* context,
                                         const RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply);
typedef UINT (*pcRdpgfxEvictCacheEntry)(RdpgfxClientContext* context,
                                        const RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry);
typedef UINT (*pcRdpgfxMapSurfaceToOutput)(RdpgfxClientContext* context,
                                           const RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput);
typedef UINT (*pcRdpgfxMapSurfaceToScaledOutput)(
    RdpgfxClientContext* context, const RDPGFX_MAP_SURFACE_TO_SCALED_OUTPUT_PDU* surfaceToOutput);
typedef UINT (*pcRdpgfxMapSurfaceToWindow)(RdpgfxClientContext* context,
                                           const RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow);
typedef UINT (*pcRdpgfxMapSurfaceToScaledWindow)(
    RdpgfxClientContext* context, const RDPGFX_MAP_SURFACE_TO_SCALED_WINDOW_PDU* surfaceToWindow);
typedef UINT (*pcRdpgfxSetSurfaceData)(RdpgfxClientContext* context, UINT16 surfaceId, void* pData);
typedef void* (*pcRdpgfxGetSurfaceData)(RdpgfxClientContext* context, UINT16 surfaceId);
typedef UINT (*pcRdpgfxGetSurfaceIds)(RdpgfxClientContext* context, UINT16** ppSurfaceIds,
                                      UINT16* count);
typedef UINT (*pcRdpgfxSetCacheSlotData)(RdpgfxClientContext* context, UINT16 cacheSlot,
                                         void* pData);
typedef void* (*pcRdpgfxGetCacheSlotData)(RdpgfxClientContext* context, UINT16 cacheSlot);

typedef UINT (*pcRdpgfxUpdateSurfaces)(RdpgfxClientContext* context);

typedef UINT (*pcRdpgfxUpdateSurfaceArea)(RdpgfxClientContext* context, UINT16 surfaceId,
                                          UINT32 nrRects, const RECTANGLE_16* rects);

typedef UINT (*pcRdpgfxOnOpen)(RdpgfxClientContext* context, BOOL* do_caps_advertise,
                               BOOL* do_frame_acks);
typedef UINT (*pcRdpgfxOnClose)(RdpgfxClientContext* context);
typedef UINT (*pcRdpgfxCapsAdvertise)(RdpgfxClientContext* context,
                                      const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise);
typedef UINT (*pcRdpgfxCapsConfirm)(RdpgfxClientContext* context,
                                    const RDPGFX_CAPS_CONFIRM_PDU* capsConfirm);
typedef UINT (*pcRdpgfxFrameAcknowledge)(RdpgfxClientContext* context,
                                         const RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge);
typedef UINT (*pcRdpgfxQoeFrameAcknowledge)(
    RdpgfxClientContext* context, const RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU* qoeFrameAcknowledge);

typedef UINT (*pcRdpgfxMapWindowForSurface)(RdpgfxClientContext* context, UINT16 surfaceID,
                                            UINT64 windowID);
typedef UINT (*pcRdpgfxUnmapWindowForSurface)(RdpgfxClientContext* context, UINT64 windowID);

struct _rdpgfx_client_context
{
	void* handle;
	void* custom;

	/* Implementations require locking */
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
	pcRdpgfxMapSurfaceToScaledOutput MapSurfaceToScaledOutput;
	pcRdpgfxMapSurfaceToWindow MapSurfaceToWindow;
	pcRdpgfxMapSurfaceToScaledWindow MapSurfaceToScaledWindow;

	pcRdpgfxGetSurfaceIds GetSurfaceIds;
	pcRdpgfxSetSurfaceData SetSurfaceData;
	pcRdpgfxGetSurfaceData GetSurfaceData;
	pcRdpgfxSetCacheSlotData SetCacheSlotData;
	pcRdpgfxGetCacheSlotData GetCacheSlotData;

	/* Proxy callbacks */
	pcRdpgfxOnOpen OnOpen;
	pcRdpgfxOnClose OnClose;
	pcRdpgfxCapsAdvertise CapsAdvertise;
	pcRdpgfxCapsConfirm CapsConfirm;
	pcRdpgfxFrameAcknowledge FrameAcknowledge;
	pcRdpgfxQoeFrameAcknowledge QoeFrameAcknowledge;

	/* No locking required */
	pcRdpgfxUpdateSurfaces UpdateSurfaces;
	pcRdpgfxUpdateSurfaceArea UpdateSurfaceArea;

	/* These callbacks allow crating/destroying a window directly
	 * mapped to a surface.
	 * NOTE: The surface is already locked.
	 */
	pcRdpgfxMapWindowForSurface MapWindowForSurface;
	pcRdpgfxUnmapWindowForSurface UnmapWindowForSurface;

	CRITICAL_SECTION mux;
	PROFILER_DEFINE(SurfaceProfiler)
};

FREERDP_API RdpgfxClientContext* rdpgfx_client_context_new(rdpSettings* settings);
FREERDP_API void rdpgfx_client_context_free(RdpgfxClientContext* context);

#endif /* FREERDP_CHANNEL_RDPGFX_CLIENT_RDPGFX_H */
