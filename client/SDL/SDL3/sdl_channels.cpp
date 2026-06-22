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
#include <freerdp/channels/rdpewa.h>

#include "sdl_channels.hpp"
#include "sdl_context.hpp"
#include "sdl_disp.hpp"

void sdl_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e)
{
	auto sdl = get_context(context);

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(e);

	if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
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
		uint32_t timeoutMS;
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
		    auto e = static_cast<struct userdata*>(userdata);
		    WINPR_ASSERT(e);
		    auto parent = SDL_GetMouseFocus();
		    if (!parent)
			    parent = SDL_GetKeyboardFocus();
		    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, e->sender.c_str(),
		                             e->message.c_str(), parent);
		    delete e;
	    },
	    ud, false);
}
