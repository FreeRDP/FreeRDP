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

#include <iostream>
#include <memory>
#include <mutex>

#include <freerdp/config.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <freerdp/constants.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/streamdump.h>
#include <freerdp/utils/signal.h>

#include <freerdp/channels/channels.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/file.h>

#include <freerdp/log.h>
#include <winpr/assert.h>
#include <winpr/config.h>
#include <winpr/crt.h>
#include <winpr/synch.h>

#include <SDL3/SDL.h>
#if !defined(__MINGW32__)
#include <SDL3/SDL_main.h>
#endif
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_video.h>

#include <sdl_config.hpp>

#include "dialogs/sdl_connection_dialog_hider.hpp"
#include "dialogs/sdl_dialogs.hpp"
#include "scoped_guard.hpp"
#include "sdl_channels.hpp"
#include "sdl_disp.hpp"
#include "sdl_freerdp.hpp"
#include "sdl_kbd.hpp"
#include "sdl_monitor.hpp"
#include "sdl_pointer.hpp"
#include "sdl_prefs.hpp"
#include "sdl_touch.hpp"
#include "sdl_utils.hpp"

#if defined(WITH_WEBVIEW)
#include <aad/sdl_webview.hpp>
#endif

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
	SDL_EXIT_CONNECT_TARGET_BOOTING = 160,

	SDL_EXIT_UNKNOWN = 255,
};

struct sdl_exit_code_map_t
{
	DWORD error;
	int code;
	const char* code_tag;
};

#define ENTRY(x, y) { x, y, #y }
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
	ENTRY(FREERDP_ERROR_CONNECT_TARGET_BOOTING, SDL_EXIT_CONNECT_TARGET_BOOTING),
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
	for (const auto& x : sdl_exit_code_map)
	{
		const struct sdl_exit_code_map_t* cur = &x;
		if (cur->code == exit_code)
			return cur;
	}
	return nullptr;
}

