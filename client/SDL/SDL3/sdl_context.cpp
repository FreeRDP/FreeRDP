/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
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

#include <algorithm>

#include "sdl_context.hpp"
#include "sdl_config.hpp"
#include "sdl_channels.hpp"
#include "sdl_monitor.hpp"
#include "sdl_pointer.hpp"
#include "sdl_touch.hpp"

#include <sdl_common_utils.hpp>
#include <scoped_guard.hpp>

#include "dialogs/sdl_dialogs.hpp"

#if defined(WITH_WEBVIEW)
#include <aad/sdl_webview.hpp>
#endif

SdlContext::SdlContext(rdpContext* context)
    : _context(context), _log(WLog_Get(CLIENT_TAG("SDL"))), _rdpThreadRunning(false),
      _primary(nullptr, SDL_DestroySurface), _disp(this), _input(this), _clip(this), _dialog(_log)
{
	WINPR_ASSERT(context);
	setMetadata();

	auto instance = _context->instance;
	WINPR_ASSERT(instance);

	instance->PreConnect = preConnect;
	instance->PostConnect = postConnect;
	instance->PostDisconnect = postDisconnect;
	instance->PostFinalDisconnect = postFinalDisconnect;
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

int SdlContext::start()
{
	_thread = std::thread(rdpThreadRun, this);
	return 0;
}

int SdlContext::join()
{
	/* We do not want to use freerdp_abort_connect_context here.
	 * It would change the exit code and we do not want that. */
	HANDLE event = freerdp_abort_event(context());
	if (!SetEvent(event))
		return -1;

	_thread.join();
	return 0;
}

void SdlContext::cleanup()
{
	std::unique_lock lock(_critical);
	_windows.clear();
	_dialog.destroy();
	_primary.reset();
}

bool SdlContext::shallAbort(bool ignoreDialogs)
{
	std::unique_lock lock(_critical);
	if (freerdp_shall_disconnect_context(context()))
	{
		if (ignoreDialogs)
			return true;
		if (_rdpThreadRunning)
			return false;
		return !getDialog().isRunning();
	}
	return false;
}

/* Called before a connection is established.
 * Set all configuration options to support and load channels here. */
BOOL SdlContext::preConnect(freerdp* instance)
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

		if (!sdl_detect_monitors(sdl, &maxWidth, &maxHeight))
			return FALSE;

		if ((maxWidth != 0) && (maxHeight != 0) &&
		    !freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
		{
			WLog_Print(sdl->getWLog(), WLOG_INFO, "Update size to %ux%u", maxWidth, maxHeight);
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, maxWidth))
				return FALSE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, maxHeight))
				return FALSE;
		}

		/**
		 * If /f is specified in combination with /smart-sizing:widthxheight then
		 * we run the session in the /smart-sizing dimensions scaled to full screen
		 */

		const uint32_t sw = freerdp_settings_get_uint32(settings, FreeRDP_SmartSizingWidth);
		const uint32_t sh = freerdp_settings_get_uint32(settings, FreeRDP_SmartSizingHeight);
		const BOOL sm = freerdp_settings_get_bool(settings, FreeRDP_SmartSizing);
		if (sm && (sw > 0) && (sh > 0))
		{
			const BOOL mm = freerdp_settings_get_bool(settings, FreeRDP_UseMultimon);
			if (mm)
				WLog_Print(sdl->getWLog(), WLOG_WARN,
				           "/smart-sizing and /multimon are currently not supported, ignoring "
				           "/smart-sizing!");
			else
			{
				sdl->_windowWidth = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
				sdl->_windowHeigth = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);

				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, sw))
					return FALSE;
				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, sh))
					return FALSE;
			}
		}
	}
	else
	{
		/* Check +auth-only has a username and password. */
		if (!freerdp_settings_get_string(settings, FreeRDP_Password))
		{
			WLog_Print(sdl->getWLog(), WLOG_INFO,
			           "auth-only, but no password set. Please provide one.");
			return FALSE;
		}

		if (!freerdp_settings_set_bool(settings, FreeRDP_DeactivateClientDecoding, TRUE))
			return FALSE;

		WLog_Print(sdl->getWLog(), WLOG_INFO, "Authentication only. Don't connect SDL.");
	}

	if (!sdl->getInputChannelContext().initialize())
		return FALSE;

	/* TODO: Any code your client requires */
	return TRUE;
}

/* Called after a RDP connection was successfully established.
 * Settings might have changed during negotiation of client / server feature
 * support.
 *
 * Set up local framebuffers and paing callbacks.
 * If required, register pointer callbacks to change the local mouse cursor
 * when hovering over the RDP window
 */
