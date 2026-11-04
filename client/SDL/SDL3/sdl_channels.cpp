/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client Channels
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/disp.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/channels/rdpewa.h>

#include "sdl_channels.hpp"
#include "sdl_context.hpp"
#include "sdl_disp.hpp"

#if defined(__EMSCRIPTEN__)
#include <cstdint>
#include <emscripten.h>

/* GFX counters for the web page, read with Module._freerdp_web_codec_stats() (layout 2):
 *   [0]       layout version, 2
 *   [1]       RDPGFX_CAPVERSION_* the server confirmed in this session, 0 before the confirm
 *             (tells AV1 from AV2 for RDPGFX_CODECID_AV1)
 *   [2..17]   surface commands per codec id (codecId & 15)
 *   [18..33]  their bytes (wraps at 2^32: read deltas as unsigned)
 *   [34..36]  SolidFill, SurfaceToSurface and CacheToSurface PDUs
 * The counters (all but [1]) keep counting across reconnects; the page computes rates from
 * deltas. They are written on the GFX channel thread and read from the page's thread. */
namespace
{
	enum : size_t
	{
		WEB_STATS_VERSION = 0,
		WEB_STATS_CAPS = 1,
		WEB_STATS_COUNT = 2,
		WEB_STATS_BYTES = 18,
		WEB_STATS_SOLID_FILL = 34,
		WEB_STATS_SURFACE_TO_SURFACE = 35,
		WEB_STATS_CACHE_TO_SURFACE = 36,
		WEB_STATS_SIZE = 37
	};

	uint32_t web_stats[WEB_STATS_SIZE] = { 2 };

	struct
	{
		pcRdpgfxSurfaceCommand SurfaceCommand;
		pcRdpgfxSolidFill SolidFill;
		pcRdpgfxSurfaceToSurface SurfaceToSurface;
		pcRdpgfxCacheToSurface CacheToSurface;
		pcRdpgfxCapsConfirm CapsConfirm;
	} web_next = {};

	void web_stats_add(size_t index, uint32_t value)
	{
		__atomic_fetch_add(&web_stats[index], value, __ATOMIC_RELAXED);
	}

	UINT web_SurfaceCommand(RdpgfxClientContext* context, const RDPGFX_SURFACE_COMMAND* cmd)
	{
		const size_t codec = cmd->codecId & 15;
		web_stats_add(WEB_STATS_COUNT + codec, 1);
		web_stats_add(WEB_STATS_BYTES + codec, cmd->length);
		return web_next.SurfaceCommand ? web_next.SurfaceCommand(context, cmd) : CHANNEL_RC_OK;
	}

	UINT web_SolidFill(RdpgfxClientContext* context, const RDPGFX_SOLID_FILL_PDU* pdu)
	{
		web_stats_add(WEB_STATS_SOLID_FILL, 1);
		return web_next.SolidFill ? web_next.SolidFill(context, pdu) : CHANNEL_RC_OK;
	}

	UINT web_SurfaceToSurface(RdpgfxClientContext* context,
	                          const RDPGFX_SURFACE_TO_SURFACE_PDU* pdu)
	{
		web_stats_add(WEB_STATS_SURFACE_TO_SURFACE, 1);
		return web_next.SurfaceToSurface ? web_next.SurfaceToSurface(context, pdu) : CHANNEL_RC_OK;
	}

	UINT web_CacheToSurface(RdpgfxClientContext* context, const RDPGFX_CACHE_TO_SURFACE_PDU* pdu)
	{
		web_stats_add(WEB_STATS_CACHE_TO_SURFACE, 1);
		return web_next.CacheToSurface ? web_next.CacheToSurface(context, pdu) : CHANNEL_RC_OK;
	}

	UINT web_CapsConfirm(RdpgfxClientContext* context, const RDPGFX_CAPS_CONFIRM_PDU* pdu)
	{
		if (pdu && pdu->capsSet)
			__atomic_store_n(&web_stats[WEB_STATS_CAPS], pdu->capsSet->version, __ATOMIC_RELAXED);
		return web_next.CapsConfirm ? web_next.CapsConfirm(context, pdu) : CHANNEL_RC_OK;
	}

	/* after gdi_graphics_pipeline_init: count, then call what gdi installed */
	/* Puts ours into one callback slot and keeps what was there as the next one. A slot that
	 * already holds ours stays as it is, so next never points back at ours. */
	template <typename T> void web_hook(T& slot, T& next, T ours)
	{
		if (slot == ours)
			return;
		next = slot;
		slot = ours;
	}

