/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP SDL UI
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

#include <memory>
#include <mutex>

#include <freerdp/config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/streamdump.h>
#include <freerdp/utils/signal.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <freerdp/log.h>

#include <SDL.h>
#include <SDL_hints.h>
#include <SDL_video.h>

#include "sdl_channels.hpp"
#include "sdl_freerdp.hpp"
#include "sdl_utils.hpp"
#include "sdl_disp.hpp"
#include "sdl_monitor.hpp"
#include "sdl_kbd.hpp"
#include "sdl_touch.hpp"
#include "sdl_pointer.hpp"
#include "dialogs/sdl_dialogs.hpp"

#include "aad/sdl_webview.hpp"

#define SDL_TAG CLIENT_TAG("SDL")

enum SDL_EXIT_CODE
{
	/* section 0-15: protocol-independent codes */
	SDL_EXIT_SUCCESS = 0,
	SDL_EXIT_DISCONNECT = 1,
	SDL_EXIT_LOGOFF = 2,
	SDL_EXIT_IDLE_TIMEOUT = 3,
	SDL_EXIT_LOGON_TIMEOUT = 4,
	SDL_EXIT_CONN_REPLACED = 5,
	SDL_EXIT_OUT_OF_MEMORY = 6,
	SDL_EXIT_CONN_DENIED = 7,
	SDL_EXIT_CONN_DENIED_FIPS = 8,
	SDL_EXIT_USER_PRIVILEGES = 9,
	SDL_EXIT_FRESH_CREDENTIALS_REQUIRED = 10,
	SDL_EXIT_DISCONNECT_BY_USER = 11,

	/* section 16-31: license error set */
	SDL_EXIT_LICENSE_INTERNAL = 16,
	SDL_EXIT_LICENSE_NO_LICENSE_SERVER = 17,
	SDL_EXIT_LICENSE_NO_LICENSE = 18,
	SDL_EXIT_LICENSE_BAD_CLIENT_MSG = 19,
	SDL_EXIT_LICENSE_HWID_DOESNT_MATCH = 20,
	SDL_EXIT_LICENSE_BAD_CLIENT = 21,
	SDL_EXIT_LICENSE_CANT_FINISH_PROTOCOL = 22,
	SDL_EXIT_LICENSE_CLIENT_ENDED_PROTOCOL = 23,
	SDL_EXIT_LICENSE_BAD_CLIENT_ENCRYPTION = 24,
	SDL_EXIT_LICENSE_CANT_UPGRADE = 25,
	SDL_EXIT_LICENSE_NO_REMOTE_CONNECTIONS = 26,

	/* section 32-127: RDP protocol error set */
	SDL_EXIT_RDP = 32,

	/* section 128-254: xfreerdp specific exit codes */
	SDL_EXIT_PARSE_ARGUMENTS = 128,
	SDL_EXIT_MEMORY = 129,
	SDL_EXIT_PROTOCOL = 130,
	SDL_EXIT_CONN_FAILED = 131,
	SDL_EXIT_AUTH_FAILURE = 132,
	SDL_EXIT_NEGO_FAILURE = 133,
	SDL_EXIT_LOGON_FAILURE = 134,
	SDL_EXIT_ACCOUNT_LOCKED_OUT = 135,
	SDL_EXIT_PRE_CONNECT_FAILED = 136,
	SDL_EXIT_CONNECT_UNDEFINED = 137,
	SDL_EXIT_POST_CONNECT_FAILED = 138,
	SDL_EXIT_DNS_ERROR = 139,
	SDL_EXIT_DNS_NAME_NOT_FOUND = 140,
	SDL_EXIT_CONNECT_FAILED = 141,
	SDL_EXIT_MCS_CONNECT_INITIAL_ERROR = 142,
	SDL_EXIT_TLS_CONNECT_FAILED = 143,
	SDL_EXIT_INSUFFICIENT_PRIVILEGES = 144,
	SDL_EXIT_CONNECT_CANCELLED = 145,

	SDL_EXIT_CONNECT_TRANSPORT_FAILED = 147,
	SDL_EXIT_CONNECT_PASSWORD_EXPIRED = 148,
	SDL_EXIT_CONNECT_PASSWORD_MUST_CHANGE = 149,
	SDL_EXIT_CONNECT_KDC_UNREACHABLE = 150,
	SDL_EXIT_CONNECT_ACCOUNT_DISABLED = 151,
	SDL_EXIT_CONNECT_PASSWORD_CERTAINLY_EXPIRED = 152,
	SDL_EXIT_CONNECT_CLIENT_REVOKED = 153,
	SDL_EXIT_CONNECT_WRONG_PASSWORD = 154,
	SDL_EXIT_CONNECT_ACCESS_DENIED = 155,
	SDL_EXIT_CONNECT_ACCOUNT_RESTRICTION = 156,
	SDL_EXIT_CONNECT_ACCOUNT_EXPIRED = 157,
	SDL_EXIT_CONNECT_LOGON_TYPE_NOT_GRANTED = 158,
	SDL_EXIT_CONNECT_NO_OR_MISSING_CREDENTIALS = 159,

	SDL_EXIT_UNKNOWN = 255,
};

struct sdl_exit_code_map_t
{
	DWORD error;
	int code;
	const char* code_tag;
};

#define ENTRY(x, y) \
	{               \
		x, y, #y    \
	}