BOOL SdlContext::postConnect(freerdp* instance)
{
	WINPR_ASSERT(instance);

	auto context = instance->context;
	WINPR_ASSERT(context);

	auto sdl = get_context(context);

	if (freerdp_settings_get_bool(context->settings, FreeRDP_UseMultimon))
	{
		const auto driver = SDL_GetCurrentVideoDriver();
		bool buggy = false;
		if (driver)
		{
			if (strcmp(driver, "wayland") == 0)
				buggy = true;
			else if (strcmp(driver, "x11") == 0)
			{
				auto env = SDL_GetEnvironment();
				auto xdg = SDL_GetEnvironmentVariable(env, "XDG_SESSION_TYPE");
				auto qpa = SDL_GetEnvironmentVariable(env, "QT_QPA_PLATFORM");
				if (xdg && (strcmp(xdg, "wayland") == 0))
					buggy = true;
				else if (qpa && (strcmp(qpa, "wayland") == 0))
					buggy = true;
			}
		}

		if (buggy)
		{
			const auto name = SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING);

			WLog_Print(sdl->getWLog(), WLOG_WARN,
			           "%s is affected by wayland bug "
			           "https://gitlab.freedesktop.org/wayland/wayland-protocols/-/issues/179",
			           name);
			WLog_Print(
			    sdl->getWLog(), WLOG_WARN,
			    "you will not be able to properly use all monitors for FreeRDP unless this is "
			    "resolved and the SDL library you are using supports this.");
			WLog_Print(sdl->getWLog(), WLOG_WARN,
			           "For the time being run %s from an X11 session or only use single monitor "
			           "fullscreen /f",
			           name);
		}
	}

	// Retry was successful, discard dialog
	sdl->getDialog().show(false);

	if (freerdp_settings_get_bool(context->settings, FreeRDP_AuthenticationOnly))
	{
		/* Check +auth-only has a username and password. */
		if (!freerdp_settings_get_string(context->settings, FreeRDP_Password))
		{
			WLog_Print(sdl->getWLog(), WLOG_INFO,
			           "auth-only, but no password set. Please provide one.");
			return FALSE;
		}

		WLog_Print(sdl->getWLog(), WLOG_INFO, "Authentication only. Don't connect to X.");
		return TRUE;
	}

	if (!sdl->waitForWindowsCreated())
		return FALSE;

	sdl->_sdlPixelFormat = SDL_PIXELFORMAT_BGRA32;
	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
		return FALSE;

	if (!sdl->createPrimary())
		return FALSE;

	if (!sdl_register_pointer(instance->context->graphics))
		return FALSE;

	WINPR_ASSERT(context->update);

	context->update->BeginPaint = beginPaint;
	context->update->EndPaint = endPaint;
	context->update->PlaySound = playSound;
	context->update->DesktopResize = desktopResize;
	context->update->SetKeyboardIndicators = sdlInput::keyboard_set_indicators;
	context->update->SetKeyboardImeStatus = sdlInput::keyboard_set_ime_status;

	if (!sdl->setResizeable(false))
		return FALSE;
	if (!sdl->setFullscreen(freerdp_settings_get_bool(context->settings, FreeRDP_Fullscreen) ||
	                            freerdp_settings_get_bool(context->settings, FreeRDP_UseMultimon),
	                        true))
		return FALSE;
	sdl->setConnected(true);
	return TRUE;
}

/* This function is called whether a session ends by failure or success.
 * Clean up everything allocated by pre_connect and post_connect.
 */
void SdlContext::postDisconnect(freerdp* instance)
{
	if (!instance)
		return;

	if (!instance->context)
		return;

	auto sdl = get_context(instance->context);
	sdl->setConnected(false);

	gdi_free(instance);
}

void SdlContext::postFinalDisconnect(freerdp* instance)
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

/* Create a SDL surface from the GDI buffer */
bool SdlContext::createPrimary()
{
	auto gdi = context()->gdi;
	WINPR_ASSERT(gdi);

	_primary = SDLSurfacePtr(
	    SDL_CreateSurfaceFrom(static_cast<int>(gdi->width), static_cast<int>(gdi->height),
	                          pixelFormat(), gdi->primary_buffer, static_cast<int>(gdi->stride)),
	    SDL_DestroySurface);
	if (!_primary)
		return false;

	SDL_SetSurfaceBlendMode(_primary.get(), SDL_BLENDMODE_NONE);
	SDL_Rect surfaceRect = { 0, 0, gdi->width, gdi->height };
	SDL_FillSurfaceRect(_primary.get(), &surfaceRect,
	                    SDL_MapSurfaceRGBA(_primary.get(), 0, 0, 0, 0xff));

	return true;
}