static const struct sdl_exit_code_map_t* sdl_map_entry_by_error(UINT32 error)
{
	for (const auto& x : sdl_exit_code_map)
	{
		const struct sdl_exit_code_map_t* cur = &x;
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

static const char* sdl_map_error_to_code_tag(UINT32 error)
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

static int error_info_to_error(freerdp* instance, DWORD* pcode, char** msg, size_t* len)
{
	const DWORD code = freerdp_error_info(instance);
	const char* name = freerdp_get_error_info_name(code);
	const char* str = freerdp_get_error_info_string(code);
	const int exit_code = sdl_map_error_to_exit_code(code);

	winpr_asprintf(msg, len, "Terminate with %s due to ERROR_INFO %s [0x%08" PRIx32 "]: %s",
	               sdl_map_error_to_code_tag(code), name, code, str);
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", *msg);
	if (pcode)
		*pcode = code;
	return exit_code;
}

/* This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas. */
static BOOL sdl_begin_paint(rdpContext* context)
{
	auto gdi = context->gdi;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(gdi->primary);

	HGDI_DC hdc = gdi->primary->hdc;
	WINPR_ASSERT(hdc);
	if (!hdc->hwnd)
		return TRUE;

	HGDI_WND hwnd = hdc->hwnd;
	WINPR_ASSERT(hwnd->invalid);
	hwnd->invalid->null = TRUE;
	hwnd->ninvalid = 0;

	return TRUE;
}

static bool sdl_draw_to_window_rect([[maybe_unused]] SdlContext* sdl, SdlWindow& window,
                                    SDL_Surface* surface, SDL_Point offset, const SDL_Rect& srcRect)
{
	WINPR_ASSERT(surface);
	SDL_Rect dstRect = { offset.x + srcRect.x, offset.y + srcRect.y, srcRect.w, srcRect.h };
	return window.blit(surface, srcRect, dstRect);
}

static bool sdl_draw_to_window_rect(SdlContext* sdl, SdlWindow& window, SDL_Surface* surface,
                                    SDL_Point offset, const std::vector<SDL_Rect>& rects = {})
{
	if (rects.empty())
	{
		return sdl_draw_to_window_rect(sdl, window, surface, offset,
		                               { 0, 0, surface->w, surface->h });
	}
	for (auto& srcRect : rects)
	{
		if (!sdl_draw_to_window_rect(sdl, window, surface, offset, srcRect))
			return false;
	}
	return true;
}

static bool sdl_draw_to_window_scaled_rect(SdlContext* sdl, SdlWindow& window, SDL_Surface* surface,
                                           const SDL_Rect& srcRect)
{
	SDL_Rect dstRect = srcRect;
	sdl_scale_coordinates(sdl, window.id(), &dstRect.x, &dstRect.y, FALSE, TRUE);
	sdl_scale_coordinates(sdl, window.id(), &dstRect.w, &dstRect.h, FALSE, TRUE);
	return window.blit(surface, srcRect, dstRect);
}

static BOOL sdl_draw_to_window_scaled_rect(SdlContext* sdl, SdlWindow& window, SDL_Surface* surface,
                                           const std::vector<SDL_Rect>& rects = {})
{
	if (rects.empty())
	{
		return sdl_draw_to_window_scaled_rect(sdl, window, surface,
		                                      { 0, 0, surface->w, surface->h });
	}
	for (const auto& srcRect : rects)
	{
		if (!sdl_draw_to_window_scaled_rect(sdl, window, surface, srcRect))
			return FALSE;
	}
	return TRUE;
}

static BOOL sdl_draw_to_window(SdlContext* sdl, SdlWindow& window,
                               const std::vector<SDL_Rect>& rects = {})
{
	WINPR_ASSERT(sdl);

	if (!sdl->isConnected())
		return TRUE;

	auto context = sdl->context();
	auto gdi = context->gdi;
	WINPR_ASSERT(gdi);

	auto size = window.rect();

	std::unique_lock lock(sdl->critical);
	auto surface = sdl->primary.get();
	if (freerdp_settings_get_bool(context->settings, FreeRDP_SmartSizing))
	{
		window.setOffsetX(0);
		window.setOffsetY(0);
		if (gdi->width < size.w)
		{
			window.setOffsetX((size.w - gdi->width) / 2);
		}
		if (gdi->height < size.h)
		{
			window.setOffsetY((size.h - gdi->height) / 2);
		}
		if (!sdl_draw_to_window_scaled_rect(sdl, window, surface, rects))
			return FALSE;
	}
	else
	{

		if (!sdl_draw_to_window_rect(sdl, window, surface, { window.offsetX(), window.offsetY() },
		                             rects))
			return FALSE;
	}

	window.updateSurface();
	return TRUE;
}

static BOOL sdl_draw_to_window(SdlContext* sdl, std::map<Uint32, SdlWindow>& windows,
                               const std::vector<SDL_Rect>& rects = {})
{
	for (auto& window : windows)
	{
		if (!sdl_draw_to_window(sdl, window.second, rects))
			return FALSE;
	}

	return TRUE;
}

/* This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
static BOOL sdl_end_paint(rdpContext* context)
{
	auto sdl = get_context(context);
	WINPR_ASSERT(sdl);

	auto gdi = context->gdi;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(gdi->primary);

	HGDI_DC hdc = gdi->primary->hdc;
	WINPR_ASSERT(hdc);
	if (!hdc->hwnd)
		return TRUE;

	HGDI_WND hwnd = hdc->hwnd;
	WINPR_ASSERT(hwnd->invalid || (hwnd->ninvalid == 0));

	if (hwnd->invalid->null)
		return TRUE;

	WINPR_ASSERT(hwnd->invalid);
	if (gdi->suppressOutput || hwnd->invalid->null)
		return TRUE;

	const INT32 ninvalid = hwnd->ninvalid;
	const GDI_RGN* cinvalid = hwnd->cinvalid;

	if (ninvalid < 1)
		return TRUE;

	std::vector<SDL_Rect> rects;
	for (INT32 x = 0; x < ninvalid; x++)
	{
		auto& rgn = cinvalid[x];
		rects.push_back({ rgn.x, rgn.y, rgn.w, rgn.h });
	}

	sdl->push(std::move(rects));
	return sdl_push_user_event(SDL_EVENT_USER_UPDATE);
}

static void sdl_destroy_primary(SdlContext* sdl)
{
	if (!sdl)
		return;
	sdl->primary.reset();
}

/* Create a SDL surface from the GDI buffer */
static BOOL sdl_create_primary(SdlContext* sdl)
{
	rdpGdi* gdi = nullptr;

	WINPR_ASSERT(sdl);

	gdi = sdl->context()->gdi;
	WINPR_ASSERT(gdi);

	sdl_destroy_primary(sdl);
	sdl->primary =
	    SDLSurfacePtr(SDL_CreateSurfaceFrom(static_cast<int>(gdi->width),
	                                        static_cast<int>(gdi->height), sdl->sdl_pixel_format,
	                                        gdi->primary_buffer, static_cast<int>(gdi->stride)),
	                  SDL_DestroySurface);
	if (!sdl->primary)
		return FALSE;

	SDL_SetSurfaceBlendMode(sdl->primary.get(), SDL_BLENDMODE_NONE);
	SDL_Rect surfaceRect = { 0, 0, gdi->width, gdi->height };
	SDL_FillSurfaceRect(sdl->primary.get(), &surfaceRect,
	                    SDL_MapSurfaceRGBA(sdl->primary.get(), 0, 0, 0, 0xff));

	return TRUE;
}

static BOOL sdl_desktop_resize(rdpContext* context)
{
	rdpGdi* gdi = nullptr;
	rdpSettings* settings = nullptr;
	auto sdl = get_context(context);

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	std::unique_lock lock(sdl->critical);
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

	if (!freerdp_settings_set_bool(settings, FreeRDP_CertificateCallbackPreferPEM, TRUE))
		return FALSE;

	/* Optional OS identifier sent to server */
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMAJORTYPE_UNIX))
		return FALSE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMinorType, OSMINORTYPE_NATIVE_SDL))
		return FALSE;
	/* OrderSupport is initialized at this point.
	 * Only override it if you plan to implement custom order
	 * callbacks or deactivate certain features. */
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

	if (!sdl->input.initialize())
		return FALSE;

	/* TODO: Any code your client requires */
	return TRUE;
}