static const struct sdl_exit_code_map_t sdl_exit_code_map[] = {
	ENTRY(FREERDP_ERROR_SUCCESS, SDL_EXIT_SUCCESS), ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_DISCONNECT),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LOGOFF), ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_IDLE_TIMEOUT),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LOGON_TIMEOUT),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_CONN_REPLACED),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_OUT_OF_MEMORY),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_CONN_DENIED),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_CONN_DENIED_FIPS),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_USER_PRIVILEGES),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_FRESH_CREDENTIALS_REQUIRED),
	ENTRY(ERRINFO_LOGOFF_BY_USER, SDL_EXIT_DISCONNECT_BY_USER),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_UNKNOWN),

	/* section 16-31: license error set */
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_INTERNAL),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_NO_LICENSE_SERVER),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_NO_LICENSE),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_BAD_CLIENT_MSG),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_HWID_DOESNT_MATCH),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_BAD_CLIENT),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_CANT_FINISH_PROTOCOL),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_CLIENT_ENDED_PROTOCOL),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_BAD_CLIENT_ENCRYPTION),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_CANT_UPGRADE),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_NO_REMOTE_CONNECTIONS),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_LICENSE_CANT_UPGRADE),

	/* section 32-127: RDP protocol error set */
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_RDP),

	/* section 128-254: xfreerdp specific exit codes */
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_PARSE_ARGUMENTS), ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_MEMORY),
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_PROTOCOL), ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_CONN_FAILED),

	ENTRY(FREERDP_ERROR_AUTHENTICATION_FAILED, SDL_EXIT_AUTH_FAILURE),
	ENTRY(FREERDP_ERROR_SECURITY_NEGO_CONNECT_FAILED, SDL_EXIT_NEGO_FAILURE),
	ENTRY(FREERDP_ERROR_CONNECT_LOGON_FAILURE, SDL_EXIT_LOGON_FAILURE),
	ENTRY(FREERDP_ERROR_CONNECT_ACCOUNT_LOCKED_OUT, SDL_EXIT_ACCOUNT_LOCKED_OUT),
	ENTRY(FREERDP_ERROR_PRE_CONNECT_FAILED, SDL_EXIT_PRE_CONNECT_FAILED),
	ENTRY(FREERDP_ERROR_CONNECT_UNDEFINED, SDL_EXIT_CONNECT_UNDEFINED),
	ENTRY(FREERDP_ERROR_POST_CONNECT_FAILED, SDL_EXIT_POST_CONNECT_FAILED),
	ENTRY(FREERDP_ERROR_DNS_ERROR, SDL_EXIT_DNS_ERROR),
	ENTRY(FREERDP_ERROR_DNS_NAME_NOT_FOUND, SDL_EXIT_DNS_NAME_NOT_FOUND),
	ENTRY(FREERDP_ERROR_CONNECT_FAILED, SDL_EXIT_CONNECT_FAILED),
	ENTRY(FREERDP_ERROR_MCS_CONNECT_INITIAL_ERROR, SDL_EXIT_MCS_CONNECT_INITIAL_ERROR),
	ENTRY(FREERDP_ERROR_TLS_CONNECT_FAILED, SDL_EXIT_TLS_CONNECT_FAILED),
	ENTRY(FREERDP_ERROR_INSUFFICIENT_PRIVILEGES, SDL_EXIT_INSUFFICIENT_PRIVILEGES),
	ENTRY(FREERDP_ERROR_CONNECT_CANCELLED, SDL_EXIT_CONNECT_CANCELLED),
	ENTRY(FREERDP_ERROR_CONNECT_TRANSPORT_FAILED, SDL_EXIT_CONNECT_TRANSPORT_FAILED),
	ENTRY(FREERDP_ERROR_CONNECT_PASSWORD_EXPIRED, SDL_EXIT_CONNECT_PASSWORD_EXPIRED),
	ENTRY(FREERDP_ERROR_CONNECT_PASSWORD_MUST_CHANGE, SDL_EXIT_CONNECT_PASSWORD_MUST_CHANGE),
	ENTRY(FREERDP_ERROR_CONNECT_KDC_UNREACHABLE, SDL_EXIT_CONNECT_KDC_UNREACHABLE),
	ENTRY(FREERDP_ERROR_CONNECT_ACCOUNT_DISABLED, SDL_EXIT_CONNECT_ACCOUNT_DISABLED),
	ENTRY(FREERDP_ERROR_CONNECT_PASSWORD_CERTAINLY_EXPIRED,
	      SDL_EXIT_CONNECT_PASSWORD_CERTAINLY_EXPIRED),
	ENTRY(FREERDP_ERROR_CONNECT_CLIENT_REVOKED, SDL_EXIT_CONNECT_CLIENT_REVOKED),
	ENTRY(FREERDP_ERROR_CONNECT_WRONG_PASSWORD, SDL_EXIT_CONNECT_WRONG_PASSWORD),
	ENTRY(FREERDP_ERROR_CONNECT_ACCESS_DENIED, SDL_EXIT_CONNECT_ACCESS_DENIED),
	ENTRY(FREERDP_ERROR_CONNECT_ACCOUNT_RESTRICTION, SDL_EXIT_CONNECT_ACCOUNT_RESTRICTION),
	ENTRY(FREERDP_ERROR_CONNECT_ACCOUNT_EXPIRED, SDL_EXIT_CONNECT_ACCOUNT_EXPIRED),
	ENTRY(FREERDP_ERROR_CONNECT_LOGON_TYPE_NOT_GRANTED, SDL_EXIT_CONNECT_LOGON_TYPE_NOT_GRANTED),
	ENTRY(FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS,
	      SDL_EXIT_CONNECT_NO_OR_MISSING_CREDENTIALS)
};

static const struct sdl_exit_code_map_t* sdl_map_entry_by_code(int exit_code)
{
	size_t x;
	for (x = 0; x < ARRAYSIZE(sdl_exit_code_map); x++)
	{
		const struct sdl_exit_code_map_t* cur = &sdl_exit_code_map[x];
		if (cur->code == exit_code)
			return cur;
	}
	return nullptr;
}

static const struct sdl_exit_code_map_t* sdl_map_entry_by_error(DWORD error)
{
	size_t x;
	for (x = 0; x < ARRAYSIZE(sdl_exit_code_map); x++)
	{
		const struct sdl_exit_code_map_t* cur = &sdl_exit_code_map[x];
		if (cur->error == error)
			return cur;
	}
	return nullptr;
}

static int sdl_map_error_to_exit_code(DWORD error)
{
	const struct sdl_exit_code_map_t* entry = sdl_map_entry_by_error(error);
	if (entry)
		return entry->code;

	return SDL_EXIT_CONN_FAILED;
}

static const char* sdl_map_error_to_code_tag(DWORD error)
{
	const struct sdl_exit_code_map_t* entry = sdl_map_entry_by_error(error);
	if (entry)
		return entry->code_tag;
	return nullptr;
}

static const char* sdl_map_to_code_tag(int code)
{
	const struct sdl_exit_code_map_t* entry = sdl_map_entry_by_code(code);
	if (entry)
		return entry->code_tag;
	return nullptr;
}

static int error_info_to_error(freerdp* instance, DWORD* pcode)
{
	const DWORD code = freerdp_error_info(instance);
	const char* name = freerdp_get_error_info_name(code);
	const char* str = freerdp_get_error_info_string(code);
	const int exit_code = sdl_map_error_to_exit_code(code);

	WLog_DBG(SDL_TAG, "Terminate with %s due to ERROR_INFO %s [0x%08" PRIx32 "]: %s",
	         sdl_map_error_to_code_tag(exit_code), name, code, str);
	if (pcode)
		*pcode = code;
	return exit_code;
}

/* This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas. */
static BOOL sdl_begin_paint(rdpContext* context)
{
	rdpGdi* gdi;
	auto sdl = get_context(context);

	WINPR_ASSERT(sdl);

	HANDLE handles[] = { sdl->update_complete.handle(), freerdp_abort_event(context) };
	const DWORD status = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
	switch (status)
	{
		case WAIT_OBJECT_0:
			break;
		default:
			return FALSE;
	}
	sdl->update_complete.clear();

	gdi = context->gdi;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(gdi->primary);
	WINPR_ASSERT(gdi->primary->hdc);
	WINPR_ASSERT(gdi->primary->hdc->hwnd);
	WINPR_ASSERT(gdi->primary->hdc->hwnd->invalid);
	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	gdi->primary->hdc->hwnd->ninvalid = 0;

	return TRUE;
}

