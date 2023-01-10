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
#include <SDL_video.h>

#include "sdl_channels.h"
#include "sdl_freerdp.h"
#include "sdl_utils.h"
#include "sdl_disp.h"
#include "sdl_monitor.h"
#include "sdl_kbd.h"
#include "sdl_touch.h"
#include "sdl_pointer.h"

#define TAG CLIENT_TAG("SDL")

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
	ENTRY(FREERDP_ERROR_NONE, SDL_EXIT_DISCONNECT_BY_USER),
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
	return NULL;
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
	return NULL;
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
	return NULL;
}

static const char* sdl_map_to_code_tag(int code)
{
	const struct sdl_exit_code_map_t* entry = sdl_map_entry_by_code(code);
	if (entry)
		return entry->code_tag;
	return NULL;
}

static BOOL sdl_init_sdl(sdlContext* sdl);
static DWORD WINAPI sdl_run(void* arg);
static BOOL sdl_create_windows(sdlContext* sdl);

/* This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas. */
static BOOL sdl_begin_paint(rdpContext* context)
{
	rdpGdi* gdi;
	sdlContext* sdl = (sdlContext*)context;

	WINPR_ASSERT(context);

	gdi = context->gdi;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(gdi->primary);
	WINPR_ASSERT(gdi->primary->hdc);
	WINPR_ASSERT(gdi->primary->hdc->hwnd);
	WINPR_ASSERT(gdi->primary->hdc->hwnd->invalid);
	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	gdi->primary->hdc->hwnd->ninvalid = 0;
	SDL_LockSurface(sdl->primary);
	return TRUE;
}

static BOOL sdl_redraw(sdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	rdpGdi* gdi = sdl->common.context.gdi;
	return gdi_send_suppress_output(gdi, FALSE);
}

/* This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
static BOOL sdl_end_paint(rdpContext* context)
{
	rdpGdi* gdi;
	sdlContext* sdl = (sdlContext*)context;

	WINPR_ASSERT(context);

	gdi = context->gdi;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(gdi->primary);
	WINPR_ASSERT(gdi->primary->hdc);
	WINPR_ASSERT(gdi->primary->hdc->hwnd);
	WINPR_ASSERT(gdi->primary->hdc->hwnd->invalid);

	if (gdi->suppressOutput || gdi->primary->hdc->hwnd->invalid->null)
	{
		SDL_UnlockSurface(sdl->primary);
		return TRUE;
	}

	SDL_UnlockSurface(sdl->primary);

	const INT32 ninvalid = gdi->primary->hdc->hwnd->ninvalid;
	const GDI_RGN* cinvalid = gdi->primary->hdc->hwnd->cinvalid;

	if (ninvalid < 1)
		return TRUE;

	// TODO: Support multiple windows
	for (size_t x = 0; x < sdl->windowCount; x++)
	{
		sdl_window_t* window = &sdl->windows[x];
		SDL_Surface* screen = SDL_GetWindowSurface(window->window);

		int w, h;
		SDL_GetWindowSize(window->window, &w, &h);

		window->offset_x = 0;
		window->offset_y = 0;
		if (!freerdp_settings_get_bool(context->settings, FreeRDP_SmartSizing))
		{
			if (gdi->width < w)
			{
				window->offset_x = (w - gdi->width) / 2;
			}
			if (gdi->height < h)
			{
				window->offset_y = (h - gdi->height) / 2;
			}

			for (INT32 i = 0; i < ninvalid; i++)
			{
				const GDI_RGN* rgn = &cinvalid[i];
				const SDL_Rect srcRect = { rgn->x, rgn->y, rgn->w, rgn->h };
				SDL_Rect dstRect = { window->offset_x + rgn->x, window->offset_y + rgn->y, rgn->w,
					                 rgn->h };
				SDL_SetClipRect(sdl->primary, &srcRect);
				SDL_BlitSurface(sdl->primary, &srcRect, screen, &dstRect);
			}
		}
		else
		{
			const Uint32 id = SDL_GetWindowID(window->window);
			for (INT32 i = 0; i < ninvalid; i++)
			{
				const GDI_RGN* rgn = &cinvalid[i];
				const SDL_Rect srcRect = { rgn->x, rgn->y, rgn->w, rgn->h };
				SDL_Rect dstRect = srcRect;
				sdl_scale_coordinates(sdl, id, &dstRect.x, &dstRect.y, FALSE, TRUE);
				sdl_scale_coordinates(sdl, id, &dstRect.w, &dstRect.h, FALSE, TRUE);
				SDL_SetClipRect(sdl->primary, &srcRect);
				SDL_SetClipRect(screen, &dstRect);
				SDL_BlitScaled(sdl->primary, &srcRect, screen, &dstRect);
			}
		}
		SDL_UpdateWindowSurface(window->window);
	}

	return TRUE;
}

/* Create a SDL surface from the GDI buffer */
static BOOL sdl_create_primary(sdlContext* sdl)
{
	rdpGdi* gdi;

	WINPR_ASSERT(sdl);

	gdi = sdl->common.context.gdi;
	WINPR_ASSERT(gdi);

	SDL_FreeSurface(sdl->primary);
	sdl->primary = SDL_CreateRGBSurfaceWithFormatFrom(
	    gdi->primary_buffer, (int)gdi->width, (int)gdi->height,
	    (int)FreeRDPGetBitsPerPixel(gdi->dstFormat), (int)gdi->stride, sdl->sdl_pixel_format);
	return sdl->primary != NULL;
}