bool SdlContext::createWindows()
{
	auto settings = context()->settings;
	const auto& title = windowTitle();

	ScopeGuard guard1([&]() { _windowsCreatedEvent.set(); });

	UINT32 windowCount = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);

	Sint32 originX = 0;
	Sint32 originY = 0;
	for (UINT32 x = 0; x < windowCount; x++)
	{
		auto id = monitorId(x);
		if (id < 0)
			return false;

		auto monitor = static_cast<rdpMonitor*>(
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, x));

		originX = std::min<Sint32>(monitor->x, originX);
		originY = std::min<Sint32>(monitor->y, originY);
	}

	for (UINT32 x = 0; x < windowCount; x++)
	{
		auto id = monitorId(x);
		if (id < 0)
			return false;

		auto monitor = static_cast<rdpMonitor*>(
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, x));

		Uint32 w = WINPR_ASSERTING_INT_CAST(Uint32, monitor->width);
		Uint32 h = WINPR_ASSERTING_INT_CAST(Uint32, monitor->height);
		if (!(freerdp_settings_get_bool(settings, FreeRDP_UseMultimon) ||
		      freerdp_settings_get_bool(settings, FreeRDP_Fullscreen)))
		{
			if (_windowWidth > 0)
				w = _windowWidth;
			else
				w = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);

			if (_windowHeigth > 0)
				h = _windowHeigth;
			else
				h = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
		}

		Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;

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

		auto did = WINPR_ASSERTING_INT_CAST(SDL_DisplayID, id);
		auto window = SdlWindow::create(did, title, flags, w, h);

		if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		{
			window.setOffsetX(originX - monitor->x);
			window.setOffsetY(originY - monitor->y);
		}

		_windows.insert({ window.id(), std::move(window) });
	}

	return true;
}

bool SdlContext::updateWindowList()
{
	std::vector<rdpMonitor> list;
	list.reserve(_windows.size());
	for (const auto& win : _windows)
		list.push_back(win.second.monitor(_windows.size() == 1));

	return freerdp_settings_set_monitor_def_array_sorted(context()->settings, list.data(),
	                                                     list.size());
}

std::string SdlContext::windowTitle() const
{
	const char* prefix = "FreeRDP:";

	const auto windowTitle = freerdp_settings_get_string(context()->settings, FreeRDP_WindowTitle);
	if (windowTitle)
		return windowTitle;

	const auto name = freerdp_settings_get_server_name(context()->settings);
	const auto port = freerdp_settings_get_uint32(context()->settings, FreeRDP_ServerPort);
	const auto addPort = (port != 3389);

	std::stringstream ss;
	ss << prefix << " " << name;

	if (addPort)
		ss << ":" << port;

	return ss.str();
}

bool SdlContext::waitForWindowsCreated()
{
	{
		std::unique_lock<CriticalSection> lock(_critical);
		_windowsCreatedEvent.clear();
		if (!sdl_push_user_event(SDL_EVENT_USER_CREATE_WINDOWS, this))
			return false;
	}

	HANDLE handles[] = { _windowsCreatedEvent.handle(), freerdp_abort_event(context()) };

	const DWORD rc = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
	switch (rc)
	{
		case WAIT_OBJECT_0:
			return true;
		default:
			return false;
	}
}

