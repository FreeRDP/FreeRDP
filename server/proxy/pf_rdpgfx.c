/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#include <winpr/synch.h>

#include "pf_rdpgfx.h"
#include "pf_context.h"
#include "pf_log.h"

#define TAG PROXY_TAG("gfx")

BOOL pf_server_rdpgfx_init(pServerContext* ps)
{
	RdpgfxServerContext* gfx;
	gfx = ps->gfx = rdpgfx_server_context_new(ps->vcm);

	if (!gfx)
	{
		return FALSE;
	}

	gfx->rdpcontext = (rdpContext*)ps;
	return TRUE;
}

static UINT pf_rdpgfx_reset_graphics(RdpgfxClientContext* context,
                                     const RDPGFX_RESET_GRAPHICS_PDU* resetGraphics)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->ResetGraphics(server, resetGraphics);
}

static UINT pf_rdpgfx_start_frame(RdpgfxClientContext* context,
                                  const RDPGFX_START_FRAME_PDU* startFrame)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->StartFrame(server, startFrame);
}

static UINT pf_rdpgfx_end_frame(RdpgfxClientContext* context,
                                const RDPGFX_END_FRAME_PDU* endFrame)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->EndFrame(server, endFrame);
}

static UINT pf_rdpgfx_surface_command(RdpgfxClientContext* context,
                                      const RDPGFX_SURFACE_COMMAND* cmd)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->SurfaceCommand(server, cmd);
}

static UINT pf_rdpgfx_delete_encoding_context(RdpgfxClientContext* context,
        const RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->DeleteEncodingContext(server, deleteEncodingContext);
}

static UINT pf_rdpgfx_create_surface(RdpgfxClientContext* context,
                                     const RDPGFX_CREATE_SURFACE_PDU* createSurface)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->CreateSurface(server, createSurface);
}

static UINT pf_rdpgfx_delete_surface(RdpgfxClientContext* context,
                                     const RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->DeleteSurface(server, deleteSurface);
}

static UINT pf_rdpgfx_solid_fill(RdpgfxClientContext* context,
                                 const RDPGFX_SOLID_FILL_PDU* solidFill)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->SolidFill(server, solidFill);
}

static UINT pf_rdpgfx_surface_to_surface(RdpgfxClientContext* context,
        const RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->SurfaceToSurface(server, surfaceToSurface);
}

static UINT pf_rdpgfx_surface_to_cache(RdpgfxClientContext* context,
                                       const RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->SurfaceToCache(server, surfaceToCache);
}

static UINT pf_rdpgfx_cache_to_surface(RdpgfxClientContext* context,
                                       const RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->CacheToSurface(server, cacheToSurface);
}

static UINT pf_rdpgfx_cache_import_reply(RdpgfxClientContext* context,
        const RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->CacheImportReply(server, cacheImportReply);
}

static UINT pf_rdpgfx_evict_cache_entry(RdpgfxClientContext* context,
                                        const RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->EvictCacheEntry(server, evictCacheEntry);
}

static UINT pf_rdpgfx_map_surface_to_output(RdpgfxClientContext* context,
        const RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->MapSurfaceToOutput(server, surfaceToOutput);
}

static UINT pf_rdpgfx_map_surface_to_window(RdpgfxClientContext* context,
        const RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->MapSurfaceToWindow(server, surfaceToWindow);
}

static UINT pf_rdpgfx_map_surface_to_scaled_window(RdpgfxClientContext* context,
        const RDPGFX_MAP_SURFACE_TO_SCALED_WINDOW_PDU* surfaceToScaledWindow)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->MapSurfaceToScaledWindow(server, surfaceToScaledWindow);
}


static UINT pf_rdpgfx_map_surface_to_scaled_output(RdpgfxClientContext* context,
        const RDPGFX_MAP_SURFACE_TO_SCALED_OUTPUT_PDU* surfaceToScaledOutput)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->MapSurfaceToScaledOutput(server, surfaceToScaledOutput);
}

static UINT pf_rdpgfx_on_open(RdpgfxClientContext* context,
                              BOOL* do_caps_advertise, BOOL* send_frame_acks)
{
	WLog_VRB(TAG, __FUNCTION__);

	if (NULL != do_caps_advertise)
		*do_caps_advertise = FALSE;

	if (NULL != send_frame_acks)
		*send_frame_acks = FALSE;

	return CHANNEL_RC_OK;
}

static UINT pf_rdpgfx_caps_confirm(RdpgfxClientContext* context,
                                   const RDPGFX_CAPS_CONFIRM_PDU* capsConfirm)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*) pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->CapsConfirm(server, capsConfirm);
}