static BOOL sdl_desktop_resize(rdpContext* context)
{
	rdpGdi* gdi;
	rdpSettings* settings;
	sdlContext* sdl = (sdlContext*)context;

	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	gdi = context->gdi;
	if (!gdi_resize(gdi, settings->DesktopWidth, settings->DesktopHeight))
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

/* Called before a connection is established.
 * Set all configuration options to support and load channels here. */
static BOOL sdl_pre_connect(freerdp* instance)
{
	rdpSettings* settings;
	sdlContext* sdl;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	sdl = (sdlContext*)instance->context;
	sdl->highDpi = TRUE; // If High DPI is available, we want unscaled data, RDP can scale itself.

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	/* Optional OS identifier sent to server */
	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_SDL;
	/* settings->OrderSupport is initialized at this point.
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

		if (!sdl_init_sdl(sdl))
			return FALSE;

		if (!sdl_detect_monitors(sdl, &maxWidth, &maxHeight))
			return FALSE;

		if ((maxWidth != 0) && (maxHeight != 0) &&
		    !freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
		{
			WLog_INFO(TAG, "Update size to %ux%u", maxWidth, maxHeight);
			settings->DesktopWidth = maxWidth;
			settings->DesktopHeight = maxHeight;
		}
	}
	else
	{
		/* Check +auth-only has a username and password. */
		if (!freerdp_settings_get_string(settings, FreeRDP_Password))
		{
			WLog_INFO(TAG, "auth-only, but no password set. Please provide one.");
			return FALSE;
		}

		if (!freerdp_settings_set_bool(settings, FreeRDP_DeactivateClientDecoding, TRUE))
			return FALSE;

		WLog_INFO(TAG, "Authentication only. Don't connect SDL.");
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
		return NULL;

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

static void sdl_cleanup_sdl(sdlContext* sdl)
{
	const sdl_window_t empty = { 0 };
	if (!sdl)
		return;

	if (sdl->thread)
	{
		int res = 0;
		SDL_Event q = { 0 };
		q.type = SDL_QUIT;
		res = SDL_PushEvent(&q);

		WaitForSingleObject(sdl->thread, INFINITE);
		CloseHandle(sdl->thread);
	}
	for (size_t x = 0; x < sdl->windowCount; x++)
	{
		sdl_window_t* window = &sdl->windows[x];
		SDL_DestroyWindow(window->window);

		*window = empty;
	}
	SDL_FreeSurface(sdl->primary);
	sdl->primary = NULL;

	sdl->windowCount = 0;
	SDL_Quit();
}

BOOL sdl_init_sdl(sdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	SDL_Init(SDL_INIT_VIDEO);

	sdl->thread = CreateThread(NULL, 0, sdl_run, sdl, 0, NULL);
	if (!sdl->thread)
		goto fail;
	return TRUE;
fail:
	sdl_cleanup_sdl(sdl);
	return FALSE;
}

BOOL sdl_create_windows(sdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	const char* title = sdl_window_get_title(sdl->common.context.settings);
	BOOL rc = FALSE;

	// TODO: Multimonitor setup
	sdl->windowCount = 1;

	const UINT32 w =
	    freerdp_settings_get_uint32(sdl->common.context.settings, FreeRDP_DesktopWidth);
	const UINT32 h =
	    freerdp_settings_get_uint32(sdl->common.context.settings, FreeRDP_DesktopHeight);

	sdl_window_t* window = NULL;
	for (size_t x = 0; x < sdl->windowCount; x++)
	{
		Uint32 flags = SDL_WINDOW_SHOWN;

		if (sdl->highDpi)
		{
#if SDL_VERSION_ATLEAST(2, 0, 1)
			flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
		}

		if (sdl->common.context.settings->Fullscreen || sdl->common.context.settings->UseMultimon)
			flags |= SDL_WINDOW_FULLSCREEN;

		window = &sdl->windows[x];

		window->window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		                                  (int)w, (int)h, flags);
		if (!window->window)
			goto fail;
	}

	rc = TRUE;
fail:
	return rc;
}

void update_resizeable(sdlContext* sdl, BOOL enable)
{
	WINPR_ASSERT(sdl);

	const rdpSettings* settings = sdl->common.context.settings;
	const BOOL dyn = freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate);
	const BOOL smart = freerdp_settings_get_bool(settings, FreeRDP_SmartSizing);
	BOOL use = (dyn && enable) || smart;

	for (uint32_t x = 0; x < sdl->windowCount; x++)
	{
		sdl_window_t* window = &sdl->windows[x];
		SDL_SetWindowResizable(window->window, use ? SDL_TRUE : SDL_FALSE);
	}
	sdl->resizeable = use;
}