/* This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
BOOL SdlContext::endPaint(rdpContext* context)
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

void SdlContext::sdl_client_cleanup(int exit_code, const std::string& error_msg)
{
	rdpSettings* settings = context()->settings;
	WINPR_ASSERT(settings);

	_rdpThreadRunning = false;
	bool showError = false;
	if (freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
		WLog_Print(getWLog(), WLOG_INFO, "Authentication only, exit status %s [%" PRId32 "]",
		           sdl::error::exitCodeToTag(exit_code), exit_code);
	else
	{
		switch (exit_code)
		{
			case sdl::error::SUCCESS:
			case sdl::error::DISCONNECT:
			case sdl::error::LOGOFF:
			case sdl::error::DISCONNECT_BY_USER:
			case sdl::error::CONNECT_CANCELLED:
				break;
			default:
			{
				getDialog().showError(error_msg);
			}
			break;
		}
	}

	if (!showError)
		getDialog().show(false);

	_exitCode = exit_code;
	std::ignore = sdl_push_user_event(SDL_EVENT_USER_QUIT);
	SDL_CleanupTLS();
}

int SdlContext::sdl_client_thread_connect(std::string& error_msg)
{
	auto instance = context()->instance;
	WINPR_ASSERT(instance);

	_rdpThreadRunning = true;
	BOOL rc = freerdp_connect(instance);

	rdpSettings* settings = context()->settings;
	WINPR_ASSERT(settings);

	int exit_code = sdl::error::SUCCESS;
	if (!rc)
	{
		UINT32 error = freerdp_get_last_error(context());
		exit_code = sdl::error::errorToExitCode(error);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
	{
		DWORD code = freerdp_get_last_error(context());
		freerdp_abort_connect_context(context());
		WLog_Print(getWLog(), WLOG_ERROR, "Authentication only, %s [0x%08" PRIx32 "] %s",
		           freerdp_get_last_error_name(code), code, freerdp_get_last_error_string(code));
		return exit_code;
	}

	if (!rc)
	{
		DWORD code = freerdp_error_info(instance);
		if (exit_code == sdl::error::SUCCESS)
		{
			char* msg = nullptr;
			size_t len = 0;
			exit_code = error_info_to_error(&code, &msg, &len);
			if (msg)
				error_msg = msg;
			free(msg);
		}

		auto last = freerdp_get_last_error(context());
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

		if (exit_code == sdl::error::SUCCESS)
		{
			if (last == FREERDP_ERROR_AUTHENTICATION_FAILED)
				exit_code = sdl::error::AUTH_FAILURE;
			else if (code == ERRINFO_SUCCESS)
				exit_code = sdl::error::CONN_FAILED;
		}

		getDialog().show(false);
	}

	return exit_code;
}

int SdlContext::sdl_client_thread_run(std::string& error_msg)
{
	auto instance = context()->instance;
	WINPR_ASSERT(instance);

	int exit_code = sdl::error::SUCCESS;
	while (!freerdp_shall_disconnect_context(context()))
	{
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = {};
		/*
		 * win8 and server 2k12 seem to have some timing issue/race condition
		 * when a initial sync request is send to sync the keyboard indicators
		 * sending the sync event twice fixed this problem
		 */
		if (freerdp_focus_required(instance))
		{
			auto ctx = get_context(context());
			WINPR_ASSERT(ctx);

			auto& input = ctx->getInputChannelContext();
			if (!input.keyboard_focus_in())
				break;
			if (!input.keyboard_focus_in())
				break;
		}

		const DWORD nCount = freerdp_get_event_handles(context(), handles, ARRAYSIZE(handles));

		if (nCount == 0)
		{
			WLog_Print(getWLog(), WLOG_ERROR, "freerdp_get_event_handles failed");
			break;
		}

		const DWORD status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_Print(getWLog(), WLOG_ERROR, "WaitForMultipleObjects WAIT_FAILED");
			break;
		}

		if (!freerdp_check_event_handles(context()))
		{
			if (client_auto_reconnect(instance))
			{
				// Retry was successful, discard dialog
				getDialog().show(false);
				continue;
			}
			else
			{
				/*
				 * Indicate an unsuccessful connection attempt if reconnect
				 * did not succeed and no other error was specified.
				 */
				if (freerdp_error_info(instance) == 0)
					exit_code = sdl::error::CONN_FAILED;
			}

			if (freerdp_get_last_error(context()) == FREERDP_ERROR_SUCCESS)
				WLog_Print(getWLog(), WLOG_ERROR, "WaitForMultipleObjects failed with %" PRIu32 "",
				           status);
			if (freerdp_get_last_error(context()) == FREERDP_ERROR_SUCCESS)
				WLog_Print(getWLog(), WLOG_ERROR, "Failed to check FreeRDP event handles");
			break;
		}
	}

	if (exit_code == sdl::error::SUCCESS)
	{
		DWORD code = 0;
		{
			char* emsg = nullptr;
			size_t elen = 0;
			exit_code = error_info_to_error(&code, &emsg, &elen);
			if (emsg)
				error_msg = emsg;
			free(emsg);
		}

		if ((code == ERRINFO_LOGOFF_BY_USER) &&
		    (freerdp_get_disconnect_ultimatum(context()) == Disconnect_Ultimatum_user_requested))
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
			WLog_Print(getWLog(), WLOG_INFO, "%s", msg);
			exit_code = sdl::error::LOGOFF;
		}
	}

	freerdp_disconnect(instance);

	return exit_code;
}

/* RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends. */
DWORD SdlContext::rdpThreadRun(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	std::string error_msg;
	int exit_code = sdl->sdl_client_thread_connect(error_msg);
	if (exit_code == sdl::error::SUCCESS)
		exit_code = sdl->sdl_client_thread_run(error_msg);
	sdl->sdl_client_cleanup(exit_code, error_msg);

	return static_cast<DWORD>(exit_code);
}

int SdlContext::error_info_to_error(DWORD* pcode, char** msg, size_t* len) const
{
	const DWORD code = freerdp_error_info(context()->instance);
	const char* name = freerdp_get_error_info_name(code);
	const char* str = freerdp_get_error_info_string(code);
	const int exit_code = sdl::error::errorToExitCode(code);

	winpr_asprintf(msg, len, "Terminate with %s due to ERROR_INFO %s [0x%08" PRIx32 "]: %s",
	               sdl::error::errorToExitCodeTag(code), name, code, str);
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", *msg);
	if (pcode)
		*pcode = code;
	return exit_code;
}

void SdlContext::applyMonitorOffset(SDL_WindowID window, float& x, float& y) const
{
	if (!freerdp_settings_get_bool(context()->settings, FreeRDP_UseMultimon))
		return;

	auto w = getWindowForId(window);
	x -= static_cast<float>(w->offsetX());
	y -= static_cast<float>(w->offsetY());
}

static bool alignX(const SDL_Rect& a, const SDL_Rect& b)
{
	if (a.x + a.w == b.x)
		return true;
	if (b.x + b.w == a.x)
		return true;
	return false;
}

static bool alignY(const SDL_Rect& a, const SDL_Rect& b)
{
	if (a.y + a.h == b.y)
		return true;
	if (b.y + b.h == a.y)
		return true;
	return false;
}