static BOOL sdl_redraw(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	auto gdi = sdl->context()->gdi;
	return gdi_send_suppress_output(gdi, FALSE);
}

class SdlEventUpdateTriggerGuard
{
  private:
	SdlContext* _sdl;

  public:
	SdlEventUpdateTriggerGuard(SdlContext* sdl) : _sdl(sdl)
	{
	}
	~SdlEventUpdateTriggerGuard()
	{
		_sdl->update_complete.set();
	}
};

static BOOL sdl_draw_to_window_rect(SdlContext* sdl, SDL_Surface* screen, SDL_Surface* surface,
                                    SDL_Point offset, const SDL_Rect& srcRect)
{
	SDL_Rect dstRect = { offset.x + srcRect.x, offset.y + srcRect.y, srcRect.w, srcRect.h };
	SDL_SetClipRect(surface, &srcRect);
	SDL_SetClipRect(screen, &dstRect);
	SDL_BlitSurface(surface, &srcRect, screen, &dstRect);
	return TRUE;
}

static BOOL sdl_draw_to_window_rect(SdlContext* sdl, SDL_Surface* screen, SDL_Surface* surface,
                                    SDL_Point offset, const std::vector<SDL_Rect>& rects = {})
{
	if (rects.empty())
	{
		return sdl_draw_to_window_rect(sdl, screen, surface, offset,
		                               { 0, 0, surface->w, surface->h });
	}
	for (auto& srcRect : rects)
	{
		if (!sdl_draw_to_window_rect(sdl, screen, surface, offset, srcRect))
			return FALSE;
	}
	return TRUE;
}

static BOOL sdl_draw_to_window_scaled_rect(SdlContext* sdl, Sint32 windowId, SDL_Surface* screen,
                                           SDL_Surface* surface, const SDL_Rect& srcRect)
{
	SDL_Rect dstRect = srcRect;
	sdl_scale_coordinates(sdl, windowId, &dstRect.x, &dstRect.y, FALSE, TRUE);
	sdl_scale_coordinates(sdl, windowId, &dstRect.w, &dstRect.h, FALSE, TRUE);
	SDL_SetClipRect(surface, &srcRect);
	SDL_SetClipRect(screen, &dstRect);
	SDL_BlitScaled(surface, &srcRect, screen, &dstRect);

	return TRUE;
}

static BOOL sdl_draw_to_window_scaled_rect(SdlContext* sdl, Sint32 windowId, SDL_Surface* screen,
                                           SDL_Surface* surface,
                                           const std::vector<SDL_Rect>& rects = {})
{
	if (rects.empty())
	{
		return sdl_draw_to_window_scaled_rect(sdl, windowId, screen, surface,
		                                      { 0, 0, surface->w, surface->h });
	}
	for (auto& srcRect : rects)
	{
		if (!sdl_draw_to_window_scaled_rect(sdl, windowId, screen, surface, srcRect))
			return FALSE;
	}
	return TRUE;
}

static BOOL sdl_draw_to_window(SdlContext* sdl, sdl_window_t window,
                               const std::vector<SDL_Rect>& rects = {})
{
	WINPR_ASSERT(sdl);

	auto context = sdl->context();
	auto gdi = context->gdi;

	SDL_Surface* screen = SDL_GetWindowSurface(window.window);

	int w, h;
	SDL_GetWindowSize(window.window, &w, &h);

	if (!freerdp_settings_get_bool(context->settings, FreeRDP_SmartSizing))
	{
		if (gdi->width < w)
		{
			window.offset_x = (w - gdi->width) / 2;
		}
		if (gdi->height < h)
		{
			window.offset_y = (h - gdi->height) / 2;
		}

		auto surface = sdl->primary.get();
		if (!sdl_draw_to_window_rect(sdl, screen, surface, { window.offset_x, window.offset_y },
		                             rects))
			return FALSE;
	}
	else
	{
		auto id = SDL_GetWindowID(window.window);

		if (!sdl_draw_to_window_scaled_rect(sdl, id, screen, sdl->primary.get(), rects))
			return FALSE;
	}
	SDL_UpdateWindowSurface(window.window);
	return TRUE;
}

static BOOL sdl_draw_to_window(SdlContext* sdl, const std::vector<sdl_window_t>& window,
                               const std::vector<SDL_Rect>& rects = {})
{
	for (auto& window : sdl->windows)
	{
		if (!sdl_draw_to_window(sdl, window, rects))
			return FALSE;
	}

	return TRUE;
}

static BOOL sdl_end_paint_process(rdpContext* context)
{
	rdpGdi* gdi;
	auto sdl = get_context(context);

	WINPR_ASSERT(context);

	SdlEventUpdateTriggerGuard guard(sdl);

	gdi = context->gdi;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(gdi->primary);
	WINPR_ASSERT(gdi->primary->hdc);
	WINPR_ASSERT(gdi->primary->hdc->hwnd);
	WINPR_ASSERT(gdi->primary->hdc->hwnd->invalid);
	if (gdi->suppressOutput || gdi->primary->hdc->hwnd->invalid->null)
		return TRUE;

	const INT32 ninvalid = gdi->primary->hdc->hwnd->ninvalid;
	const GDI_RGN* cinvalid = gdi->primary->hdc->hwnd->cinvalid;

	if (ninvalid < 1)
		return TRUE;

	std::vector<SDL_Rect> rects;
	for (INT32 x = 0; x < ninvalid; x++)
	{
		auto& rgn = cinvalid[x];
		rects.push_back({ rgn.x, rgn.y, rgn.w, rgn.h });
	}

	return sdl_draw_to_window(sdl, sdl->windows, rects);
}

/* This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
static BOOL sdl_end_paint(rdpContext* context)
{
	auto sdl = get_context(context);
	WINPR_ASSERT(sdl);

	std::lock_guard<CriticalSection> lock(sdl->critical);
	const BOOL rc = sdl_push_user_event(SDL_USEREVENT_UPDATE, context);

	return rc;
}

static void sdl_destroy_primary(SdlContext* sdl)
{
	if (!sdl)
		return;
	sdl->primary.reset();
	sdl->primary_format.reset();
}

/* Create a SDL surface from the GDI buffer */
static BOOL sdl_create_primary(SdlContext* sdl)
{
	rdpGdi* gdi;

	WINPR_ASSERT(sdl);

	gdi = sdl->context()->gdi;
	WINPR_ASSERT(gdi);

	sdl_destroy_primary(sdl);
	sdl->primary = SDLSurfacePtr(
	    SDL_CreateRGBSurfaceWithFormatFrom(gdi->primary_buffer, static_cast<int>(gdi->width),
	                                       static_cast<int>(gdi->height),
	                                       static_cast<int>(FreeRDPGetBitsPerPixel(gdi->dstFormat)),
	                                       static_cast<int>(gdi->stride), sdl->sdl_pixel_format),
	    SDL_FreeSurface);
	sdl->primary_format = SDLPixelFormatPtr(SDL_AllocFormat(sdl->sdl_pixel_format), SDL_FreeFormat);

	if (!sdl->primary || !sdl->primary_format)
		return FALSE;

	SDL_SetSurfaceBlendMode(sdl->primary.get(), SDL_BLENDMODE_NONE);
	SDL_FillRect(sdl->primary.get(), nullptr,
	             SDL_MapRGBA(sdl->primary_format.get(), 0, 0, 0, 0xff));

	return TRUE;
}

