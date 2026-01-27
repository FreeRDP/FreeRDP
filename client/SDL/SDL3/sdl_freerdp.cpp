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
#include "sdl_freerdp.hpp"
#include "sdl_context.hpp"
#include "sdl_monitor.hpp"
#include "sdl_pointer.hpp"
#include "sdl_prefs.hpp"
#include "sdl_utils.hpp"

#if defined(_WIN32)
#define SDL_TAG CLIENT_TAG("SDL")
#endif

static void sdl_term_handler([[maybe_unused]] int signum, [[maybe_unused]] const char* signame,
                             [[maybe_unused]] void* context)
{
	std::ignore = sdl_push_quit();
}

[[nodiscard]] static int sdl_run(SdlContext* sdl)
{
	int rc = -1;
	WINPR_ASSERT(sdl);

	while (!sdl->shallAbort())
	{
		SDL_Event windowEvent = {};
		while (!sdl->shallAbort() && SDL_WaitEventTimeout(nullptr, 1000))
		{
			/* Only poll standard SDL events and SDL_EVENT_USERS meant to create
			 * dialogs. do not process the dialog return value events here.
			 */
			const int prc = SDL_PeepEvents(&windowEvent, 1, SDL_GETEVENT, SDL_EVENT_FIRST,
			                               SDL_EVENT_USER_RETRY_DIALOG);
			if (prc < 0)
			{
				if (sdl_log_error(prc, sdl->getWLog(), "SDL_PeepEvents"))
					continue;
			}

#if defined(WITH_DEBUG_SDL_EVENTS)
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "got event %s [0x%08" PRIx32 "]",
			             sdl::utils::toString(windowEvent.type).c_str(), windowEvent.type);
#endif
			if (sdl->shallAbort(true))
				continue;

			if (sdl->getDialog().handleEvent(windowEvent))
				continue;

			if (!sdl->handleEvent(windowEvent))
				return -1;

			switch (windowEvent.type)
			{
				case SDL_EVENT_QUIT:
					freerdp_abort_connect_context(sdl->context());
					break;
				case SDL_EVENT_USER_CERT_DIALOG:
				{
					SDLConnectionDialogHider hider(sdl);
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char*>(windowEvent.user.data2);
					if (!sdl_cert_dialog_show(title, msg))
						return -1;
				}
				break;
				case SDL_EVENT_USER_SHOW_DIALOG:
				{
					SDLConnectionDialogHider hider(sdl);
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char*>(windowEvent.user.data2);
					if (!sdl_message_dialog_show(title, msg, windowEvent.user.code))
						return -1;
				}
				break;
				case SDL_EVENT_USER_SCARD_DIALOG:
				{
					SDLConnectionDialogHider hider(sdl);
					auto title = static_cast<const char*>(windowEvent.user.data1);
					auto msg = static_cast<const char**>(windowEvent.user.data2);
					if (!sdl_scard_dialog_show(title, windowEvent.user.code, msg))
						return -1;
				}
				break;
				case SDL_EVENT_USER_AUTH_DIALOG:
				{
					SDLConnectionDialogHider hider(sdl);
					if (!sdl_auth_dialog_show(
					        reinterpret_cast<const SDL_UserAuthArg*>(windowEvent.padding)))
						return -1;
				}
				break;
				case SDL_EVENT_USER_UPDATE:
				{
					std::vector<SDL_Rect> rectangles;
					do
					{
						rectangles = sdl->pop();
						if (!sdl->drawToWindows(rectangles))
							return -1;
					} while (!rectangles.empty());
				}
				break;
				case SDL_EVENT_USER_CREATE_WINDOWS:
				{
					auto ctx = static_cast<SdlContext*>(windowEvent.user.data1);
					if (!ctx->createWindows())
						return -1;
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
					if (!sdl->minimizeAllWindows())
						return -1;
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
					if (!sdl->moveMouseTo(
					        { static_cast<float>(x) * 1.0f, static_cast<float>(y) * 1.0f }))
						return -1;
				}
				break;
				case SDL_EVENT_USER_POINTER_SET:
					sdl->setCursor(static_cast<rdpPointer*>(windowEvent.user.data1));
					if (!sdl_Pointer_Set_Process(sdl))
						return -1;
					break;
				case SDL_EVENT_USER_QUIT:
				default:
					break;
			}
		}
	}

	rc = 1;

	return rc;
}