void update_fullscreen(sdlContext* sdl, BOOL enter)
{
	WINPR_ASSERT(sdl);

	for (uint32_t x = 0; x < sdl->windowCount; x++)
	{
		sdl_window_t* window = &sdl->windows[x];
		Uint32 curFlags = SDL_GetWindowFlags(window->window);
		const BOOL isSet = (curFlags & SDL_WINDOW_FULLSCREEN);
		if (enter)
			curFlags |= SDL_WINDOW_FULLSCREEN;
		else
			curFlags &= ~SDL_WINDOW_FULLSCREEN;

		if ((enter && !isSet) || (!enter && isSet))
			SDL_SetWindowFullscreen(window->window, curFlags);
	}
	sdl->fullscreen = enter;
}

static DWORD WINAPI sdl_run(void* arg)
{
	sdlContext* sdl = arg;
	WINPR_ASSERT(sdl);
	while (!freerdp_shall_disconnect_context(&sdl->common.context))
	{
		SDL_Event windowEvent = { 0 };
		while (!freerdp_shall_disconnect_context(&sdl->common.context) &&
		       SDL_PollEvent(&windowEvent))
		{
			// SDL_Log("got event %s", sdl_event_type_str(windowEvent.type));
			switch (windowEvent.type)
			{
				case SDL_QUIT:
					freerdp_abort_connect_context(&sdl->common.context);
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					const SDL_KeyboardEvent* ev = &windowEvent.key;
					sdl_handle_keyboard_event(sdl, ev);
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
					sdl_disp_handle_display_event(sdl->disp, ev);
				}
				break;
#endif
				case SDL_WINDOWEVENT:
				{
					const SDL_WindowEvent* ev = &windowEvent.window;
					sdl_disp_handle_window_event(sdl->disp, ev);
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
				default:
					break;
			}
		}
	}
	return TRUE;
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
	sdlContext* sdl;
	rdpContext* context;

	WINPR_ASSERT(instance);

	context = instance->context;
	WINPR_ASSERT(context);

	sdl = (sdlContext*)context;

	if (freerdp_settings_get_bool(context->settings, FreeRDP_AuthenticationOnly))
	{
		/* Check +auth-only has a username and password. */
		if (!freerdp_settings_get_string(context->settings, FreeRDP_Password))
		{
			WLog_INFO(TAG, "auth-only, but no password set. Please provide one.");
			return FALSE;
		}

		WLog_INFO(TAG, "Authentication only. Don't connect to X.");
		return TRUE;
	}

	if (!sdl_create_windows(sdl))
		return FALSE;

	update_resizeable(sdl, FALSE);
	update_fullscreen(sdl, context->settings->Fullscreen || context->settings->UseMultimon);

	sdl->sdl_pixel_format = SDL_PIXELFORMAT_BGRA32;
	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
		return FALSE;

	if (!sdl_create_primary(sdl))
		return FALSE;

	sdl->disp = sdl_disp_new(sdl);
	if (!sdl->disp)
		return FALSE;

	if (!sdl_register_pointer(instance->context->graphics))
		return FALSE;

	WINPR_ASSERT(context->update);

	context->update->BeginPaint = sdl_begin_paint;
	context->update->EndPaint = sdl_end_paint;
	context->update->PlaySound = sdl_play_sound;
	context->update->DesktopResize = sdl_desktop_resize;
	context->update->SetKeyboardIndicators = sdl_keyboard_set_indicators;
	context->update->SetKeyboardImeStatus = sdl_keyboard_set_ime_status;
	return TRUE;
}