static BOOL sdl_desktop_resize(rdpContext* context)
{
	rdpGdi* gdi;
	rdpSettings* settings;
	auto sdl = get_context(context);

	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	gdi = context->gdi;
	if (!gdi_resize(gdi, freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
	                freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight)))
		return FALSE;
	return sdl_create_primary(sdl);
}

/* This function is called to output a System BEEP */
static BOOL sdl_play_sound(rdpContext* context, const PLAY_SOUND_UPDATE* play_sound)
{
	/* TODO: Implement */
	WINPR_UNUSED(context);
	WINPR_UNUSED(play_sound);
	return TRUE;
}

static BOOL sdl_wait_for_init(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);
	sdl->initialize.set();

	HANDLE handles[] = { sdl->initialized.handle(), freerdp_abort_event(sdl->context()) };

	const DWORD rc = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
	switch (rc)
	{
		case WAIT_OBJECT_0:
			return TRUE;
		default:
			return FALSE;
	}
}

/* Called before a connection is established.
 * Set all configuration options to support and load channels here. */
static BOOL sdl_pre_connect(freerdp* instance)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	auto sdl = get_context(instance->context);

	auto settings = instance->context->settings;
	WINPR_ASSERT(settings);

	/* Optional OS identifier sent to server */
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMAJORTYPE_UNIX))
		return FALSE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMinorType, OSMINORTYPE_NATIVE_SDL))
		return FALSE;
	/* OrderSupport is initialized at this point.
	 * Only override it if you plan to implement custom order
	 * callbacks or deactiveate certain features. */
	/* Register the channel listeners.
	 * They are required to set up / tear down channels if they are loaded. */
	PubSub_SubscribeChannelConnected(instance->context->pubSub, sdl_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    sdl_OnChannelDisconnectedEventHandler);

	if (!freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
	{
		UINT32 maxWidth = 0;
		UINT32 maxHeight = 0;

		if (!sdl_wait_for_init(sdl))
			return FALSE;

		if (!sdl_detect_monitors(sdl, &maxWidth, &maxHeight))
			return FALSE;

		if ((maxWidth != 0) && (maxHeight != 0) &&
		    !freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
		{
			WLog_Print(sdl->log, WLOG_INFO, "Update size to %ux%u", maxWidth, maxHeight);
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, maxWidth))
				return FALSE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, maxHeight))
				return FALSE;
		}
	}
	else
	{
		/* Check +auth-only has a username and password. */
		if (!freerdp_settings_get_string(settings, FreeRDP_Password))
		{
			WLog_Print(sdl->log, WLOG_INFO, "auth-only, but no password set. Please provide one.");
			return FALSE;
		}

		if (!freerdp_settings_set_bool(settings, FreeRDP_DeactivateClientDecoding, TRUE))
			return FALSE;

		WLog_Print(sdl->log, WLOG_INFO, "Authentication only. Don't connect SDL.");
	}

	/* TODO: Any code your client requires */
	return TRUE;
}

static const char* sdl_window_get_title(rdpSettings* settings)
{
	const char* windowTitle;
	UINT32 port;
	BOOL addPort;
	const char* name;
	const char* prefix = "FreeRDP:";

	if (!settings)
		return nullptr;

	windowTitle = freerdp_settings_get_string(settings, FreeRDP_WindowTitle);
	if (windowTitle)
		return _strdup(windowTitle);

	name = freerdp_settings_get_server_name(settings);
	port = freerdp_settings_get_uint32(settings, FreeRDP_ServerPort);

	addPort = (port != 3389);

	char buffer[MAX_PATH + 64] = { 0 };

	if (!addPort)
		sprintf_s(buffer, sizeof(buffer), "%s %s", prefix, name);
	else
		sprintf_s(buffer, sizeof(buffer), "%s %s:%" PRIu32, prefix, name, port);

	freerdp_settings_set_string(settings, FreeRDP_WindowTitle, buffer);
	return freerdp_settings_get_string(settings, FreeRDP_WindowTitle);
}

static void sdl_cleanup_sdl(SdlContext* sdl)
{
	if (!sdl)
		return;

	for (auto& window : sdl->windows)
		SDL_DestroyWindow(window.window);

	sdl_destroy_primary(sdl);

	sdl->windows.clear();
	SDL_Quit();
}

static BOOL sdl_create_windows(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	auto settings = sdl->context()->settings;
	auto title = sdl_window_get_title(settings);
	BOOL rc = FALSE;

	UINT32 windowCount = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);

	for (UINT32 x = 0; x < windowCount; x++)
	{
		auto monitor = static_cast<rdpMonitor*>(
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, x));

		Uint32 w = monitor->width;
		Uint32 h = monitor->height;
		if (!(freerdp_settings_get_bool(settings, FreeRDP_UseMultimon) ||
		      freerdp_settings_get_bool(settings, FreeRDP_Fullscreen)))
		{
			w = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
			h = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
		}

		sdl_window_t window = {};
		Uint32 flags = SDL_WINDOW_SHOWN;
		Uint32 startupX = SDL_WINDOWPOS_CENTERED_DISPLAY(x);
		Uint32 startupY = SDL_WINDOWPOS_CENTERED_DISPLAY(x);

		if (monitor->attributes.desktopScaleFactor > 100)
		{
#if SDL_VERSION_ATLEAST(2, 0, 1)
			flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
		}

		if (freerdp_settings_get_bool(settings, FreeRDP_Fullscreen) &&
		    !freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		{
			flags |= SDL_WINDOW_FULLSCREEN;
		}

		if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		{
			flags |= SDL_WINDOW_BORDERLESS;
		}
		else
		{
			window.offset_x = 0;
			window.offset_y = 0;
		}

		window.window = SDL_CreateWindow(title, startupX, startupY, static_cast<int>(w),
		                                 static_cast<int>(h), flags);
		if (!window.window)
			goto fail;

		if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		{
			int win_x;
			int win_y;
			SDL_GetWindowPosition(window.window, &win_x, &win_y);
			window.offset_x = 0 - win_x;
			window.offset_y = 0 - win_y;
		}

		sdl->windows.push_back(window);
	}

	rc = TRUE;
fail:

	sdl->windows_created.set();
	return rc;
}