static const char* sdl_window_get_title(rdpSettings* settings)
{
	const char* windowTitle = nullptr;
	UINT32 port = 0;
	BOOL addPort = 0;
	const char* name = nullptr;
	const char* prefix = "FreeRDP:";

	if (!settings)
		return nullptr;

	windowTitle = freerdp_settings_get_string(settings, FreeRDP_WindowTitle);
	if (windowTitle)
		return windowTitle;

	name = freerdp_settings_get_server_name(settings);
	port = freerdp_settings_get_uint32(settings, FreeRDP_ServerPort);

	addPort = (port != 3389);

	char buffer[MAX_PATH + 64] = {};

	if (!addPort)
		(void)sprintf_s(buffer, sizeof(buffer), "%s %s", prefix, name);
	else
		(void)sprintf_s(buffer, sizeof(buffer), "%s %s:%" PRIu32, prefix, name, port);

	if (!freerdp_settings_set_string(settings, FreeRDP_WindowTitle, buffer))
		return nullptr;
	return freerdp_settings_get_string(settings, FreeRDP_WindowTitle);
}

static void sdl_term_handler([[maybe_unused]] int signum, [[maybe_unused]] const char* signame,
                             [[maybe_unused]] void* context)
{
	sdl_push_quit();
}

static void sdl_cleanup_sdl(SdlContext* sdl)
{
	if (!sdl)
		return;

	std::unique_lock lock(sdl->critical);
	sdl->windows.clear();
	sdl->dialog.destroy();

	sdl_destroy_primary(sdl);

	freerdp_del_signal_cleanup_handler(sdl->context(), sdl_term_handler);
	sdl_dialogs_uninit();
	SDL_Quit();
}

static BOOL sdl_create_windows(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	auto settings = sdl->context()->settings;
	auto title = sdl_window_get_title(settings);

	ScopeGuard guard1([&]() { sdl->windows_created.set(); });

	UINT32 windowCount = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);

	Sint32 originX = 0;
	Sint32 originY = 0;
	for (UINT32 x = 0; x < windowCount; x++)
	{
		auto id = sdl->monitorId(x);
		if (id < 0)
			return FALSE;

		auto monitor = static_cast<rdpMonitor*>(
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, x));

		originX = std::min<Sint32>(monitor->x, originX);
		originY = std::min<Sint32>(monitor->y, originY);
	}

	for (UINT32 x = 0; x < windowCount; x++)
	{
		auto id = sdl->monitorId(x);
		if (id < 0)
			return FALSE;

		auto monitor = static_cast<rdpMonitor*>(
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, x));

		auto w = WINPR_ASSERTING_INT_CAST(Uint32, monitor->width);
		auto h = WINPR_ASSERTING_INT_CAST(Uint32, monitor->height);
		if (!(freerdp_settings_get_bool(settings, FreeRDP_UseMultimon) ||
		      freerdp_settings_get_bool(settings, FreeRDP_Fullscreen)))
		{
			w = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
			h = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
		}

		Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
		auto startupX = SDL_WINDOWPOS_CENTERED_DISPLAY(id);
		auto startupY = SDL_WINDOWPOS_CENTERED_DISPLAY(id);

		if (freerdp_settings_get_bool(settings, FreeRDP_Fullscreen) &&
		    !freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		{
			flags |= SDL_WINDOW_FULLSCREEN;
		}

		if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		{
			flags |= SDL_WINDOW_BORDERLESS;
		}

		if (!freerdp_settings_get_bool(settings, FreeRDP_Decorations))
			flags |= SDL_WINDOW_BORDERLESS;

		char buffer[MAX_PATH + 64] = {};
		(void)sprintf_s(buffer, sizeof(buffer), "%s:%" PRIu32, title, x);
		SdlWindow window{ buffer,
			              static_cast<int>(startupX),
			              static_cast<int>(startupY),
			              static_cast<int>(w),
			              static_cast<int>(h),
			              flags };
		if (!window.window())
			return FALSE;

		if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		{
			window.setOffsetX(originX - monitor->x);
			window.setOffsetY(originY - monitor->y);
		}

		sdl->windows.insert({ window.id(), std::move(window) });
	}

	return TRUE;
}

static BOOL sdl_wait_create_windows(SdlContext* sdl)
{
	{
		std::unique_lock<CriticalSection> lock(sdl->critical);
		sdl->windows_created.clear();
		if (!sdl_push_user_event(SDL_EVENT_USER_CREATE_WINDOWS, sdl))
			return FALSE;
	}

	HANDLE handles[] = { sdl->windows_created.handle(), freerdp_abort_event(sdl->context()) };

	const DWORD rc = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
	switch (rc)
	{
		case WAIT_OBJECT_0:
			return TRUE;
		default:
			return FALSE;
	}
}