std::vector<SDL_DisplayID>
SdlContext::updateDisplayOffsetsForNeighbours(SDL_DisplayID id,
                                              const std::vector<SDL_DisplayID>& ignore)
{
	auto first = _offsets.at(id);
	std::vector<SDL_DisplayID> neighbours;

	for (auto& entry : _offsets)
	{
		if (entry.first == id)
			continue;
		if (std::find(ignore.begin(), ignore.end(), entry.first) != ignore.end())
			continue;

		bool neighbor = false;
		if (alignX(entry.second.first, first.first))
		{
			if (entry.second.first.x < first.first.x)
				entry.second.second.x = first.second.x - entry.second.second.w;
			else
				entry.second.second.x = first.second.x + first.second.w;
			neighbor = true;
		}
		if (alignY(entry.second.first, first.first))
		{
			if (entry.second.first.y < first.first.y)
				entry.second.second.y = first.second.y - entry.second.second.h;
			else
				entry.second.second.y = first.second.y + first.second.h;
			neighbor = true;
		}

		if (neighbor)
			neighbours.push_back(entry.first);
	}
	return neighbours;
}

void SdlContext::updateMonitorDataFromOffsets()
{
	for (auto& entry : _displays)
	{
		auto offsets = _offsets.at(entry.first);
		entry.second.x = offsets.second.x;
		entry.second.y = offsets.second.y;
	}

	for (auto& entry : _windows)
	{
		const auto& monitor = _displays.at(entry.first);
		entry.second.setMonitor(monitor);
	}
}

bool SdlContext::drawToWindow(SdlWindow& window, const std::vector<SDL_Rect>& rects)
{
	if (!isConnected())
		return true;

	auto gdi = context()->gdi;
	WINPR_ASSERT(gdi);

	auto size = window.rect();

	std::unique_lock lock(_critical);
	auto surface = _primary.get();
	if (freerdp_settings_get_bool(context()->settings, FreeRDP_SmartSizing))
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

		_localScale = { static_cast<float>(size.w) / static_cast<float>(gdi->width),
			            static_cast<float>(size.h) / static_cast<float>(gdi->height) };
		if (!window.drawScaledRects(surface, _localScale, rects))
			return false;
	}
	else
	{
		SDL_Point offset{ 0, 0 };
		if (freerdp_settings_get_bool(context()->settings, FreeRDP_UseMultimon))
			offset = { window.offsetX(), window.offsetY() };
		if (!window.drawRects(surface, offset, rects))
			return false;
	}

	window.updateSurface();
	return true;
}

bool SdlContext::minimizeAllWindows()
{
	for (auto& w : _windows)
		w.second.minimize();
	return true;
}

int SdlContext::exitCode() const
{
	return _exitCode;
}

SDL_PixelFormat SdlContext::pixelFormat() const
{
	return _sdlPixelFormat;
}

bool SdlContext::addDisplayWindow(SDL_DisplayID id)
{
	const auto flags =
	    SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
	auto title = sdl::utils::windowTitle(context()->settings);
	auto w = SdlWindow::create(id, title, flags);
	_windows.emplace(w.id(), std::move(w));
	return true;
}

bool SdlContext::removeDisplayWindow(SDL_DisplayID id)
{
	for (auto& w : _windows)
	{
		if (w.second.displayIndex() == id)
			_windows.erase(w.first);
	}
	return true;
}

bool SdlContext::detectDisplays()
{
	int count = 0;
	auto display = SDL_GetDisplays(&count);
	if (!display)
		return false;
	for (int x = 0; x < count; x++)
	{
		const auto id = display[x];
		addOrUpdateDisplay(id);
	}

	return true;
}

rdpMonitor SdlContext::getDisplay(SDL_DisplayID id) const
{
	return _displays.at(id);
}

std::vector<SDL_DisplayID> SdlContext::getDisplayIds() const
{
	std::vector<SDL_DisplayID> keys;
	keys.reserve(_displays.size());
	for (const auto& entry : _displays)
	{
		keys.push_back(entry.first);
	}
	return keys;
}

const SdlWindow* SdlContext::getWindowForId(SDL_WindowID id) const
{
	auto it = _windows.find(id);
	if (it == _windows.end())
		return nullptr;
	return &it->second;
}

SdlWindow* SdlContext::getWindowForId(SDL_WindowID id)
{
	auto it = _windows.find(id);
	if (it == _windows.end())
		return nullptr;
	return &it->second;
}

SdlWindow* SdlContext::getFirstWindow()
{
	if (_windows.empty())
		return nullptr;
	return &_windows.begin()->second;
}

sdlDispContext& SdlContext::getDisplayChannelContext()
{
	return _disp;
}

sdlInput& SdlContext::getInputChannelContext()
{
	return _input;
}

sdlClip& SdlContext::getClipboardChannelContext()
{
	return _clip;
}

SdlConnectionDialogWrapper& SdlContext::getDialog()
{
	return _dialog;
}

wLog* SdlContext::getWLog()
{
	return _log;
}

bool SdlContext::moveMouseTo(const SDL_FPoint& pos)
{
	auto window = SDL_GetMouseFocus();
	if (!window)
		return true;

	const auto id = SDL_GetWindowID(window);
	const auto spos = pixelToScreen(id, pos);
	SDL_WarpMouseInWindow(window, spos.x, spos.y);
	return true;
}