static BOOL sdl_wait_create_windows(SdlContext* sdl)
{
	std::lock_guard<CriticalSection> lock(sdl->critical);
	sdl->windows_created.clear();
	if (!sdl_push_user_event(SDL_USEREVENT_CREATE_WINDOWS, sdl))
		return FALSE;

	HANDLE handles[] = { sdl->initialized.handle(), freerdp_abort_event(sdl->context()) };

	const DWORD rc = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
	switch (rc)
	{
		case WAIT_OBJECT_0:
			return TRUE;
		default:
			return FALSE;
	}
}

static int sdl_run(SdlContext* sdl)
{
	int rc = -1;
	WINPR_ASSERT(sdl);

	HANDLE handles[] = { sdl->initialize.handle(), freerdp_abort_event(sdl->context()) };
	const DWORD status = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
	switch (status)
	{
		case WAIT_OBJECT_0:
			break;
		default:
			return -1;
	}

	SDL_Init(SDL_INIT_VIDEO);
#if SDL_VERSION_ATLEAST(2, 0, 16)
	SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");
#endif

	sdl->initialized.set();

	while (!freerdp_shall_disconnect_context(sdl->context()))
	{
		SDL_Event windowEvent = { 0 };
		while (!freerdp_shall_disconnect_context(sdl->context()) &&
		       SDL_WaitEventTimeout(nullptr, 1000))
		{
			/* Only poll standard SDL events and SDL_USEREVENTS meant to create dialogs.
			 * do not process the dialog return value events here.
			 */
			const int prc = SDL_PeepEvents(&windowEvent, 1, SDL_GETEVENT, SDL_FIRSTEVENT,
			                               SDL_USEREVENT_SCARD_DIALOG);
			if (prc < 0)
			{
				if (sdl_log_error(prc, sdl->log, "SDL_PeepEvents"))
					continue;
			}

#if defined(WITH_DEBUG_SDL_EVENTS)
			SDL_Log("got event %s [0x%08" PRIx32 "]", sdl_event_type_str(windowEvent.type),
			        windowEvent.type);
#endif
			std::lock_guard<CriticalSection> lock(sdl->critical);
			switch (windowEvent.type)
			{
				case SDL_QUIT:
					freerdp_abort_connect_context(sdl->context());
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					const SDL_KeyboardEvent* ev = &windowEvent.key;
					sdl->input.keyboard_handle_event(ev);
				}
				break;
				case SDL_KEYMAPCHANGED:
				{
				}
				break; // TODO: Switch keyboard layout
				case SDL_MOUSEMOTION:
				{
					const SDL_MouseMotionEvent* ev = &windowEvent.motion;
					sdl_handle_mouse_motion(sdl, ev);
				}
				break;
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
				{
					const SDL_MouseButtonEvent* ev = &windowEvent.button;
					sdl_handle_mouse_button(sdl, ev);
				}
				break;
				case SDL_MOUSEWHEEL:
				{
					const SDL_MouseWheelEvent* ev = &windowEvent.wheel;
					sdl_handle_mouse_wheel(sdl, ev);
				}
				break;
				case SDL_FINGERDOWN:
				{
					const SDL_TouchFingerEvent* ev = &windowEvent.tfinger;
					sdl_handle_touch_down(sdl, ev);
				}
				break;
				case SDL_FINGERUP:
				{
					const SDL_TouchFingerEvent* ev = &windowEvent.tfinger;
					sdl_handle_touch_up(sdl, ev);
				}
				break;
				case SDL_FINGERMOTION:
				{
					const SDL_TouchFingerEvent* ev = &windowEvent.tfinger;
					sdl_handle_touch_motion(sdl, ev);
				}
				break;
#if SDL_VERSION_ATLEAST(2, 0, 10)
				case SDL_DISPLAYEVENT:
				{
					const SDL_DisplayEvent* ev = &windowEvent.display;
					sdl->disp.handle_display_event(ev);
				}
				break;
#endif
				case SDL_WINDOWEVENT:
				{
					const SDL_WindowEvent* ev = &windowEvent.window;
					sdl->disp.handle_window_event(ev);

					switch (ev->event)
					{
						case SDL_WINDOWEVENT_RESIZED:
						case SDL_WINDOWEVENT_SIZE_CHANGED:
						{
							auto window = SDL_GetWindowFromID(ev->windowID);

							if (window)
							{
								auto surface = SDL_GetWindowSurface(window);
								if (surface)
								{
									SDL_Rect rect = { 0, 0, surface->w, surface->h };

									auto color = SDL_MapRGBA(surface->format, 0, 0, 0, 0xff);
									SDL_FillRect(surface, &rect, color);
								}
								sdl_draw_to_window(sdl, sdl->windows);
							}
						}
						break;
						default:
							break;
					}
				}
				break;

				case SDL_RENDER_TARGETS_RESET:
					sdl_redraw(sdl);
					break;
				case SDL_RENDER_DEVICE_RESET:
					sdl_redraw(sdl);
					break;
				case SDL_APP_WILLENTERFOREGROUND:
					sdl_redraw(sdl);
					break;
				case SDL_USEREVENT_CERT_DIALOG:
				{
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char*>(windowEvent.user.data2);
					sdl_cert_dialog_show(title, msg);
				}
				break;
				case SDL_USEREVENT_SHOW_DIALOG:
				{
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char*>(windowEvent.user.data2);
					sdl_message_dialog_show(title, msg, windowEvent.user.code);
				}
				break;
				case SDL_USEREVENT_SCARD_DIALOG:
				{
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char**>(windowEvent.user.data2);
					sdl_scard_dialog_show(title, windowEvent.user.code, msg);
				}
				break;
				case SDL_USEREVENT_AUTH_DIALOG:
					sdl_auth_dialog_show(
					    reinterpret_cast<const SDL_UserAuthArg*>(windowEvent.padding));
					break;
				case SDL_USEREVENT_UPDATE:
				{
					auto context = static_cast<rdpContext*>(windowEvent.user.data1);
					sdl_end_paint_process(context);
				}
				break;
				case SDL_USEREVENT_CREATE_WINDOWS:
				{
					auto ctx = static_cast<SdlContext*>(windowEvent.user.data1);
					sdl_create_windows(ctx);
				}
				break;
				case SDL_USEREVENT_WINDOW_RESIZEABLE:
				{
					auto window = static_cast<SDL_Window*>(windowEvent.user.data1);
					const SDL_bool use = windowEvent.user.code != 0 ? SDL_TRUE : SDL_FALSE;
					SDL_SetWindowResizable(window, use);
				}
				break;
				case SDL_USEREVENT_WINDOW_FULLSCREEN:
				{
					auto window = static_cast<SDL_Window*>(windowEvent.user.data1);
					const SDL_bool enter = windowEvent.user.code != 0 ? SDL_TRUE : SDL_FALSE;

					Uint32 curFlags = SDL_GetWindowFlags(window);

					if (enter)
					{
						if (!(curFlags & SDL_WINDOW_BORDERLESS))
						{
							auto idx = SDL_GetWindowDisplayIndex(window);
							SDL_DisplayMode mode = {};
							SDL_GetCurrentDisplayMode(idx, &mode);

							SDL_RestoreWindow(window); // Maximize so we can see the caption and
							                           // bits
							SDL_SetWindowBordered(window, SDL_FALSE);
							SDL_SetWindowPosition(window, 0, 0);
#if SDL_VERSION_ATLEAST(2, 0, 16)
							SDL_SetWindowAlwaysOnTop(window, SDL_TRUE);
#endif
							SDL_RaiseWindow(window);
							SDL_SetWindowSize(window, mode.w, mode.h);
						}
					}
					else
					{
						if (curFlags & SDL_WINDOW_BORDERLESS)
						{

							SDL_SetWindowBordered(window, SDL_TRUE);
#if SDL_VERSION_ATLEAST(2, 0, 16)
							SDL_SetWindowAlwaysOnTop(window, SDL_FALSE);
#endif
							SDL_RaiseWindow(window);
							SDL_MinimizeWindow(
							    window); // Maximize so we can see the caption and bits
							SDL_MaximizeWindow(
							    window); // Maximize so we can see the caption and bits
						}
					}
				}
				break;
				case SDL_USEREVENT_POINTER_NULL:
					SDL_ShowCursor(SDL_DISABLE);
					break;
				case SDL_USEREVENT_POINTER_DEFAULT:
				{
					SDL_Cursor* def = SDL_GetDefaultCursor();
					SDL_SetCursor(def);
					SDL_ShowCursor(SDL_ENABLE);
				}
				break;
				case SDL_USEREVENT_POINTER_POSITION:
				{
					const INT32 x =
					    static_cast<INT32>(reinterpret_cast<uintptr_t>(windowEvent.user.data1));
					const INT32 y =
					    static_cast<INT32>(reinterpret_cast<uintptr_t>(windowEvent.user.data2));

					SDL_Window* window = SDL_GetMouseFocus();
					if (window)
					{
						const Uint32 id = SDL_GetWindowID(window);

						INT32 sx = x;
						INT32 sy = y;
						if (sdl_scale_coordinates(sdl, id, &sx, &sy, FALSE, FALSE))
							SDL_WarpMouseInWindow(window, sx, sy);
					}
				}
				break;
				case SDL_USEREVENT_POINTER_SET:
					sdl_Pointer_Set_Process(&windowEvent.user);
					break;
				case SDL_USEREVENT_QUIT:
				default:
					break;
			}
		}
	}

	rc = 1;

	sdl_cleanup_sdl(sdl);
	return rc;
}