/* Optional global initializer.
 * Here we just register a signal handler to print out stack traces
 * if available. */
[[nodiscard]] static BOOL sdl_client_global_init()
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

[[nodiscard]] static BOOL sdl_client_new(freerdp* instance, rdpContext* context)
{
	auto sdl = reinterpret_cast<sdl_rdp_context*>(context);

	if (!instance || !context)
		return FALSE;

	sdl->sdl = new SdlContext(context);
	return sdl->sdl != nullptr;
}

static void sdl_client_free([[maybe_unused]] freerdp* instance, rdpContext* context)
{
	auto sdl = reinterpret_cast<sdl_rdp_context*>(context);

	if (!context)
		return;

	delete sdl->sdl;
}

[[nodiscard]] static int sdl_client_start(rdpContext* context)
{
	auto sdl = get_context(context);
	WINPR_ASSERT(sdl);
	return sdl->start();
}

[[nodiscard]] static int sdl_client_stop(rdpContext* context)
{
	auto sdl = get_context(context);
	WINPR_ASSERT(sdl);
	return sdl->join();
}

static void RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
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
}

static void context_free(sdl_rdp_context* sdl)
{
	if (sdl)
		freerdp_client_context_free(&sdl->common.context);
}

[[nodiscard]] static const char* category2str(int category)
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

[[nodiscard]] static SDL_LogPriority wloglevel2dl(DWORD level)
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

[[nodiscard]] static DWORD sdlpriority2wlog(SDL_LogPriority priority)
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
	auto log = sdl->getWLog();
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

[[nodiscard]] static std::string getRdpFile()
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
		std::ignore = SDL_PollEvent(&event);

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
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = {};

	/* Allocate the RDP context first, we need to pass it to SDL */
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

	auto status = freerdp_client_settings_parse_command_line(
	    settings, WINPR_ASSERTING_INT_CAST(int, args.size()), args.data(), FALSE);
	sdl_rdp->sdl->setMetadata();
	if (status)
	{
		rc = freerdp_client_settings_command_line_status_print(settings, status, argc, argv);
		if (freerdp_settings_get_bool(settings, FreeRDP_ListMonitors))
		{
			if (!sdl_list_monitors(sdl))
				return -1;
		}
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

	/* Basic SDL initialization */
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		return -1;

	/* Redirect SDL log messages to wLog */
	SDL_SetLogOutputFunction(winpr_LogOutputFunction, sdl);
	auto level = WLog_GetLogLevel(sdl->getWLog());
	SDL_SetLogPriorities(wloglevel2dl(level));

	auto backend = SDL_GetCurrentVideoDriver();
	WLog_Print(sdl->getWLog(), WLOG_DEBUG, "client is using backend '%s'", backend);
	sdl_dialogs_init();

	SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
	SDL_SetHint(SDL_HINT_PEN_MOUSE_EVENTS, "0");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	SDL_SetHint(SDL_HINT_PEN_TOUCH_EVENTS, "1");
	SDL_SetHint(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, "1");

	/* SDL cleanup code if the client exits */
	ScopeGuard guard(
	    [&]()
	    {
		    sdl->cleanup();
		    freerdp_del_signal_cleanup_handler(sdl->context(), sdl_term_handler);
		    sdl_dialogs_uninit();
		    SDL_Quit();
	    });

	/* Initialize RDP */
	auto context = sdl->context();
	WINPR_ASSERT(context);

	if (!stream_dump_register_handlers(context, CONNECTION_STATE_MCS_CREATE_REQUEST, FALSE))
		return -1;

	if (freerdp_client_start(context) != 0)
		return -1;

	rc = sdl_run(sdl);

	if (freerdp_client_stop(context) != 0)
		return -1;

	if (sdl->exitCode() != 0)
		rc = sdl->exitCode();

	return rc;
}
