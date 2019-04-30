/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include <freerdp/client/rdpgfx.h>
#include <freerdp/server/rdpgfx.h>

#include "pf_rdpgfx.h"
#include "pf_log.h"

#define TAG PROXY_TAG("client")


static UINT proxy_ResetGraphics(RdpgfxClientContext* context,
                                const RDPGFX_RESET_GRAPHICS_PDU* resetGraphics)
{
	WLog_DBG(TAG, "ResetGraphics");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->ResetGraphics(server, resetGraphics);
}

static UINT proxy_StartFrame(RdpgfxClientContext* context,
                             const RDPGFX_START_FRAME_PDU* startFrame)
{
	WLog_DBG(TAG, "StartFrame");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->StartFrame(server, startFrame);
}

static UINT proxy_EndFrame(RdpgfxClientContext* context,
                           const RDPGFX_END_FRAME_PDU* endFrame)
{
	WLog_DBG(TAG, "EndFrame");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->EndFrame(server, endFrame);
}

static UINT proxy_SurfaceCommand(RdpgfxClientContext* context,
                                 const RDPGFX_SURFACE_COMMAND* cmd)
{
	WLog_DBG(TAG, "SurfaceCommand");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->SurfaceCommand(server, cmd);
}

static UINT proxy_DeleteEncodingContext(RdpgfxClientContext* context,
                                        const RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext)
{
	WLog_DBG(TAG, "DeleteEncodingContext");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->DeleteEncodingContext(server, deleteEncodingContext);
}

static UINT proxy_CreateSurface(RdpgfxClientContext* context,
                                const RDPGFX_CREATE_SURFACE_PDU* createSurface)
{
	WLog_DBG(TAG, "CreateSurface");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->CreateSurface(server, createSurface);
}

static UINT proxy_DeleteSurface(RdpgfxClientContext* context,
                                const RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	WLog_DBG(TAG, "DeleteSurface");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->DeleteSurface(server, deleteSurface);
}

static UINT proxy_SolidFill(RdpgfxClientContext* context,
                            const RDPGFX_SOLID_FILL_PDU* solidFill)
{
	WLog_DBG(TAG, "SolidFill");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->SolidFill(server, solidFill);
}

static UINT proxy_SurfaceToSurface(RdpgfxClientContext* context,
                                   const RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface)
{
	WLog_DBG(TAG, "SurfaceToSurface");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->SurfaceToSurface(server, surfaceToSurface);
}

static UINT proxy_SurfaceToCache(RdpgfxClientContext* context,
                                 const RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache)
{
	WLog_DBG(TAG, "SurfaceToCache");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->SurfaceToCache(server, surfaceToCache);
}

static UINT proxy_CacheToSurface(RdpgfxClientContext* context,
                                 const RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface)
{
	WLog_DBG(TAG, "CacheToSurface");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->CacheToSurface(server, cacheToSurface);
}

static UINT proxy_CacheImportReply(RdpgfxClientContext* context,
                                   const RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply)
{
	WLog_DBG(TAG, "CacheImportReply");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->CacheImportReply(server, cacheImportReply);
}

static UINT proxy_EvictCacheEntry(RdpgfxClientContext* context,
                                  const RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry)
{
	WLog_DBG(TAG, "EvictCacheEntry");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->EvictCacheEntry(server, evictCacheEntry);
}

static UINT proxy_MapSurfaceToOutput(RdpgfxClientContext* context,
                                     const RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput)
{
	WLog_DBG(TAG, "MapSurfaceToOutput");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->MapSurfaceToOutput(server, surfaceToOutput);
}

static UINT proxy_MapSurfaceToWindow(RdpgfxClientContext* context,
                                     const RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow)
{
	WLog_DBG(TAG, "MapSurfaceToWindow");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	return server->MapSurfaceToWindow(server, surfaceToWindow);
}

static UINT proxy_OnOpen(RdpgfxClientContext* context, BOOL* do_caps_advertise)
{
	WLog_DBG(TAG, "OnOpen");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;

	if (NULL != do_caps_advertise)
		*do_caps_advertise = FALSE;

	WLog_DBG(TAG, "calling server's Open()");
	// TODO: check if proxy server's dynvc is in ready state

	// we do this error check since the server's API doesn't return WTSAPI error codes
	if (server->Open(server))
	{
		return CHANNEL_RC_OK;
	}

	return CHANNEL_RC_INITIALIZATION_ERROR;
}

static UINT proxy_OnClose(RdpgfxClientContext* context)
{
	WLog_DBG(TAG, "OnClose");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	WLog_DBG(TAG, "calling server's Close()");
	return server->Close(server);
}

static UINT proxy_CapsConfirm(RdpgfxClientContext* context,
                              RDPGFX_CAPS_CONFIRM_PDU* capsConfirm)
{
	WLog_DBG(TAG, "CapsConfirm");
	RdpgfxServerContext* server = (RdpgfxServerContext*) context->custom;
	WLog_DBG(TAG, "calling server's CapsConfirm()");
	return server->CapsConfirm(server, capsConfirm);
}

/* called when proxy server receives `CapsAdvertise` */
static UINT pf_rdpgfx_caps_advertise(RdpgfxServerContext* context,
                                     RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise)
{
	RdpgfxClientContext* client = (RdpgfxClientContext*) context->custom;
	WLog_DBG(TAG, "pf_rdpgfx_caps_advertise, sending caps advertise to target");
	return client->CapsAdvertise(client, capsAdvertise);
}


void proxy_graphics_pipeline_init(RdpgfxClientContext* gfx, RdpgfxServerContext* server)
{
	gfx->custom = (void*) server;
	/* set client callbacks */
	gfx->ResetGraphics = proxy_ResetGraphics;
	gfx->StartFrame = proxy_StartFrame;
	gfx->EndFrame = proxy_EndFrame;
	gfx->SurfaceCommand = proxy_SurfaceCommand;
	gfx->DeleteEncodingContext = proxy_DeleteEncodingContext;
	gfx->CreateSurface = proxy_CreateSurface;
	gfx->DeleteSurface = proxy_DeleteSurface;
	gfx->SolidFill = proxy_SolidFill;
	gfx->SurfaceToSurface = proxy_SurfaceToSurface;
	gfx->SurfaceToCache = proxy_SurfaceToCache;
	gfx->CacheToSurface = proxy_CacheToSurface;
	gfx->CacheImportReply = proxy_CacheImportReply;
	gfx->EvictCacheEntry = proxy_EvictCacheEntry;
	gfx->MapSurfaceToOutput = proxy_MapSurfaceToOutput;
	gfx->MapSurfaceToWindow = proxy_MapSurfaceToWindow;
	gfx->OnOpen = proxy_OnOpen;
	gfx->OnClose = proxy_OnClose;
	gfx->CapsConfirm = proxy_CapsConfirm;
	server->custom = gfx;
	/* set server callbacks */
	server->CapsAdvertise = pf_rdpgfx_caps_advertise;
}