/* Proxy server side callbacks */
static UINT pf_rdpgfx_caps_advertise(RdpgfxServerContext* context,
                                     const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxClientContext* client = (RdpgfxClientContext*) pdata->pc->gfx;
	UINT16 index;
	UINT16 proxySupportedCapsSetCount = 0;
	RDPGFX_CAPS_ADVERTISE_PDU supportedCapsAdvertise;
	RDPGFX_CAPSET* proxySupportedCapsSet;
	RDPGFX_CAPSET proxySupportedCapsSets[RDPGFX_NUMBER_CAPSETS] = { 0 };

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		/* Add cap to supported caps list if supported by FreeRDP.
		 * TODO: Have a better way of expressing max supported GFX caps version
		 * by FreeRDP.
		 */
		if (currentCaps->version <= RDPGFX_CAPVERSION_106)
		{
			proxySupportedCapsSet = &proxySupportedCapsSets[proxySupportedCapsSetCount++];
			proxySupportedCapsSet->version = currentCaps->version;
			proxySupportedCapsSet->length = currentCaps->length;
			proxySupportedCapsSet->flags = currentCaps->flags;
		}
	}

	supportedCapsAdvertise.capsSetCount = proxySupportedCapsSetCount;
	supportedCapsAdvertise.capsSets = proxySupportedCapsSets;
	WLog_VRB(TAG, __FUNCTION__);
	return client->CapsAdvertise(client, &supportedCapsAdvertise);
}

static UINT pf_rdpgfx_frame_acknowledge(RdpgfxServerContext* context,
                                        const RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxClientContext* client = (RdpgfxClientContext*) pdata->pc->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return client->FrameAcknowledge(client, frameAcknowledge);
}

static UINT pf_rdpgfx_qoe_frame_acknowledge(RdpgfxServerContext* context,
                                        const RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU* qoeFrameAcknowledge)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxClientContext* client = (RdpgfxClientContext*) pdata->pc->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return client->QoeFrameAcknowledge(client, qoeFrameAcknowledge);
}

static UINT pf_rdpgfx_cache_import_offer(RdpgfxServerContext* context,
                                        const RDPGFX_CACHE_IMPORT_OFFER_PDU* cacheImportOffer)
{
	proxyData* pdata = (proxyData*) context->custom;
	RdpgfxClientContext* client = (RdpgfxClientContext*) pdata->pc->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return client->CacheImportOffer(client, cacheImportOffer);
}

void pf_rdpgfx_pipeline_init(RdpgfxClientContext* gfx, RdpgfxServerContext* server,
                             proxyData* pdata)
{
	/* Set server and client side references to proxy data */
	gfx->custom = (void*) pdata;
	server->custom = (void*) pdata;
	/* Set client callbacks */
	gfx->ResetGraphics = pf_rdpgfx_reset_graphics;
	gfx->StartFrame = pf_rdpgfx_start_frame;
	gfx->EndFrame = pf_rdpgfx_end_frame;
	gfx->SurfaceCommand = pf_rdpgfx_surface_command;
	gfx->DeleteEncodingContext = pf_rdpgfx_delete_encoding_context;
	gfx->CreateSurface = pf_rdpgfx_create_surface;
	gfx->DeleteSurface = pf_rdpgfx_delete_surface;
	gfx->SolidFill = pf_rdpgfx_solid_fill;
	gfx->SurfaceToSurface = pf_rdpgfx_surface_to_surface;
	gfx->SurfaceToCache = pf_rdpgfx_surface_to_cache;
	gfx->CacheToSurface = pf_rdpgfx_cache_to_surface;
	gfx->CacheImportReply = pf_rdpgfx_cache_import_reply;
	gfx->EvictCacheEntry = pf_rdpgfx_evict_cache_entry;
	gfx->MapSurfaceToOutput = pf_rdpgfx_map_surface_to_output;
	gfx->MapSurfaceToWindow = pf_rdpgfx_map_surface_to_window;
	gfx->MapSurfaceToScaledOutput = pf_rdpgfx_map_surface_to_scaled_output;
	gfx->MapSurfaceToScaledWindow = pf_rdpgfx_map_surface_to_scaled_window;

	/* No need to register to OnClose callback. GFX termination is handled in pf_server */
	gfx->OnOpen = pf_rdpgfx_on_open;
	gfx->CapsConfirm = pf_rdpgfx_caps_confirm;
	/* Set server callbacks */
	server->CapsAdvertise = pf_rdpgfx_caps_advertise;
	server->FrameAcknowledge = pf_rdpgfx_frame_acknowledge;
	server->CacheImportOffer = pf_rdpgfx_cache_import_offer;
	server->QoeFrameAcknowledge = pf_rdpgfx_qoe_frame_acknowledge;
}