bool SdlContext::handleEvent(const SDL_MouseMotionEvent& ev)
{
	SDL_Event copy{};
	copy.motion = ev;
	if (!eventToPixelCoordinates(ev.windowID, copy))
		return false;
	removeLocalScaling(copy.motion.x, copy.motion.y);
	removeLocalScaling(copy.motion.xrel, copy.motion.yrel);
	applyMonitorOffset(copy.motion.windowID, copy.motion.x, copy.motion.y);

	return SdlTouch::handleEvent(this, copy.motion);
}

bool SdlContext::handleEvent(const SDL_MouseWheelEvent& ev)
{
	SDL_Event copy{};
	copy.wheel = ev;
	if (!eventToPixelCoordinates(ev.windowID, copy))
		return false;
	removeLocalScaling(copy.wheel.mouse_x, copy.wheel.mouse_y);
	return SdlTouch::handleEvent(this, copy.wheel);
}

bool SdlContext::handleEvent(const SDL_WindowEvent& ev)
{
	if (!getDisplayChannelContext().handleEvent(ev))
		return false;

	auto window = getWindowForId(ev.windowID);
	if (!window)
		return true;

	{
		const auto& r = window->rect();
		const auto& b = window->bounds();
		const auto& scale = window->scale();
		const auto& orientation = window->orientation();
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
		             "%s: [%u] %dx%d-%dx%d {%dx%d-%dx%d}{scale=%f,orientation=%s}",
		             sdl::utils::toString(ev.type).c_str(), ev.windowID, r.x, r.y, r.w, r.h, b.x,
		             b.y, b.w, b.h, static_cast<double>(scale),
		             sdl::utils::toString(orientation).c_str());
	}

	switch (ev.type)
	{
		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			return restoreCursor();
		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
			if (isConnected())
			{
				if (!window->fill())
					return false;
				if (!drawToWindow(*window))
					return false;
				if (!restoreCursor())
					return false;
			}
			break;
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			if (!window->fill())
				return false;
			if (!drawToWindow(*window))
				return false;
			if (!restoreCursor())
				return false;
			break;
		case SDL_EVENT_WINDOW_MOVED:
		{
			auto r = window->rect();
			auto id = window->id();
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%u: %dx%d-%dx%d", id, r.x, r.y, r.w, r.h);
		}
		break;
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		{
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Window closed, terminating RDP session...");
			freerdp_abort_connect_context(context());
		}
		break;
		default:
			break;
	}
	return true;
}

bool SdlContext::handleEvent(const SDL_DisplayEvent& ev)
{
	if (!getDisplayChannelContext().handleEvent(ev))
		return false;

	switch (ev.type)
	{
		case SDL_EVENT_DISPLAY_REMOVED: // Can't show details for this one...
			break;
		default:
		{
			SDL_Rect r = {};
			if (!SDL_GetDisplayBounds(ev.displayID, &r))
				return false;
			const auto name = SDL_GetDisplayName(ev.displayID);
			if (!name)
				return false;
			const auto orientation = SDL_GetCurrentDisplayOrientation(ev.displayID);
			const auto scale = SDL_GetDisplayContentScale(ev.displayID);
			const auto mode = SDL_GetCurrentDisplayMode(ev.displayID);
			if (!mode)
				return false;

			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
			             "%s: [%u, %s] %dx%d-%dx%d {orientation=%s, scale=%f}%s",
			             sdl::utils::toString(ev.type).c_str(), ev.displayID, name, r.x, r.y, r.w,
			             r.h, sdl::utils::toString(orientation).c_str(), static_cast<double>(scale),
			             sdl::utils::toString(mode).c_str());
		}
		break;
	}
	return true;
}

bool SdlContext::handleEvent(const SDL_MouseButtonEvent& ev)
{
	SDL_Event copy = {};
	copy.button = ev;
	if (!eventToPixelCoordinates(ev.windowID, copy))
		return false;
	removeLocalScaling(copy.button.x, copy.button.y);
	applyMonitorOffset(copy.button.windowID, copy.button.x, copy.button.y);
	return SdlTouch::handleEvent(this, copy.button);
}

bool SdlContext::handleEvent(const SDL_TouchFingerEvent& ev)
{
	SDL_Event copy{};
	copy.tfinger = ev;
	if (!eventToPixelCoordinates(ev.windowID, copy))
		return false;
	removeLocalScaling(copy.tfinger.dx, copy.tfinger.dy);
	removeLocalScaling(copy.tfinger.x, copy.tfinger.y);
	applyMonitorOffset(copy.tfinger.windowID, copy.tfinger.x, copy.tfinger.y);
	return SdlTouch::handleEvent(this, copy.tfinger);
}