static bool shall_abort(SdlContext* sdl)
{
	std::unique_lock lock(sdl->critical);
	if (freerdp_shall_disconnect_context(sdl->context()))
	{
		if (sdl->rdp_thread_running)
			return false;
		return !sdl->dialog.isRunning();
	}
	return false;
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
			return 0;
	}

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	auto backend = SDL_GetCurrentVideoDriver();
	WLog_Print(sdl->log, WLOG_DEBUG, "client is using backend '%s'", backend);
	sdl_dialogs_init();

	SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
	SDL_SetHint(SDL_HINT_PEN_MOUSE_EVENTS, "0");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	SDL_SetHint(SDL_HINT_PEN_TOUCH_EVENTS, "1");
	SDL_SetHint(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, "1");

	freerdp_add_signal_cleanup_handler(sdl->context(), sdl_term_handler);
	sdl->dialog.create(sdl->context());
	sdl->dialog.setTitle("Connecting to '%s'",
	                     freerdp_settings_get_server_name(sdl->context()->settings));
	sdl->dialog.showInfo("The connection is being established\n\nPlease wait...");
	if (!freerdp_settings_get_bool(sdl->context()->settings, FreeRDP_UseCommonStdioCallbacks))
	{
		sdl->dialog.show(true);
	}

	sdl->initialized.set();

	while (!shall_abort(sdl))
	{
		SDL_Event windowEvent = {};
		while (!shall_abort(sdl) && SDL_WaitEventTimeout(nullptr, 1000))
		{
			/* Only poll standard SDL events and SDL_EVENT_USERS meant to create
			 * dialogs. do not process the dialog return value events here.
			 */
			const int prc = SDL_PeepEvents(&windowEvent, 1, SDL_GETEVENT, SDL_EVENT_FIRST,
			                               SDL_EVENT_USER_RETRY_DIALOG);
			if (prc < 0)
			{
				if (sdl_log_error(prc, sdl->log, "SDL_PeepEvents"))
					continue;
			}

#if defined(WITH_DEBUG_SDL_EVENTS)
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "got event %s [0x%08" PRIx32 "]",
			             sdl_event_type_str(windowEvent.type), windowEvent.type);
#endif
			{
				std::unique_lock lock(sdl->critical);
				/* The session might have been disconnected while we were waiting for a
				 * new SDL event. In that case ignore the SDL event and terminate. */
				if (freerdp_shall_disconnect_context(sdl->context()))
					continue;
			}

			if (sdl->dialog.handleEvent(windowEvent))
			{
				continue;
			}

			auto point2pix = [](Uint32 win_id, float& x, float& y)
			{
				auto win = SDL_GetWindowFromID(win_id);
				if (win)
				{
					auto scale = SDL_GetWindowDisplayScale(win);
					assert(scale);
					x *= scale;
					y *= scale;
				}
			};

			switch (windowEvent.type)
			{
				case SDL_EVENT_QUIT:
					freerdp_abort_connect_context(sdl->context());
					break;
				case SDL_EVENT_KEY_DOWN:
				case SDL_EVENT_KEY_UP:
				{
					const SDL_KeyboardEvent* ev = &windowEvent.key;
					sdl->input.keyboard_handle_event(ev);
				}
				break;
				case SDL_EVENT_KEYMAP_CHANGED:
				{
				}
				break; // TODO: Switch keyboard layout
				case SDL_EVENT_MOUSE_MOTION:
				{
					SDL_MouseMotionEvent& ev = windowEvent.motion;
					point2pix(ev.windowID, ev.x, ev.y);
					point2pix(ev.windowID, ev.xrel, ev.yrel);
					sdl_handle_mouse_motion(sdl, &ev);
				}
				break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
				case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					SDL_MouseButtonEvent& ev = windowEvent.button;
					point2pix(ev.windowID, ev.x, ev.y);
					sdl_handle_mouse_button(sdl, &ev);
				}
				break;
				case SDL_EVENT_MOUSE_WHEEL:
				{
					const SDL_MouseWheelEvent* ev = &windowEvent.wheel;
					sdl_handle_mouse_wheel(sdl, ev);
				}
				break;
				case SDL_EVENT_FINGER_DOWN:
				{
					const SDL_TouchFingerEvent* ev = &windowEvent.tfinger;
					sdl_handle_touch_down(sdl, ev);
				}
				break;
				case SDL_EVENT_FINGER_UP:
				{
					const SDL_TouchFingerEvent* ev = &windowEvent.tfinger;
					sdl_handle_touch_up(sdl, ev);
				}
				break;
				case SDL_EVENT_FINGER_MOTION:
				{
					const SDL_TouchFingerEvent* ev = &windowEvent.tfinger;
					sdl_handle_touch_motion(sdl, ev);
				}
				break;

				case SDL_EVENT_RENDER_TARGETS_RESET:
					(void)sdl->redraw();
					break;
				case SDL_EVENT_RENDER_DEVICE_RESET:
					(void)sdl->redraw();
					break;
				case SDL_EVENT_WILL_ENTER_FOREGROUND:
					(void)sdl->redraw();
					break;
				case SDL_EVENT_USER_CERT_DIALOG:
				{
					SDLConnectionDialogHider hider(sdl);
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char*>(windowEvent.user.data2);
					sdl_cert_dialog_show(title, msg);
				}
				break;
				case SDL_EVENT_USER_SHOW_DIALOG:
				{
					SDLConnectionDialogHider hider(sdl);
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char*>(windowEvent.user.data2);
					sdl_message_dialog_show(title, msg, windowEvent.user.code);
				}
				break;
				case SDL_EVENT_USER_SCARD_DIALOG:
				{
					SDLConnectionDialogHider hider(sdl);
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char**>(windowEvent.user.data2);
					sdl_scard_dialog_show(title, windowEvent.user.code, msg);
				}
				break;
				case SDL_EVENT_USER_AUTH_DIALOG:
				{
					SDLConnectionDialogHider hider(sdl);
					sdl_auth_dialog_show(
					    reinterpret_cast<const SDL_UserAuthArg*>(windowEvent.padding));
				}
				break;
				case SDL_EVENT_USER_UPDATE:
				{
					std::vector<SDL_Rect> rectangles;
					do
					{
						rectangles = sdl->pop();
						sdl_draw_to_window(sdl, sdl->windows, rectangles);
					} while (!rectangles.empty());
				}
				break;
				case SDL_EVENT_USER_CREATE_WINDOWS:
				{
					auto ctx = static_cast<SdlContext*>(windowEvent.user.data1);
					sdl_create_windows(ctx);
				}
				break;
				case SDL_EVENT_USER_WINDOW_RESIZEABLE:
				{
					auto window = static_cast<SdlWindow*>(windowEvent.user.data1);
					const bool use = windowEvent.user.code != 0;
					if (window)
						window->resizeable(use);
				}
				break;
				case SDL_EVENT_USER_WINDOW_FULLSCREEN:
				{
					auto window = static_cast<SdlWindow*>(windowEvent.user.data1);
					const bool enter = windowEvent.user.code != 0;
					if (window)
						window->fullscreen(enter);
				}
				break;
				case SDL_EVENT_USER_WINDOW_MINIMIZE:
					for (auto& window : sdl->windows)
					{
						window.second.minimize();
					}
					break;
				case SDL_EVENT_USER_POINTER_NULL:
					SDL_HideCursor();
					sdl->setCursor(nullptr);
					sdl->setHasCursor(false);
					break;
				case SDL_EVENT_USER_POINTER_DEFAULT:
				{
					SDL_Cursor* def = SDL_GetDefaultCursor();
					SDL_SetCursor(def);
					SDL_ShowCursor();
					sdl->setCursor(nullptr);
					sdl->setHasCursor(true);
				}
				break;
				case SDL_EVENT_USER_POINTER_POSITION:
				{
					const auto x =
					    static_cast<INT32>(reinterpret_cast<uintptr_t>(windowEvent.user.data1));
					const auto y =
					    static_cast<INT32>(reinterpret_cast<uintptr_t>(windowEvent.user.data2));

					SDL_Window* window = SDL_GetMouseFocus();
					if (window)
					{
						const Uint32 id = SDL_GetWindowID(window);

						INT32 sx = x;
						INT32 sy = y;
						if (sdl_scale_coordinates(sdl, id, &sx, &sy, FALSE, FALSE))
							SDL_WarpMouseInWindow(window, static_cast<float>(sx),
							                      static_cast<float>(sy));
					}
				}
				break;
				case SDL_EVENT_USER_POINTER_SET:
					sdl->setCursor(static_cast<rdpPointer*>(windowEvent.user.data1));
					sdl_Pointer_Set_Process(sdl);
					break;
				case SDL_EVENT_CLIPBOARD_UPDATE:
					sdl->clip.handle_update(windowEvent.clipboard);
					break;
				case SDL_EVENT_USER_QUIT:
				default:
					if ((windowEvent.type >= SDL_EVENT_DISPLAY_FIRST) &&
					    (windowEvent.type <= SDL_EVENT_DISPLAY_LAST))
					{
						const SDL_DisplayEvent* ev = &windowEvent.display;
						(void)sdl->disp.handle_display_event(ev);
					}
					else if ((windowEvent.type >= SDL_EVENT_WINDOW_FIRST) &&
					         (windowEvent.type <= SDL_EVENT_WINDOW_LAST))
					{
						const SDL_WindowEvent* ev = &windowEvent.window;
						auto window = sdl->windows.find(ev->windowID);
						if (window != sdl->windows.end())
						{
							(void)sdl->disp.handle_window_event(ev);

							switch (ev->type)
							{
								case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
									if (sdl->isConnected())
									{
										sdl_Pointer_Set_Process(sdl);
										if (freerdp_settings_get_bool(
										        sdl->context()->settings,
										        FreeRDP_DynamicResolutionUpdate))
										{
											break;
										}
										auto win = window->second.window();
										int w_pix{};
										int h_pix{};
										[[maybe_unused]] auto rcpix =
										    SDL_GetWindowSizeInPixels(win, &w_pix, &h_pix);
										assert(rcpix);
										auto scale = SDL_GetWindowDisplayScale(win);
										if (scale <= SDL_FLT_EPSILON)
										{
											auto err = SDL_GetError();
											SDL_LogWarn(
											    SDL_LOG_CATEGORY_APPLICATION,
											    "SDL_GetWindowDisplayScale() failed with %s", err);
										}
										else
										{
											assert(SDL_isnanf(scale) == 0);
											assert(SDL_isinff(scale) == 0);
											assert(scale > SDL_FLT_EPSILON);
											auto w_gdi = sdl->context()->gdi->width;
											auto h_gdi = sdl->context()->gdi->height;
											auto pix2point = [=](int pix)
											{
												return static_cast<int>(static_cast<float>(pix) /
												                        scale);
											};
											if (w_pix != w_gdi || h_pix != h_gdi)
											{
												const auto ssws = SDL_SetWindowSize(
												    win, pix2point(w_gdi), pix2point(h_gdi));
												if (!ssws)
												{
													auto err = SDL_GetError();
													SDL_LogWarn(
													    SDL_LOG_CATEGORY_APPLICATION,
													    "SDL_SetWindowSize() failed with %s", err);
												}
											}
										}
									}
									break;
								case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
									window->second.fill();
									sdl_draw_to_window(sdl, window->second);
									sdl_Pointer_Set_Process(sdl);
									break;
								case SDL_EVENT_WINDOW_MOVED:
								{
									auto r = window->second.rect();
									auto id = window->second.id();
									SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%u: %dx%d-%dx%d",
									             id, r.x, r.y, r.w, r.h);
								}
								break;
								case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
								{
									SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
									             "Window closed, terminating RDP session...");
									freerdp_abort_connect_context(sdl->context());
								}
								break;
								default:
									break;
							}
						}
					}
					break;
			}
		}
	}

	rc = 1;

	sdl_cleanup_sdl(sdl);
	return rc;
}