/* This function is called whether a session ends by failure or success.
 * Clean up everything allocated by pre_connect and post_connect.
 */
static void sdl_post_disconnect(freerdp* instance)
{
	sdlContext* context;

	if (!instance)
		return;

	if (!instance->context)
		return;

	context = (sdlContext*)instance->context;
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
	sdlContext* context;

	if (!instance)
		return;

	if (!instance->context)
		return;

	context = (sdlContext*)instance->context;

	sdl_disp_free(context->disp);
	context->disp = NULL;
	sdl_cleanup_sdl(context);
}

/* RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends. */
static int WINAPI sdl_client_thread_proc(LPVOID arg)
{
	freerdp* instance = (freerdp*)arg;
	DWORD nCount;
	DWORD status;
	int exit_code = SDL_EXIT_SUCCESS;
	HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
	BOOL rc = freerdp_connect(instance);

	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	if (!rc)
	{
		UINT32 error = freerdp_get_last_error(instance->context);
		exit_code = sdl_map_error_to_exit_code(error);
	}

	if (freerdp_settings_get_bool(instance->context->settings, FreeRDP_AuthenticationOnly))
	{
		DWORD code = freerdp_get_last_error(instance->context);
		freerdp_abort_connect_context(instance->context);
		WLog_ERR(TAG, "Authentication only, freerdp_get_last_error() %s [0x%08" PRIx32 "] %s",
		         freerdp_get_last_error_name(code), code, freerdp_get_last_error_string(code));
		goto disconnect;
	}

	if (!rc)
	{
		DWORD code = freerdp_error_info(instance);
		if (exit_code == SDL_EXIT_SUCCESS)
			exit_code = sdl_map_error_to_exit_code(code);

		if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_AUTHENTICATION_FAILED)
			exit_code = SDL_EXIT_AUTH_FAILURE;
		else if (code == ERRINFO_SUCCESS)
			exit_code = SDL_EXIT_CONN_FAILED;

		goto disconnect;
	}

	while (!freerdp_shall_disconnect_context(instance->context))
	{
		/*
		 * win8 and server 2k12 seem to have some timing issue/race condition
		 * when a initial sync request is send to sync the keyboard indicators
		 * sending the sync event twice fixed this problem
		 */
		if (freerdp_focus_required(instance))
		{
			if (!sdl_keyboard_focus_in(instance->context))
				goto disconnect;
			if (!sdl_keyboard_focus_in(instance->context))
				goto disconnect;
		}

		nCount = freerdp_get_event_handles(instance->context, handles, ARRAYSIZE(handles));

		if (nCount == 0)
		{
			WLog_ERR(TAG, "%s: freerdp_get_event_handles failed", __FUNCTION__);
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

			if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_SUCCESS)
				WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %" PRIu32 "", __FUNCTION__,
				         status);
			break;
		}

		if (!freerdp_check_event_handles(instance->context))
		{
			if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_SUCCESS)
				WLog_ERR(TAG, "Failed to check FreeRDP event handles");

			break;
		}
	}

	if (exit_code == SDL_EXIT_SUCCESS)
	{
		DWORD code = freerdp_error_info(instance);
		exit_code = sdl_map_error_to_exit_code(code);

		if ((code == SDL_EXIT_DISCONNECT) && (freerdp_get_disconnect_ultimatum(instance->context) ==
		                                      Disconnect_Ultimatum_user_requested))
		{
			/* This situation might be limited to Windows XP. */
			WLog_INFO(TAG, "Error info says user did not initiate but disconnect ultimatum says "
			               "they did; treat this as a user logoff");
			exit_code = SDL_EXIT_LOGOFF;
		}
	}

