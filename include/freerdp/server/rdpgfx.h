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

#ifndef FREERDP_CHANNEL_RDPGFX_SERVER_RDPGFX_H
#define FREERDP_CHANNEL_RDPGFX_SERVER_RDPGFX_H

#include <freerdp/channels/rdpgfx.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_rdpgfx_server_context RdpgfxServerContext;
	typedef struct s_rdpgfx_server_private RdpgfxServerPrivate;

	typedef BOOL (*psRdpgfxServerOpen)(RdpgfxServerContext* context);
	typedef BOOL (*psRdpgfxServerClose)(RdpgfxServerContext* context);

	typedef BOOL (*psRdpgfxServerChannelIdAssigned)(RdpgfxServerContext* context, UINT32 channelId);

	typedef BOOL (*psRdpgfxServerInitialize)(RdpgfxServerContext* context, BOOL externalThread);

	typedef UINT (*psRdpgfxResetGraphics)(RdpgfxServerContext* context,
	                                      const RDPGFX_RESET_GRAPHICS_PDU* resetGraphics);
	typedef UINT (*psRdpgfxStartFrame)(RdpgfxServerContext* context,
	                                   const RDPGFX_START_FRAME_PDU* startFrame);
	typedef UINT (*psRdpgfxEndFrame)(RdpgfxServerContext* context,
	                                 const RDPGFX_END_FRAME_PDU* endFrame);
	typedef UINT (*psRdpgfxSurfaceCommand)(RdpgfxServerContext* context,
	                                       const RDPGFX_SURFACE_COMMAND* cmd);
	typedef UINT (*psRdpgfxSurfaceFrameCommand)(RdpgfxServerContext* context,
	                                            const RDPGFX_SURFACE_COMMAND* cmd,
	                                            const RDPGFX_START_FRAME_PDU* startFrame,
	                                            const RDPGFX_END_FRAME_PDU* endFrame);
	typedef UINT (*psRdpgfxDeleteEncodingContext)(
	    RdpgfxServerContext* context,
	    const RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext);
	typedef UINT (*psRdpgfxCreateSurface)(RdpgfxServerContext* context,
	                                      const RDPGFX_CREATE_SURFACE_PDU* createSurface);
	typedef UINT (*psRdpgfxDeleteSurface)(RdpgfxServerContext* context,
	                                      const RDPGFX_DELETE_SURFACE_PDU* deleteSurface);
	typedef UINT (*psRdpgfxSolidFill)(RdpgfxServerContext* context,
	                                  const RDPGFX_SOLID_FILL_PDU* solidFill);
	typedef UINT (*psRdpgfxSurfaceToSurface)(RdpgfxServerContext* context,
	                                         const RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface);
	typedef UINT (*psRdpgfxSurfaceToCache)(RdpgfxServerContext* context,
	                                       const RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache);
	typedef UINT (*psRdpgfxCacheToSurface)(RdpgfxServerContext* context,
	                                       const RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface);
	typedef UINT (*psRdpgfxCacheImportOffer)(RdpgfxServerContext* context,
	                                         const RDPGFX_CACHE_IMPORT_OFFER_PDU* cacheImportOffer);
	typedef UINT (*psRdpgfxCacheImportReply)(RdpgfxServerContext* context,
	                                         const RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply);
	typedef UINT (*psRdpgfxEvictCacheEntry)(RdpgfxServerContext* context,
	                                        const RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry);
	typedef UINT (*psRdpgfxMapSurfaceToOutput)(
	    RdpgfxServerContext* context, const RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput);
	typedef UINT (*psRdpgfxMapSurfaceToWindow)(
	    RdpgfxServerContext* context, const RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow);
	typedef UINT (*psRdpgfxMapSurfaceToScaledOutput)(
	    RdpgfxServerContext* context,
	    const RDPGFX_MAP_SURFACE_TO_SCALED_OUTPUT_PDU* surfaceToOutput);
	typedef UINT (*psRdpgfxMapSurfaceToScaledWindow)(
	    RdpgfxServerContext* context,
	    const RDPGFX_MAP_SURFACE_TO_SCALED_WINDOW_PDU* surfaceToWindow);
	typedef UINT (*psRdpgfxCapsAdvertise)(RdpgfxServerContext* context,
	                                      const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise);
	typedef UINT (*psRdpgfxCapsConfirm)(RdpgfxServerContext* context,
	                                    const RDPGFX_CAPS_CONFIRM_PDU* capsConfirm);
	typedef UINT (*psRdpgfxFrameAcknowledge)(RdpgfxServerContext* context,
	                                         const RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge);
	typedef UINT (*psRdpgfxQoeFrameAcknowledge)(
	    RdpgfxServerContext* context, const RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU* qoeFrameAcknowledge);

	struct s_rdpgfx_server_context
	{
		HANDLE vcm;
		void* custom;

		WINPR_ATTR_NODISCARD psRdpgfxServerOpen Open;
		WINPR_ATTR_NODISCARD psRdpgfxServerClose Close;

		WINPR_ATTR_NODISCARD psRdpgfxResetGraphics ResetGraphics;
		WINPR_ATTR_NODISCARD psRdpgfxStartFrame StartFrame;
		WINPR_ATTR_NODISCARD psRdpgfxEndFrame EndFrame;
		WINPR_ATTR_NODISCARD psRdpgfxSurfaceCommand SurfaceCommand;
		WINPR_ATTR_NODISCARD psRdpgfxSurfaceFrameCommand SurfaceFrameCommand;
		WINPR_ATTR_NODISCARD psRdpgfxDeleteEncodingContext DeleteEncodingContext;
		WINPR_ATTR_NODISCARD psRdpgfxCreateSurface CreateSurface;
		WINPR_ATTR_NODISCARD psRdpgfxDeleteSurface DeleteSurface;
		WINPR_ATTR_NODISCARD psRdpgfxSolidFill SolidFill;
		WINPR_ATTR_NODISCARD psRdpgfxSurfaceToSurface SurfaceToSurface;
		WINPR_ATTR_NODISCARD psRdpgfxSurfaceToCache SurfaceToCache;
		WINPR_ATTR_NODISCARD psRdpgfxCacheToSurface CacheToSurface;
		WINPR_ATTR_NODISCARD psRdpgfxCacheImportOffer CacheImportOffer;
		WINPR_ATTR_NODISCARD psRdpgfxCacheImportReply CacheImportReply;
		WINPR_ATTR_NODISCARD psRdpgfxEvictCacheEntry EvictCacheEntry;
		WINPR_ATTR_NODISCARD psRdpgfxMapSurfaceToOutput MapSurfaceToOutput;
		WINPR_ATTR_NODISCARD psRdpgfxMapSurfaceToWindow MapSurfaceToWindow;
		WINPR_ATTR_NODISCARD psRdpgfxMapSurfaceToScaledOutput MapSurfaceToScaledOutput;
		WINPR_ATTR_NODISCARD psRdpgfxMapSurfaceToScaledWindow MapSurfaceToScaledWindow;
		WINPR_ATTR_NODISCARD psRdpgfxCapsAdvertise CapsAdvertise;
		WINPR_ATTR_NODISCARD psRdpgfxCapsConfirm CapsConfirm;
		WINPR_ATTR_NODISCARD psRdpgfxFrameAcknowledge FrameAcknowledge;
		WINPR_ATTR_NODISCARD psRdpgfxQoeFrameAcknowledge QoeFrameAcknowledge;

		RdpgfxServerPrivate* priv;
		rdpContext* rdpcontext;

		/**
		 * Callback, when the channel got its id assigned.
		 */
		WINPR_ATTR_NODISCARD psRdpgfxServerChannelIdAssigned ChannelIdAssigned;
		/**
		 * Optional: Set thread handling.
		 * When externalThread=TRUE, the application is responsible to call
		 * Poll() periodically to process channel events.
		 *
		 * Defaults to externalThread=FALSE
		 */
		WINPR_ATTR_NODISCARD psRdpgfxServerInitialize Initialize;
	};

	FREERDP_API void rdpgfx_server_context_free(RdpgfxServerContext* context);

	WINPR_ATTR_MALLOC(rdpgfx_server_context_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API RdpgfxServerContext* rdpgfx_server_context_new(HANDLE vcm);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL rdpgfx_server_set_own_thread(RdpgfxServerContext* context,
	                                              BOOL internalThread);

	WINPR_ATTR_NODISCARD
	FREERDP_API HANDLE rdpgfx_server_get_event_handle(RdpgfxServerContext* context);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT rdpgfx_server_handle_messages(RdpgfxServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPGFX_SERVER_RDPGFX_H */