/* Called after a RDP connection was successfully established.
 * Settings might have changed during negotiation of client / server feature
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

	// Retry was successful, discard dialog
	sdl->dialog.show(false);

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

	if (!sdl->update_resizeable(false))
		return FALSE;
	if (!sdl->update_fullscreen(freerdp_settings_get_bool(context->settings, FreeRDP_Fullscreen) ||
	                            freerdp_settings_get_bool(context->settings, FreeRDP_UseMultimon)))
		return FALSE;
	sdl->setConnected(true);
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

	auto sdl = get_context(instance->context);
	sdl->setConnected(false);

	gdi_free(instance);
}

static void sdl_post_final_disconnect(freerdp* instance)
{
	if (!instance)
		return;

	if (!instance->context)
		return;

	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   sdl_OnChannelConnectedEventHandler);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      sdl_OnChannelDisconnectedEventHandler);
}

static void sdl_client_cleanup(SdlContext* sdl, int exit_code, const std::string& error_msg)
{
	WINPR_ASSERT(sdl);

	rdpContext* context = sdl->context();
	WINPR_ASSERT(context);
	rdpSettings* settings = context->settings;
	WINPR_ASSERT(settings);

	sdl->rdp_thread_running = false;
	bool showError = false;
	if (freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
		WLog_Print(sdl->log, WLOG_INFO, "Authentication only, exit status %s [%" PRId32 "]",
		           sdl_map_to_code_tag(exit_code), exit_code);
	else
	{
		switch (exit_code)
		{
			case SDL_EXIT_SUCCESS:
			case SDL_EXIT_DISCONNECT:
			case SDL_EXIT_LOGOFF:
			case SDL_EXIT_DISCONNECT_BY_USER:
			case SDL_EXIT_CONNECT_CANCELLED:
				break;
			default:
			{
				sdl->dialog.showError(error_msg);
			}
			break;
		}
	}

	if (!showError)
		sdl->dialog.show(false);

	sdl->exit_code = exit_code;
	sdl_push_user_event(SDL_EVENT_USER_QUIT);
	SDL_CleanupTLS();
}

static int sdl_client_thread_connect(SdlContext* sdl, std::string& error_msg)
{
	WINPR_ASSERT(sdl);

	auto instance = sdl->context()->instance;
	WINPR_ASSERT(instance);

	sdl->rdp_thread_running = true;
	BOOL rc = freerdp_connect(instance);

	rdpContext* context = sdl->context();
	rdpSettings* settings = context->settings;
	WINPR_ASSERT(settings);

	int exit_code = SDL_EXIT_SUCCESS;
	if (!rc)
	{
		UINT32 error = freerdp_get_last_error(context);
		exit_code = sdl_map_error_to_exit_code(error);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
	{
		DWORD code = freerdp_get_last_error(context);
		freerdp_abort_connect_context(context);
		WLog_Print(sdl->log, WLOG_ERROR, "Authentication only, %s [0x%08" PRIx32 "] %s",
		           freerdp_get_last_error_name(code), code, freerdp_get_last_error_string(code));
		return exit_code;
	}

	if (!rc)
	{
		DWORD code = freerdp_error_info(instance);
		if (exit_code == SDL_EXIT_SUCCESS)
		{
			char* msg = nullptr;
			size_t len = 0;
			exit_code = error_info_to_error(instance, &code, &msg, &len);
			if (msg)
				error_msg = msg;
			free(msg);
		}

		auto last = freerdp_get_last_error(context);
		if (error_msg.empty())
		{
			char* msg = nullptr;
			size_t len = 0;
			winpr_asprintf(&msg, &len, "%s [0x%08" PRIx32 "]\n%s",
			               freerdp_get_last_error_name(last), last,
			               freerdp_get_last_error_string(last));
			if (msg)
				error_msg = msg;
			free(msg);
		}

		if (exit_code == SDL_EXIT_SUCCESS)
		{
			if (last == FREERDP_ERROR_AUTHENTICATION_FAILED)
				exit_code = SDL_EXIT_AUTH_FAILURE;
			else if (code == ERRINFO_SUCCESS)
				exit_code = SDL_EXIT_CONN_FAILED;
		}

		sdl->dialog.show(false);
	}

	return exit_code;
}

static int sdl_client_thread_run(SdlContext* sdl, std::string& error_msg)
{
	WINPR_ASSERT(sdl);

	auto context = sdl->context();
	WINPR_ASSERT(context);

	auto instance = context->instance;
	WINPR_ASSERT(instance);

	int exit_code = SDL_EXIT_SUCCESS;
	while (!freerdp_shall_disconnect_context(context))
	{
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = {};
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

		const DWORD nCount = freerdp_get_event_handles(context, handles, ARRAYSIZE(handles));

		if (nCount == 0)
		{
			WLog_Print(sdl->log, WLOG_ERROR, "freerdp_get_event_handles failed");
			break;
		}

		const DWORD status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
			break;

		if (!freerdp_check_event_handles(context))
		{
			if (client_auto_reconnect(instance))
			{
				// Retry was successful, discard dialog
				sdl->dialog.show(false);
				continue;
			}
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
			if (freerdp_get_last_error(context) == FREERDP_ERROR_SUCCESS)
				WLog_Print(sdl->log, WLOG_ERROR, "Failed to check FreeRDP event handles");
			break;
		}
	}

	if (exit_code == SDL_EXIT_SUCCESS)
	{
		DWORD code = 0;
		{
			char* emsg = nullptr;
			size_t elen = 0;
			exit_code = error_info_to_error(instance, &code, &emsg, &elen);
			if (emsg)
				error_msg = emsg;
			free(emsg);
		}

		if ((code == ERRINFO_LOGOFF_BY_USER) &&
		    (freerdp_get_disconnect_ultimatum(context) == Disconnect_Ultimatum_user_requested))
		{
			const char* msg = "Error info says user did not initiate but disconnect ultimatum says "
			                  "they did; treat this as a user logoff";

			char* emsg = nullptr;
			size_t elen = 0;
			winpr_asprintf(&emsg, &elen, "%s", msg);
			if (emsg)
				error_msg = emsg;
			free(emsg);

			/* This situation might be limited to Windows XP. */
			WLog_Print(sdl->log, WLOG_INFO, "%s", msg);
			exit_code = SDL_EXIT_LOGOFF;
		}
	}

	freerdp_disconnect(instance);

	return exit_code;
}

/* RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends. */
static DWORD WINAPI sdl_client_thread_proc(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	std::string error_msg;
	int exit_code = sdl_client_thread_connect(sdl, error_msg);
	if (exit_code == SDL_EXIT_SUCCESS)
		exit_code = sdl_client_thread_run(sdl, error_msg);
	sdl_client_cleanup(sdl, exit_code, error_msg);

	return static_cast<DWORD>(exit_code);
}

/* Optional global initializer.
 * Here we just register a signal handler to print out stack traces
 * if available. */
static BOOL sdl_client_global_init()
{
#if defined(_WIN32)
	WSADATA wsaData = {};
	const DWORD wVersionRequested = MAKEWORD(1, 1);
	const int rc = WSAStartup(wVersionRequested, &wsaData);
	if (rc != 0)
	{
		WLog_ERR(SDL_TAG, "WSAStartup failed with %s [%d]", gai_strerrorA(rc), rc);
		return FALSE;
	}
#endif

	return (freerdp_handle_signals() == 0);
}

/* Optional global tear down */
static void sdl_client_global_uninit()
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
	instance->ChooseSmartcard = sdl_choose_smartcard;
	instance->RetryDialog = sdl_retry_dialog;

#ifdef WITH_WEBVIEW
	instance->GetAccessToken = sdl_webview_get_access_token;