disconnect:
	if (freerdp_settings_get_bool(instance->context->settings, FreeRDP_AuthenticationOnly))
		WLog_INFO(TAG, "Authentication only, exit status %s [%" PRId32 "]",
		          sdl_map_to_code_tag(exit_code), exit_code);
	freerdp_disconnect(instance);
	return exit_code;
}

/* Optional global initializer.
 * Here we just register a signal handler to print out stack traces
 * if available. */
static BOOL sdl_client_global_init(void)
{
	if (freerdp_handle_signals() != 0)
		return FALSE;

	return TRUE;
}

/* Optional global tear down */
static void sdl_client_global_uninit(void)
{
}

static int sdl_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	sdlContext* tf;
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	tf = (sdlContext*)instance->context;
	WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);
	WINPR_UNUSED(tf);

	return 1;
}

static BOOL sdl_client_new(freerdp* instance, rdpContext* context)
{
	sdlContext* tf = (sdlContext*)context;

	if (!instance || !context)
		return FALSE;

	instance->PreConnect = sdl_pre_connect;
	instance->PostConnect = sdl_post_connect;
	instance->PostDisconnect = sdl_post_disconnect;
	instance->PostFinalDisconnect = sdl_post_final_disconnect;
	instance->AuthenticateEx = client_cli_authenticate_ex;
	instance->VerifyCertificateEx = client_cli_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
	instance->LogonErrorInfo = sdl_logon_error_info;
	/* TODO: Client display set up */
	WINPR_UNUSED(tf);
	return TRUE;
}

static void sdl_client_free(freerdp* instance, rdpContext* context)
{
	sdlContext* tf = (sdlContext*)instance->context;

	if (!context)
		return;

	/* TODO: Client display tear down */
	WINPR_UNUSED(tf);
}

static int sdl_client_start(rdpContext* context)
{
	/* TODO: Start client related stuff */
	WINPR_UNUSED(context);
	return 0;
}

static int sdl_client_stop(rdpContext* context)
{
	/* TODO: Stop client related stuff */
	WINPR_UNUSED(context);
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
	pEntryPoints->ContextSize = sizeof(sdlContext);
	pEntryPoints->ClientNew = sdl_client_new;
	pEntryPoints->ClientFree = sdl_client_free;
	pEntryPoints->ClientStart = sdl_client_start;
	pEntryPoints->ClientStop = sdl_client_stop;
	return 0;
}

int main(int argc, char* argv[])
{
	int rc = -1;
	int status;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* context;

	WLog_WARN(TAG, "[experimental] The SDL client is currently experimental!");
	WLog_WARN(TAG, " If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for "
	               "know issues or create a new one!");
	WLog_WARN(TAG, " Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org "
	               "- dont hesitate to ask some questions. (replies might take some time depending "
	               "on your timezone)");

	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		goto fail;

	status = freerdp_client_settings_parse_command_line(context->settings, argc, argv, FALSE);
	if (status)
	{
		rc = freerdp_client_settings_command_line_status_print(context->settings, status, argc,
		                                                       argv);
		if (context->settings->ListMonitors)
			sdl_list_monitors((sdlContext*)context);
		goto fail;
	}

	if (!stream_dump_register_handlers(context, CONNECTION_STATE_MCS_CREATE_REQUEST, FALSE))
		goto fail;

	if (freerdp_client_start(context) != 0)
		goto fail;

	rc = sdl_client_thread_proc(context->instance);

	if (freerdp_client_stop(context) != 0)
		rc = -1;

fail:
	freerdp_client_context_free(context);
	return rc;
}