	/* Runs on every GFX channel connect, which can come again for the same context (a server
	 * that reactivates the session, e.g. on a monitor layout change). gdi then sets its draw
	 * callbacks again but not CapsConfirm, so each callback is checked on its own. */
	void web_stats_wrap(RdpgfxClientContext* gfx)
	{
		if (!gfx)
			return;
		__atomic_store_n(&web_stats[WEB_STATS_CAPS], 0u, __ATOMIC_RELAXED);
		web_hook(gfx->SurfaceCommand, web_next.SurfaceCommand, web_SurfaceCommand);
		web_hook(gfx->SolidFill, web_next.SolidFill, web_SolidFill);
		web_hook(gfx->SurfaceToSurface, web_next.SurfaceToSurface, web_SurfaceToSurface);
		web_hook(gfx->CacheToSurface, web_next.CacheToSurface, web_CacheToSurface);
		web_hook(gfx->CapsConfirm, web_next.CapsConfirm, web_CapsConfirm);
	}
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* freerdp_web_codec_stats(void)
{
	return web_stats;
}
#endif

void sdl_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e)
{
	auto sdl = get_context(context);

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(e);

	if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		auto rail = reinterpret_cast<RailClientContext*>(e->pInterface);
		WINPR_ASSERT(rail);

		if (!sdl->getRailChannelContext().init(rail))
			WLog_Print(sdl->getWLog(), WLOG_WARN, "Failed to initialize RAIL channel");
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		auto clip = reinterpret_cast<CliprdrClientContext*>(e->pInterface);
		WINPR_ASSERT(clip);

		if (!sdl->getClipboardChannelContext().init(clip))
			WLog_Print(sdl->getWLog(), WLOG_WARN, "Failed to initialize clipboard channel");
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		auto disp = reinterpret_cast<DispClientContext*>(e->pInterface);
		WINPR_ASSERT(disp);

		if (!sdl->getDisplayChannelContext().init(disp))
			WLog_Print(sdl->getWLog(), WLOG_WARN, "Failed to initialize display channel");
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		/* After the common handler wires the GFX pipeline, override UpdateWindowFromSurface
		 * to route window-mapped surfaces to RAIL. */
		freerdp_client_OnChannelConnectedEventHandler(context, e);
		auto gfx = reinterpret_cast<RdpgfxClientContext*>(e->pInterface);
		auto ctx = static_cast<rdpContext*>(context);
		if (gfx && freerdp_settings_get_bool(ctx->settings, FreeRDP_RemoteApplicationMode))
			gfx->UpdateWindowFromSurface = SdlRail::UpdateWindowFromSurface;
#if defined(__EMSCRIPTEN__)
		web_stats_wrap(gfx);
#endif
	}
	else
		freerdp_client_OnChannelConnectedEventHandler(context, e);
}

void sdl_OnChannelDisconnectedEventHandler(void* context, const ChannelDisconnectedEventArgs* e)
{
	auto sdl = get_context(context);

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(e);

	// TODO: Set resizeable depending on disp channel and /dynamic-resolution
	if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
		auto rail = reinterpret_cast<RailClientContext*>(e->pInterface);
		WINPR_ASSERT(rail);
		sdl->getRailChannelContext().uninit(rail);
		rail->custom = nullptr;
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		auto clip = reinterpret_cast<CliprdrClientContext*>(e->pInterface);
		WINPR_ASSERT(clip);

		if (!sdl->getClipboardChannelContext().uninit(clip))
			WLog_Print(sdl->getWLog(), WLOG_WARN, "Failed to uninitialize clipboard channel");
		clip->custom = nullptr;
	}
	else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0)
	{
		auto disp = reinterpret_cast<DispClientContext*>(e->pInterface);
		WINPR_ASSERT(disp);

		if (!sdl->getDisplayChannelContext().uninit(disp))
			WLog_Print(sdl->getWLog(), WLOG_WARN, "Failed to uninitialize display channel");
		disp->custom = nullptr;
	}
	else
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}

void sdl_OnUserNotificationEventHandler(void* context, const UserNotificationEventArgs* e)
{
	WINPR_UNUSED(context);
	WINPR_ASSERT(e);
	WINPR_ASSERT(e->e.Sender);

	if (strcmp(e->e.Sender, RDPEWA_CHANNEL_NAME) != 0)
		return;

	if (e->cancelPreviousNotification)
		return;

	struct userdata
	{
		std::string sender;
		std::string message;
		uint32_t timeoutMS = 0;
	};

	auto ud = new struct userdata;
	if (e->message)
		ud->message = e->message;
	if (e->e.Sender)
		ud->sender = e->e.Sender;
	ud->timeoutMS = e->timeoutMS;

	SDL_RunOnMainThread(
	    [](void* userdata)
	    {
		    auto ed = static_cast<struct userdata*>(userdata);
		    assert(ed);
		    auto parent = SDL_GetMouseFocus();
		    if (!parent)
			    parent = SDL_GetKeyboardFocus();
		    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, ed->sender.c_str(),
		                             ed->message.c_str(), parent);
		    delete ed;
	    },
	    ud, false);
}