#else
	instance->GetAccessToken = client_cli_get_access_token;
#endif
	/* TODO: Client display set up */

	return TRUE;
}

static void sdl_client_free([[maybe_unused]] freerdp* instance, rdpContext* context)
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
		case SDL_LOG_CATEGORY_GPU:
			return "SDL_LOG_CATEGORY_GPU";
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

	WLog_PrintTextMessage(log, level, __LINE__, __FILE__, __func__, "[%s] %s",
	                      category2str(category), message);
}

static void sdl_quit()
{
	SDL_Event ev = {};
	ev.type = SDL_EVENT_QUIT;
	if (!SDL_PushEvent(&ev))
	{
		SDL_Log("An error occurred: %s", SDL_GetError());
	}
}

static void SDLCALL rdp_file_cb(void* userdata, const char* const* filelist,
                                WINPR_ATTR_UNUSED int filter)
{
	auto rdp = static_cast<std::string*>(userdata);

	if (!filelist)
	{
		SDL_Log("An error occurred: %s", SDL_GetError());
		sdl_quit();
		return;
	}
	else if (!*filelist)
	{
		SDL_Log("The user did not select any file.");
		SDL_Log("Most likely, the dialog was canceled.");
		sdl_quit();
		return;
	}

	while (*filelist)
	{
		SDL_Log("Full path to selected file: '%s'", *filelist);
		*rdp = *filelist;
		filelist++;
	}

	sdl_quit();
}

static std::string getRdpFile()
{
	const auto flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
	SDL_DialogFileFilter filters[] = { { "RDP files", "rdp;rdpw" } };
	std::string rdp;

	bool running = true;
	if (!SDL_Init(flags))
		return {};

	auto props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING,
	                      "SDL Freerdp - Open a RDP file");
	SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
	SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, filters);
	SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, ARRAYSIZE(filters));
	SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFILE, rdp_file_cb, &rdp, props);
	SDL_DestroyProperties(props);

	do
	{
		SDL_Event event = {};
		(void)SDL_PollEvent(&event);

		switch (event.type)
		{
			case SDL_EVENT_QUIT:
				running = false;
				break;
			default:
				break;
		}
	} while (running);
	SDL_Quit();
	return rdp;
}

