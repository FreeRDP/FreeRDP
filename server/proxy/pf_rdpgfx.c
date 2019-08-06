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
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->ResetGraphics(server, resetGraphics)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->ResetGraphics(gfx_decoder, resetGraphics);
}

static UINT pf_rdpgfx_start_frame(RdpgfxClientContext* context,
                                  const RDPGFX_START_FRAME_PDU* startFrame)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->StartFrame(server, startFrame)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->StartFrame(gfx_decoder, startFrame);
}

static UINT pf_rdpgfx_end_frame(RdpgfxClientContext* context, const RDPGFX_END_FRAME_PDU* endFrame)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->EndFrame(server, endFrame)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->EndFrame(gfx_decoder, endFrame);
}

static UINT pf_rdpgfx_surface_command(RdpgfxClientContext* context,
                                      const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->SurfaceCommand(server, cmd)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->SurfaceCommand(gfx_decoder, cmd);
}

static UINT
pf_rdpgfx_delete_encoding_context(RdpgfxClientContext* context,
                                  const RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->DeleteEncodingContext(server, deleteEncodingContext)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->DeleteEncodingContext(gfx_decoder, deleteEncodingContext);
}

static UINT pf_rdpgfx_create_surface(RdpgfxClientContext* context,
                                     const RDPGFX_CREATE_SURFACE_PDU* createSurface)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->CreateSurface(server, createSurface)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->CreateSurface(gfx_decoder, createSurface);
}

static UINT pf_rdpgfx_delete_surface(RdpgfxClientContext* context,
                                     const RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->DeleteSurface(server, deleteSurface)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->DeleteSurface(gfx_decoder, deleteSurface);
}

static UINT pf_rdpgfx_solid_fill(RdpgfxClientContext* context,
                                 const RDPGFX_SOLID_FILL_PDU* solidFill)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->SolidFill(server, solidFill)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->SolidFill(gfx_decoder, solidFill);
}

static UINT pf_rdpgfx_surface_to_surface(RdpgfxClientContext* context,
                                         const RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->SurfaceToSurface(server, surfaceToSurface)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->SurfaceToSurface(gfx_decoder, surfaceToSurface);
}

static UINT pf_rdpgfx_surface_to_cache(RdpgfxClientContext* context,
                                       const RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->SurfaceToCache(server, surfaceToCache)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->SurfaceToCache(gfx_decoder, surfaceToCache);
}

static UINT pf_rdpgfx_cache_to_surface(RdpgfxClientContext* context,
                                       const RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->CacheToSurface(server, cacheToSurface)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->CacheToSurface(gfx_decoder, cacheToSurface);
}

static UINT pf_rdpgfx_cache_import_reply(RdpgfxClientContext* context,
                                         const RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply)
{
	proxyData* pdata = (proxyData*)context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->CacheImportReply(server, cacheImportReply);
}

static UINT pf_rdpgfx_evict_cache_entry(RdpgfxClientContext* context,
                                        const RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->EvictCacheEntry(server, evictCacheEntry)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->EvictCacheEntry(gfx_decoder, evictCacheEntry);
}

static UINT pf_rdpgfx_map_surface_to_output(RdpgfxClientContext* context,
                                            const RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->MapSurfaceToOutput(server, surfaceToOutput)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->MapSurfaceToOutput(gfx_decoder, surfaceToOutput);
}

static UINT pf_rdpgfx_map_surface_to_window(RdpgfxClientContext* context,
                                            const RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->MapSurfaceToWindow(server, surfaceToWindow)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->MapSurfaceToWindow(gfx_decoder, surfaceToWindow);
}

static UINT pf_rdpgfx_map_surface_to_scaled_window(
    RdpgfxClientContext* context,
    const RDPGFX_MAP_SURFACE_TO_SCALED_WINDOW_PDU* surfaceToScaledWindow)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->MapSurfaceToScaledWindow(server, surfaceToScaledWindow)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->MapSurfaceToScaledWindow(gfx_decoder, surfaceToScaledWindow);
}