/* Called after a RDP connection was successfully established.
 * Settings might have changed during negociation of client / server feature
 * support.
 *
 * Set up local framebuffers and paing callbacks.
 * If required, register pointer callbacks to change the local mouse cursor
 * when hovering over the RDP window
 */
static BOOL sdl_post_connect(freerdp* instance)
{
	WINPR_ASSERT(instance);

	auto context = instance->context;
	WINPR_ASSERT(context);

	auto sdl = get_context(context);

	if (freerdp_settings_get_bool(context->settings, FreeRDP_AuthenticationOnly))
	{
		/* Check +auth-only has a username and password. */
		if (!freerdp_settings_get_string(context->settings, FreeRDP_Password))
		{
			WLog_Print(sdl->log, WLOG_INFO, "auth-only, but no password set. Please provide one.");
			return FALSE;
		}

		WLog_Print(sdl->log, WLOG_INFO, "Authentication only. Don't connect to X.");
		return TRUE;
	}

	if (!sdl_wait_create_windows(sdl))
		return FALSE;

	sdl->sdl_pixel_format = SDL_PIXELFORMAT_BGRA32;
	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
		return FALSE;

	if (!sdl_create_primary(sdl))
		return FALSE;

	if (!sdl_register_pointer(instance->context->graphics))
		return FALSE;

	WINPR_ASSERT(context->update);

	context->update->BeginPaint = sdl_begin_paint;
	context->update->EndPaint = sdl_end_paint;
	context->update->PlaySound = sdl_play_sound;
	context->update->DesktopResize = sdl_desktop_resize;
	context->update->SetKeyboardIndicators = sdlInput::keyboard_set_indicators;
	context->update->SetKeyboardImeStatus = sdlInput::keyboard_set_ime_status;

	sdl->update_resizeable(FALSE);
	sdl->update_fullscreen(freerdp_settings_get_bool(context->settings, FreeRDP_Fullscreen) ||
	                       freerdp_settings_get_bool(context->settings, FreeRDP_UseMultimon));
	return TRUE;
}

/* This function is called whether a session ends by failure or success.
 * Clean up everything allocated by pre_connect and post_connect.
 */
static void sdl_post_disconnect(freerdp* instance)
{
	if (!instance)
		return;

	if (!instance->context)
		return;

	auto context = get_context(instance->context);
	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   sdl_OnChannelConnectedEventHandler);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      sdl_OnChannelDisconnectedEventHandler);
	gdi_free(instance);
	/* TODO : Clean up custom stuff */
	WINPR_UNUSED(context);
}

static void sdl_post_final_disconnect(freerdp* instance)
{
	if (!instance)
		return;

	if (!instance->context)
		return;
}

/* RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends. */
static DWORD WINAPI sdl_client_thread_proc(SdlContext* sdl)
{
	DWORD nCount;
	DWORD status;
	int exit_code = SDL_EXIT_SUCCESS;
	HANDLE handles[MAXIMUM_WAIT_OBJECTS] = {};

	WINPR_ASSERT(sdl);

	auto instance = sdl->context()->instance;
	WINPR_ASSERT(instance);

	BOOL rc = freerdp_connect(instance);

	rdpContext* context = sdl->context();
	rdpSettings* settings = context->settings;
	WINPR_ASSERT(settings);

	if (!rc)
	{
		UINT32 error = freerdp_get_last_error(context);
		exit_code = sdl_map_error_to_exit_code(error);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
	{
		DWORD code = freerdp_get_last_error(context);
		freerdp_abort_connect_context(context);
		WLog_Print(sdl->log, WLOG_ERROR,
		           "Authentication only, freerdp_get_last_error() %s [0x%08" PRIx32 "] %s",
		           freerdp_get_last_error_name(code), code, freerdp_get_last_error_string(code));
		goto terminate;
	}

	if (!rc)
	{
		DWORD code = freerdp_error_info(instance);
		if (exit_code == SDL_EXIT_SUCCESS)
			exit_code = error_info_to_error(instance, &code);

		if (freerdp_get_last_error(context) == FREERDP_ERROR_AUTHENTICATION_FAILED)
			exit_code = SDL_EXIT_AUTH_FAILURE;
		else if (code == ERRINFO_SUCCESS)
			exit_code = SDL_EXIT_CONN_FAILED;

		goto terminate;
	}

	while (!freerdp_shall_disconnect_context(context))
	{
		/*
		 * win8 and server 2k12 seem to have some timing issue/race condition
		 * when a initial sync request is send to sync the keyboard indicators
		 * sending the sync event twice fixed this problem
		 */
		if (freerdp_focus_required(instance))
		{
			auto ctx = get_context(context);
			WINPR_ASSERT(ctx);
			if (!ctx->input.keyboard_focus_in())
				break;
			if (!ctx->input.keyboard_focus_in())
				break;
		}

		nCount = freerdp_get_event_handles(context, handles, ARRAYSIZE(handles));

		if (nCount == 0)
		{
			WLog_Print(sdl->log, WLOG_ERROR, "freerdp_get_event_handles failed");
			break;
		}

		status = WaitForMultipleObjects(nCount, handles, FALSE, 100);

		if (status == WAIT_FAILED)
		{
			if (client_auto_reconnect(instance))
				continue;
			else
			{
				/*
				 * Indicate an unsuccessful connection attempt if reconnect
				 * did not succeed and no other error was specified.
				 */
				if (freerdp_error_info(instance) == 0)
					exit_code = SDL_EXIT_CONN_FAILED;
			}

			if (freerdp_get_last_error(context) == FREERDP_ERROR_SUCCESS)
				WLog_Print(sdl->log, WLOG_ERROR, "WaitForMultipleObjects failed with %" PRIu32 "",
				           status);
			break;
		}

		if (!freerdp_check_event_handles(context))
		{
			if (freerdp_get_last_error(context) == FREERDP_ERROR_SUCCESS)
				WLog_Print(sdl->log, WLOG_ERROR, "Failed to check FreeRDP event handles");

			break;
		}
	}

	if (exit_code == SDL_EXIT_SUCCESS)
	{
		DWORD code = 0;
		exit_code = error_info_to_error(instance, &code);

		if ((code == ERRINFO_LOGOFF_BY_USER) &&
		    (freerdp_get_disconnect_ultimatum(context) == Disconnect_Ultimatum_user_requested))
		{
			/* This situation might be limited to Windows XP. */
			WLog_Print(sdl->log, WLOG_INFO,
			           "Error info says user did not initiate but disconnect ultimatum says "
			           "they did; treat this as a user logoff");
			exit_code = SDL_EXIT_LOGOFF;
		}
	}

	freerdp_disconnect(instance);

terminate:
	if (freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
		WLog_Print(sdl->log, WLOG_INFO, "Authentication only, exit status %s [%" PRId32 "]",
		           sdl_map_to_code_tag(exit_code), exit_code);

	sdl->exit_code = exit_code;
	sdl_push_user_event(SDL_USEREVENT_QUIT);
#if SDL_VERSION_ATLEAST(2, 0, 16)
	SDL_TLSCleanup();
#endif
	return 0;
}

/* Optional global initializer.
 * Here we just register a signal handler to print out stack traces
 * if available. */
static BOOL sdl_client_global_init(void)
{
#if defined(_WIN32)
	WSADATA wsaData = { 0 };
	const DWORD wVersionRequested = MAKEWORD(1, 1);
	const int rc = WSAStartup(wVersionRequested, &wsaData);
	if (rc != 0)
	{
		WLog_ERR(SDL_TAG, "WSAStartup failed with %s [%d]", gai_strerrorA(rc), rc);
		return FALSE;
	}
#endif

	if (freerdp_handle_signals() != 0)
		return FALSE;

	return TRUE;
}

/* Optional global tear down */
static void sdl_client_global_uninit(void)
{
#if defined(_WIN32)
	WSACleanup();
#endif
}

static BOOL sdl_client_new(freerdp* instance, rdpContext* context)
{
	auto sdl = reinterpret_cast<sdl_rdp_context*>(context);

	if (!instance || !context)
		return FALSE;

	sdl->sdl = new SdlContext(context);
	if (!sdl->sdl)
		return FALSE;

	instance->PreConnect = sdl_pre_connect;
	instance->PostConnect = sdl_post_connect;
	instance->PostDisconnect = sdl_post_disconnect;
	instance->PostFinalDisconnect = sdl_post_final_disconnect;
	instance->AuthenticateEx = sdl_authenticate_ex;
	instance->VerifyCertificateEx = sdl_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = sdl_verify_changed_certificate_ex;
	instance->LogonErrorInfo = sdl_logon_error_info;
	instance->PresentGatewayMessage = sdl_present_gateway_message;

#ifdef WITH_WEBVIEW
	instance->GetAccessToken = sdl_webview_get_access_token;
#else
	instance->GetAccessToken = client_cli_get_access_token;
#endif
	/* TODO: Client display set up */

	return TRUE;
}

static void sdl_client_free(freerdp* instance, rdpContext* context)
{
	auto sdl = reinterpret_cast<sdl_rdp_context*>(context);

	if (!context)
		return;

	delete sdl->sdl;
}

static int sdl_client_start(rdpContext* context)
{
	auto sdl = get_context(context);
	WINPR_ASSERT(sdl);

	sdl->thread = std::thread(sdl_client_thread_proc, sdl);
	return 0;
}

static int sdl_client_stop(rdpContext* context)
{
	auto sdl = get_context(context);
	WINPR_ASSERT(sdl);

	/* We do not want to use freerdp_abort_connect_context here.
	 * It would change the exit code and we do not want that. */
	HANDLE event = freerdp_abort_event(context);
	if (!SetEvent(event))
		return -1;

	sdl->thread.join();
	return 0;
}

static int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	WINPR_ASSERT(pEntryPoints);

	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = sdl_client_global_init;
	pEntryPoints->GlobalUninit = sdl_client_global_uninit;
	pEntryPoints->ContextSize = sizeof(sdl_rdp_context);
	pEntryPoints->ClientNew = sdl_client_new;
	pEntryPoints->ClientFree = sdl_client_free;
	pEntryPoints->ClientStart = sdl_client_start;
	pEntryPoints->ClientStop = sdl_client_stop;
	return 0;
}

static void context_free(sdl_rdp_context* sdl)
{
	if (sdl)
		freerdp_client_context_free(&sdl->common.context);
}

static const char* category2str(int category)
{
	switch (category)
	{
		case SDL_LOG_CATEGORY_APPLICATION:
			return "SDL_LOG_CATEGORY_APPLICATION";
		case SDL_LOG_CATEGORY_ERROR:
			return "SDL_LOG_CATEGORY_ERROR";
		case SDL_LOG_CATEGORY_ASSERT:
			return "SDL_LOG_CATEGORY_ASSERT";
		case SDL_LOG_CATEGORY_SYSTEM:
			return "SDL_LOG_CATEGORY_SYSTEM";
		case SDL_LOG_CATEGORY_AUDIO:
			return "SDL_LOG_CATEGORY_AUDIO";
		case SDL_LOG_CATEGORY_VIDEO:
			return "SDL_LOG_CATEGORY_VIDEO";
		case SDL_LOG_CATEGORY_RENDER:
			return "SDL_LOG_CATEGORY_RENDER";
		case SDL_LOG_CATEGORY_INPUT:
			return "SDL_LOG_CATEGORY_INPUT";
		case SDL_LOG_CATEGORY_TEST:
			return "SDL_LOG_CATEGORY_TEST";
		case SDL_LOG_CATEGORY_RESERVED1:
			return "SDL_LOG_CATEGORY_RESERVED1";
		case SDL_LOG_CATEGORY_RESERVED2:
			return "SDL_LOG_CATEGORY_RESERVED2";
		case SDL_LOG_CATEGORY_RESERVED3:
			return "SDL_LOG_CATEGORY_RESERVED3";
		case SDL_LOG_CATEGORY_RESERVED4:
			return "SDL_LOG_CATEGORY_RESERVED4";
		case SDL_LOG_CATEGORY_RESERVED5:
			return "SDL_LOG_CATEGORY_RESERVED5";
		case SDL_LOG_CATEGORY_RESERVED6:
			return "SDL_LOG_CATEGORY_RESERVED6";
		case SDL_LOG_CATEGORY_RESERVED7:
			return "SDL_LOG_CATEGORY_RESERVED7";
		case SDL_LOG_CATEGORY_RESERVED8:
			return "SDL_LOG_CATEGORY_RESERVED8";
		case SDL_LOG_CATEGORY_RESERVED9:
			return "SDL_LOG_CATEGORY_RESERVED9";
		case SDL_LOG_CATEGORY_RESERVED10:
			return "SDL_LOG_CATEGORY_RESERVED10";
		case SDL_LOG_CATEGORY_CUSTOM:
		default:
			return "SDL_LOG_CATEGORY_CUSTOM";
	}
}

static SDL_LogPriority wloglevel2dl(DWORD level)
{
	switch (level)
	{
		case WLOG_TRACE:
			return SDL_LOG_PRIORITY_VERBOSE;
		case WLOG_DEBUG:
			return SDL_LOG_PRIORITY_DEBUG;
		case WLOG_INFO:
			return SDL_LOG_PRIORITY_INFO;
		case WLOG_WARN:
			return SDL_LOG_PRIORITY_WARN;
		case WLOG_ERROR:
			return SDL_LOG_PRIORITY_ERROR;
		case WLOG_FATAL:
			return SDL_LOG_PRIORITY_CRITICAL;
		case WLOG_OFF:
		default:
			return SDL_LOG_PRIORITY_VERBOSE;
	}
}

static DWORD sdlpriority2wlog(SDL_LogPriority priority)
{
	DWORD level = WLOG_OFF;
	switch (priority)
	{
		case SDL_LOG_PRIORITY_VERBOSE:
			level = WLOG_TRACE;
			break;
		case SDL_LOG_PRIORITY_DEBUG:
			level = WLOG_DEBUG;
			break;
		case SDL_LOG_PRIORITY_INFO:
			level = WLOG_INFO;
			break;
		case SDL_LOG_PRIORITY_WARN:
			level = WLOG_WARN;
			break;
		case SDL_LOG_PRIORITY_ERROR:
			level = WLOG_ERROR;
			break;
		case SDL_LOG_PRIORITY_CRITICAL:
			level = WLOG_FATAL;
			break;
		default:
			break;
	}

	return level;
}

static void SDLCALL winpr_LogOutputFunction(void* userdata, int category, SDL_LogPriority priority,
                                            const char* message)
{
	auto sdl = static_cast<SdlContext*>(userdata);
	WINPR_ASSERT(sdl);

	const DWORD level = sdlpriority2wlog(priority);
	auto log = sdl->log;
	if (!WLog_IsLevelActive(log, level))
		return;

	WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, level, __LINE__, __FILE__, __func__, "[%s] %s",
	                  category2str(category), message);
}