int main(int argc, char* argv[])
{
	int rc = -1;
	int status = 0;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = {};

	RdpClientEntry(&clientEntryPoints);
	std::unique_ptr<sdl_rdp_context, void (*)(sdl_rdp_context*)> sdl_rdp(
	    reinterpret_cast<sdl_rdp_context*>(freerdp_client_context_new(&clientEntryPoints)),
	    context_free);

	if (!sdl_rdp)
		return -1;
	auto sdl = sdl_rdp->sdl;

	auto settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	std::string rdp_file;
	std::vector<char*> args;
	args.reserve(WINPR_ASSERTING_INT_CAST(size_t, argc));
	for (auto x = 0; x < argc; x++)
		args.push_back(argv[x]);

	if (argc == 1)
	{
		rdp_file = getRdpFile();
		if (!rdp_file.empty())
		{
			args.push_back(rdp_file.data());
		}
	}

	status = freerdp_client_settings_parse_command_line(
	    settings, WINPR_ASSERTING_INT_CAST(int, args.size()), args.data(), FALSE);
	sdl_rdp->sdl->setMetadata();
	if (status)
	{
		rc = freerdp_client_settings_command_line_status_print(settings, status, argc, argv);
		if (freerdp_settings_get_bool(settings, FreeRDP_ListMonitors))
			sdl_list_monitors(sdl);
		else
		{
			switch (status)
			{
				case COMMAND_LINE_STATUS_PRINT:
				case COMMAND_LINE_STATUS_PRINT_VERSION:
				case COMMAND_LINE_STATUS_PRINT_BUILDCONFIG:
					break;
				case COMMAND_LINE_STATUS_PRINT_HELP:
				default:
					SdlPref::print_config_file_help(3);
					break;
			}
		}
		return rc;
	}

	SDL_SetLogOutputFunction(winpr_LogOutputFunction, sdl);
	auto level = WLog_GetLogLevel(sdl->log);
	SDL_SetLogPriorities(wloglevel2dl(level));

	auto context = sdl->context();
	WINPR_ASSERT(context);

	if (!stream_dump_register_handlers(context, CONNECTION_STATE_MCS_CREATE_REQUEST, FALSE))
		return -1;

	if (freerdp_client_start(context) != 0)
		return -1;

	rc = sdl_run(sdl);

	if (freerdp_client_stop(context) != 0)
		return -1;

	if (sdl->exit_code != 0)
		rc = sdl->exit_code;

	return rc;
}

bool SdlContext::update_fullscreen(bool enter)
{
	for (const auto& window : windows)
	{
		if (!sdl_push_user_event(SDL_EVENT_USER_WINDOW_FULLSCREEN, &window.second, enter))
			return false;
	}
	fullscreen = enter;
	return true;
}

bool SdlContext::update_minimize()
{
	return sdl_push_user_event(SDL_EVENT_USER_WINDOW_MINIMIZE);
}

bool SdlContext::update_resizeable(bool enable)
{
	const auto settings = context()->settings;
	const bool dyn = freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate);
	const bool smart = freerdp_settings_get_bool(settings, FreeRDP_SmartSizing);
	bool use = (dyn && enable) || smart;

	for (const auto& window : windows)
	{
		if (!sdl_push_user_event(SDL_EVENT_USER_WINDOW_RESIZEABLE, &window.second, use))
			return false;
	}
	resizeable = use;

	return true;
}

SdlContext::SdlContext(rdpContext* context)
    : _context(context), log(WLog_Get(SDL_TAG)), disp(this), input(this), clip(this),
      primary(nullptr, SDL_DestroySurface), rdp_thread_running(false), dialog(log)
{
	WINPR_ASSERT(context);
	setMetadata();
}

void SdlContext::setHasCursor(bool val)
{
	this->_cursor_visible = val;
}

bool SdlContext::hasCursor() const
{
	return _cursor_visible;
}

void SdlContext::setMetadata()
{
	auto wmclass = freerdp_settings_get_string(_context->settings, FreeRDP_WmClass);
	if (!wmclass || (strlen(wmclass) == 0))
		wmclass = SDL_CLIENT_UUID;

	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, wmclass);
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, SDL_CLIENT_NAME);
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, SDL_CLIENT_VERSION);
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, SDL_CLIENT_VENDOR);
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, SDL_CLIENT_COPYRIGHT);
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, SDL_CLIENT_URL);
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, SDL_CLIENT_TYPE);
}

bool SdlContext::redraw(bool suppress) const
{
	if (!_connected)
		return true;

	auto gdi = context()->gdi;
	WINPR_ASSERT(gdi);
	return gdi_send_suppress_output(gdi, suppress);
}

void SdlContext::setConnected(bool val)
{
	_connected = val;
}

bool SdlContext::isConnected() const
{
	return _connected;
}

rdpContext* SdlContext::context() const
{
	WINPR_ASSERT(_context);
	return _context;
}

rdpClientContext* SdlContext::common() const
{
	return reinterpret_cast<rdpClientContext*>(context());
}

void SdlContext::setCursor(rdpPointer* cursor)
{
	_cursor = cursor;
}

rdpPointer* SdlContext::cursor() const
{
	return _cursor;
}

void SdlContext::setMonitorIds(const std::vector<SDL_DisplayID>& ids)
{
	_monitorIds.clear();
	for (auto id : ids)
	{
		_monitorIds.push_back(id);
	}
}

const std::vector<SDL_DisplayID>& SdlContext::monitorIds() const
{
	return _monitorIds;
}

int64_t SdlContext::monitorId(uint32_t index) const
{
	if (index >= _monitorIds.size())
	{
		return -1;
	}
	return _monitorIds[index];
}

void SdlContext::push(std::vector<SDL_Rect>&& rects)
{
	std::unique_lock lock(_queue_mux);
	_queue.emplace(std::move(rects));
}

std::vector<SDL_Rect> SdlContext::pop()
{
	std::unique_lock lock(_queue_mux);
	if (_queue.empty())
	{
		return {};
	}
	auto val = std::move(_queue.front());
	_queue.pop();
	return val;
}