static UINT pf_rdpgfx_map_surface_to_scaled_output(
    RdpgfxClientContext* context,
    const RDPGFX_MAP_SURFACE_TO_SCALED_OUTPUT_PDU* surfaceToScaledOutput)
{
	UINT error;
	proxyData* pdata = (proxyData*)context->custom;
	proxyConfig* config = pdata->config;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	RdpgfxClientContext* gfx_decoder = pdata->pc->gfx_decoder;
	WLog_VRB(TAG, __FUNCTION__);

	if ((error = server->MapSurfaceToScaledOutput(server, surfaceToScaledOutput)))
		return error;

	if (!config->SessionCapture)
		return CHANNEL_RC_OK;

	return gfx_decoder->MapSurfaceToScaledOutput(gfx_decoder, surfaceToScaledOutput);
}

static UINT pf_rdpgfx_on_open(RdpgfxClientContext* context, BOOL* do_caps_advertise,
                              BOOL* send_frame_acks)
{
	proxyData* pdata = (proxyData*)context->custom;
	WLog_VRB(TAG, __FUNCTION__);

	if (NULL != do_caps_advertise)
		*do_caps_advertise = FALSE;

	if (NULL != send_frame_acks)
		*send_frame_acks = FALSE;

	/* do not open the channel before gfx server side is in ready state */
	WaitForSingleObject(pdata->gfx_server_ready, INFINITE);
	return CHANNEL_RC_OK;
}

static UINT pf_rdpgfx_caps_confirm(RdpgfxClientContext* context,
                                   const RDPGFX_CAPS_CONFIRM_PDU* capsConfirm)
{
	proxyData* pdata = (proxyData*)context->custom;
	RdpgfxServerContext* server = (RdpgfxServerContext*)pdata->ps->gfx;
	WLog_VRB(TAG, __FUNCTION__);
	return server->CapsConfirm(server, capsConfirm);
}

/* Proxy server side callbacks */
static UINT pf_rdpgfx_caps_advertise(RdpgfxServerContext* context,
                                     const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise)
{
	proxyData* pdata = (proxyData*)context->custom;
	RdpgfxClientContext* client = (RdpgfxClientContext*)pdata->pc->gfx_proxy;
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
	proxyData* pdata = (proxyData*)context->custom;
	RdpgfxClientContext* client = (RdpgfxClientContext*)pdata->pc->gfx_proxy;
	WLog_VRB(TAG, __FUNCTION__);
	return client->FrameAcknowledge(client, frameAcknowledge);
}

static UINT
pf_rdpgfx_qoe_frame_acknowledge(RdpgfxServerContext* context,
                                const RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU* qoeFrameAcknowledge)
{
	proxyData* pdata = (proxyData*)context->custom;
	RdpgfxClientContext* client = (RdpgfxClientContext*)pdata->pc->gfx_proxy;
	WLog_VRB(TAG, __FUNCTION__);
	return client->QoeFrameAcknowledge(client, qoeFrameAcknowledge);
}

static UINT pf_rdpgfx_cache_import_offer(RdpgfxServerContext* context,
                                         const RDPGFX_CACHE_IMPORT_OFFER_PDU* cacheImportOffer)
{
	proxyData* pdata = (proxyData*)context->custom;
	RdpgfxClientContext* client = (RdpgfxClientContext*)pdata->pc->gfx_proxy;
	WLog_VRB(TAG, __FUNCTION__);

	if (pdata->config->SessionCapture)
	{
		/* do not proxy CacheImportOffer, as is it currently not supported by FREERDP. */
		return CHANNEL_RC_OK;
	}

	return client->CacheImportOffer(client, cacheImportOffer);
}

void pf_rdpgfx_pipeline_init(RdpgfxClientContext* gfx, RdpgfxServerContext* server,
                             proxyData* pdata)
{
	pClientContext* pc = pdata->pc;

	/* create another gfx client and register it to the gdi graphics pipeline */
	pc->gfx_decoder = rdpgfx_client_context_new(pc->context.settings);
	if (!pc->gfx_decoder)
	{
		WLog_ERR(TAG, "failed to initialize gfx capture client!");
		return;
	}

	/* start GFX pipeline for fake client */
	gdi_graphics_pipeline_init(pc->context.gdi, pc->gfx_decoder);

	/* Set server and client side references to proxy data */
	gfx->custom = (void*)pdata;
	server->custom = (void*)pdata;
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