int main(int argc, char* argv[])
{
	int rc = -1;
	int status;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = {};

	freerdp_client_warn_experimental(argc, argv);

	RdpClientEntry(&clientEntryPoints);
	std::unique_ptr<sdl_rdp_context, void (*)(sdl_rdp_context*)> sdl_rdp(
	    reinterpret_cast<sdl_rdp_context*>(freerdp_client_context_new(&clientEntryPoints)),
	    context_free);

	if (!sdl_rdp)
		return -1;
	auto sdl = sdl_rdp->sdl;

	auto settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	status = freerdp_client_settings_parse_command_line(settings, argc, argv, FALSE);
	if (status)
	{
		rc = freerdp_client_settings_command_line_status_print(settings, status, argc, argv);
		if (freerdp_settings_get_bool(settings, FreeRDP_ListMonitors))
			sdl_list_monitors(sdl);
		return rc;
	}

	SDL_LogSetOutputFunction(winpr_LogOutputFunction, sdl);
	auto level = WLog_GetLogLevel(sdl->log);
	SDL_LogSetAllPriority(wloglevel2dl(level));

	auto context = sdl->context();
	WINPR_ASSERT(context);

	if (!stream_dump_register_handlers(context, CONNECTION_STATE_MCS_CREATE_REQUEST, FALSE))
		return -1;

	if (freerdp_client_start(context) != 0)
		return -1;

	rc = sdl_run(sdl);

	if (freerdp_client_stop(context) != 0)
		return -1;

	rc = sdl->exit_code;

	return rc;
}

BOOL SdlContext::update_fullscreen(BOOL enter)
{
	std::lock_guard<CriticalSection> lock(critical);
	for (const auto& window : windows)
	{
		if (!sdl_push_user_event(SDL_USEREVENT_WINDOW_FULLSCREEN, window.window, enter))
			return FALSE;
	}
	fullscreen = enter;
	return TRUE;
}

BOOL SdlContext::update_resizeable(BOOL enable)
{
	std::lock_guard<CriticalSection> lock(critical);

	const auto settings = context()->settings;
	const BOOL dyn = freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate);
	const BOOL smart = freerdp_settings_get_bool(settings, FreeRDP_SmartSizing);
	BOOL use = (dyn && enable) || smart;

	for (const auto& window : windows)
	{
		if (!sdl_push_user_event(SDL_USEREVENT_WINDOW_RESIZEABLE, window.window, use))
			return FALSE;
	}
	resizeable = use;

	return TRUE;
}

SdlContext::SdlContext(rdpContext* context)
    : _context(context), log(WLog_Get(SDL_TAG)), update_complete(true), disp(this), input(this),
      primary(nullptr, SDL_FreeSurface), primary_format(nullptr, SDL_FreeFormat)
{
}

rdpContext* SdlContext::context() const
{
	return _context;
}

rdpClientContext* SdlContext::common() const
{
	return reinterpret_cast<rdpClientContext*>(_context);
}