void SdlContext::addOrUpdateDisplay(SDL_DisplayID id)
{
	auto monitor = SdlWindow::query(id, false);
	_displays.emplace(id, monitor);

	/* Update actual display rectangles:
	 *
	 * 1. Get logical display bounds
	 * 2. Use already known pixel width and height
	 * 3. Iterate over each display and update the x and y offsets by adding all monitor
	 * widths/heights from the primary
	 */
	_offsets.clear();
	for (auto& entry : _displays)
	{
		SDL_Rect bounds{};
		std::ignore = SDL_GetDisplayBounds(entry.first, &bounds);

		SDL_Rect pixel{};
		pixel.w = entry.second.width;
		pixel.h = entry.second.height;
		_offsets.emplace(entry.first, std::pair{ bounds, pixel });
	}

	/* 1. Find primary and update all neighbors
	 * 2. For each neighbor update all neighbors
	 * 3. repeat until all displays updated.
	 */
	const auto primary = SDL_GetPrimaryDisplay();
	std::vector<SDL_DisplayID> handled;
	handled.push_back(primary);

	auto neighbors = updateDisplayOffsetsForNeighbours(primary);
	while (!neighbors.empty())
	{
		auto neighbor = *neighbors.begin();
		neighbors.pop_back();

		if (std::find(handled.begin(), handled.end(), neighbor) != handled.end())
			continue;
		handled.push_back(neighbor);

		auto next = updateDisplayOffsetsForNeighbours(neighbor, handled);
		neighbors.insert(neighbors.end(), next.begin(), next.end());
	}
	updateMonitorDataFromOffsets();
}

void SdlContext::deleteDisplay(SDL_DisplayID id)
{
	_displays.erase(id);
}

bool SdlContext::eventToPixelCoordinates(SDL_WindowID id, SDL_Event& ev)
{
	auto w = getWindowForId(id);
	if (!w)
		return false;

	/* Ignore errors here, sometimes SDL has no renderer */
	auto renderer = SDL_GetRenderer(w->window());
	if (!renderer)
		return true;
	return SDL_ConvertEventToRenderCoordinates(renderer, &ev);
}

SDL_FPoint SdlContext::applyLocalScaling(const SDL_FPoint& val) const
{
	if (!freerdp_settings_get_bool(context()->settings, FreeRDP_SmartSizing))
		return val;

	auto rval = val;
	rval.x *= _localScale.x;
	rval.y *= _localScale.y;
	return rval;
}

void SdlContext::removeLocalScaling(float& x, float& y) const
{
	if (!freerdp_settings_get_bool(context()->settings, FreeRDP_SmartSizing))
		return;
	x /= _localScale.x;
	y /= _localScale.y;
}

SDL_FPoint SdlContext::screenToPixel(SDL_WindowID id, const SDL_FPoint& pos)
{
	auto w = getWindowForId(id);
	if (!w)
		return {};

	/* Ignore errors here, sometimes SDL has no renderer */
	auto renderer = SDL_GetRenderer(w->window());
	if (!renderer)
		return pos;

	SDL_FPoint rpos{};
	if (!SDL_RenderCoordinatesFromWindow(renderer, pos.x, pos.y, &rpos.x, &rpos.y))
		return {};
	removeLocalScaling(rpos.x, rpos.y);
	return rpos;
}

SDL_FPoint SdlContext::pixelToScreen(SDL_WindowID id, const SDL_FPoint& pos)
{
	auto w = getWindowForId(id);
	if (!w)
		return {};

	/* Ignore errors here, sometimes SDL has no renderer */
	auto renderer = SDL_GetRenderer(w->window());
	if (!renderer)
		return pos;

	SDL_FPoint rpos{};
	if (!SDL_RenderCoordinatesToWindow(renderer, pos.x, pos.y, &rpos.x, &rpos.y))
		return {};
	return applyLocalScaling(rpos);
}

SDL_FRect SdlContext::pixelToScreen(SDL_WindowID id, const SDL_FRect& pos)
{
	const auto fpos = pixelToScreen(id, SDL_FPoint{ pos.x, pos.y });
	const auto size = pixelToScreen(id, SDL_FPoint{ pos.w, pos.h });
	return { fpos.x, fpos.y, size.x, size.y };
}

bool SdlContext::handleEvent(const SDL_Event& ev)
{
	if ((ev.type >= SDL_EVENT_DISPLAY_FIRST) && (ev.type <= SDL_EVENT_DISPLAY_LAST))
	{
		const auto& dev = ev.display;
		return handleEvent(dev);
	}
	if ((ev.type >= SDL_EVENT_WINDOW_FIRST) && (ev.type <= SDL_EVENT_WINDOW_LAST))
	{
		const auto& wev = ev.window;
		return handleEvent(wev);
	}
	switch (ev.type)
	{
		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_MOTION:
		{
			const auto& cev = ev.tfinger;
			return handleEvent(cev);
		}
		case SDL_EVENT_MOUSE_MOTION:
		{
			const auto& cev = ev.motion;
			return handleEvent(cev);
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			const auto& cev = ev.button;
			return handleEvent(cev);
		}
		case SDL_EVENT_MOUSE_WHEEL:
		{
			const auto& cev = ev.wheel;
			return handleEvent(cev);
		}
		case SDL_EVENT_CLIPBOARD_UPDATE:
		{
			const auto& cev = ev.clipboard;
			return getClipboardChannelContext().handleEvent(cev);
		}
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		{
			const auto& cev = ev.key;
			return getInputChannelContext().handleEvent(cev);
		}
		case SDL_EVENT_RENDER_TARGETS_RESET:
		case SDL_EVENT_RENDER_DEVICE_RESET:
		case SDL_EVENT_WILL_ENTER_FOREGROUND:
			return redraw();
		default:
			return true;
	}
}

bool SdlContext::drawToWindows(const std::vector<SDL_Rect>& rects)
{
	for (auto& window : _windows)
	{
		if (!drawToWindow(window.second, rects))
			return FALSE;
	}

	return TRUE;
}

BOOL SdlContext::desktopResize(rdpContext* context)
{
	rdpGdi* gdi = nullptr;
	rdpSettings* settings = nullptr;
	auto sdl = get_context(context);

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	std::unique_lock lock(sdl->_critical);
	gdi = context->gdi;
	if (!gdi_resize(gdi, freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
	                freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight)))
		return FALSE;
	return sdl->createPrimary();
}

/* This function is called to output a System BEEP */
BOOL SdlContext::playSound(rdpContext* context, const PLAY_SOUND_UPDATE* play_sound)
{
	/* TODO: Implement */
	WINPR_UNUSED(context);
	WINPR_UNUSED(play_sound);
	return TRUE;
}

/* This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas. */
BOOL SdlContext::beginPaint(rdpContext* context)
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

bool SdlContext::setCursor(CursorType type)
{
	_cursorType = type;
	return restoreCursor();
}

bool SdlContext::setCursor(rdpPointer* cursor)
{
	_cursor = cursor;
	return setCursor(CURSOR_IMAGE);
}

rdpPointer* SdlContext::cursor() const
{
	return _cursor;
}

bool SdlContext::restoreCursor()
{
	WLog_Print(getWLog(), WLOG_DEBUG, "restore cursor: %d", _cursorType);
	switch (_cursorType)
	{
		case CURSOR_NULL:
			if (!SDL_HideCursor())
			{
				WLog_Print(getWLog(), WLOG_ERROR, "SDL_HideCursor failed");
				return false;
			}

			setHasCursor(false);
			return true;

		case CURSOR_DEFAULT:
		{
			auto def = SDL_GetDefaultCursor();
			if (!SDL_SetCursor(def))
			{
				WLog_Print(getWLog(), WLOG_ERROR, "SDL_SetCursor(default=%p) failed",
				           static_cast<void*>(def));
				return false;
			}
			if (!SDL_ShowCursor())
			{
				WLog_Print(getWLog(), WLOG_ERROR, "SDL_ShowCursor failed");
				return false;
			}
			setHasCursor(true);
			return true;
		}
		case CURSOR_IMAGE:
			setHasCursor(true);
			return sdl_Pointer_Set_Process(this);
		default:
			WLog_Print(getWLog(), WLOG_ERROR, "Unknown cursorType %s",
			           sdl::utils::toString(_cursorType).c_str());
			return false;
	}
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
	return _monitorIds.at(index);
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

bool SdlContext::setFullscreen(bool enter, bool forceOriginalDisplay)
{
	for (const auto& window : _windows)
	{
		if (!sdl_push_user_event(SDL_EVENT_USER_WINDOW_FULLSCREEN, &window.second, enter,
		                         forceOriginalDisplay))
			return false;
	}
	_fullscreen = enter;
	return true;
}

bool SdlContext::setMinimized()
{
	return sdl_push_user_event(SDL_EVENT_USER_WINDOW_MINIMIZE);
}

bool SdlContext::grabMouse() const
{
	return _grabMouse;
}

bool SdlContext::toggleGrabMouse()
{
	return setGrabMouse(!grabMouse());
}

bool SdlContext::setGrabMouse(bool enter)
{
	_grabMouse = enter;
	return true;
}

bool SdlContext::grabKeyboard() const
{
	return _grabKeyboard;
}

bool SdlContext::toggleGrabKeyboard()
{
	return setGrabKeyboard(!grabKeyboard());
}

bool SdlContext::setGrabKeyboard(bool enter)
{
	_grabKeyboard = enter;
	return true;
}

bool SdlContext::setResizeable(bool enable)
{
	const auto settings = context()->settings;
	const bool dyn = freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate);
	const bool smart = freerdp_settings_get_bool(settings, FreeRDP_SmartSizing);
	bool use = (dyn && enable) || smart;

	for (const auto& window : _windows)
	{
		if (!sdl_push_user_event(SDL_EVENT_USER_WINDOW_RESIZEABLE, &window.second, use))
			return false;
	}
	_resizeable = use;

	return true;
}

bool SdlContext::resizeable() const
{
	return _resizeable;
}

bool SdlContext::toggleResizeable()
{
	return setResizeable(!resizeable());
}

bool SdlContext::fullscreen() const
{
	return _fullscreen;
}

bool SdlContext::toggleFullscreen()
{
	return setFullscreen(!fullscreen());
}
