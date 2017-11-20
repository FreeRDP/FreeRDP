/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mfreerdp.h"
#include <freerdp/constants.h>
#include <freerdp/utils/signal.h>
#include <freerdp/client/cmdline.h>

/**
 * Client Interface
 */

static BOOL mfreerdp_client_global_init()
{
	freerdp_handle_signals();
	return TRUE;
}

static void mfreerdp_client_global_uninit()
{
}

static int mfreerdp_client_start(rdpContext* context)
{
	MRDPView* view;
	mfContext* mfc = (mfContext*) context;

	if (mfc->view == NULL)
	{
		// view not specified beforehand. Create view dynamically
		mfc->view = [[MRDPView alloc] initWithFrame : NSMakeRect(0, 0,
		             context->settings->DesktopWidth, context->settings->DesktopHeight)];
		mfc->view_ownership = TRUE;
	}

	view = (MRDPView*) mfc->view;
	return [view rdpStart:context];
}

static int mfreerdp_client_stop(rdpContext* context)
{
	mfContext* mfc = (mfContext*) context;

	if (mfc->thread)
	{
		SetEvent(mfc->stopEvent);
		WaitForSingleObject(mfc->thread, INFINITE);
		CloseHandle(mfc->thread);
		mfc->thread = NULL;
	}

	if (mfc->view_ownership)
	{
		MRDPView* view = (MRDPView*) mfc->view;
		[view releaseResources];
		[view release];
		mfc->view = nil;
	}

	return 0;
}

static BOOL mfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	mfContext* mfc;
	rdpSettings* settings;
	mfc = (mfContext*) instance->context;
	mfc->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	context->instance->PreConnect = mac_pre_connect;
	context->instance->PostConnect = mac_post_connect;
	context->instance->Authenticate = mac_authenticate;
	settings = instance->settings;
	settings->AsyncTransport = FALSE;
	settings->AsyncUpdate = TRUE;
	settings->AsyncInput = TRUE;
	return TRUE;
}

static void mfreerdp_client_free(freerdp* instance, rdpContext* context)
{
	mfContext* mfc;
	rdpSettings* settings;

	if (!instance || !context)
		return;

	mfc = (mfContext*) instance->context;
	CloseHandle(mfc->stopEvent);
}

static void freerdp_client_mouse_event(rdpContext* cfc, DWORD flags, int x,
                                       int y)
{
	int width, height;
	rdpInput* input = cfc->instance->input;
	rdpSettings* settings = cfc->instance->settings;
	width = settings->DesktopWidth;
	height = settings->DesktopHeight;

	if (x < 0)
		x = 0;

	x = width - 1;

	if (y < 0)
		y = 0;

	if (y >= height)
		y = height - 1;

	freerdp_input_send_mouse_event(input, flags, x, y);
}

void mf_scale_mouse_event(void* context, rdpInput* input, UINT16 flags,
                          UINT16 x, UINT16 y)
{
	mfContext* mfc = (mfContext*) context;
	MRDPView* view = (MRDPView*) mfc->view;
	int ww, wh, dw, dh;
	ww = mfc->client_width;
	wh = mfc->client_height;
	dw = mfc->context.settings->DesktopWidth;
	dh = mfc->context.settings->DesktopHeight;
	// Convert to windows coordinates
	y = [view frame].size.height - y;

	if (!mfc->context.settings->SmartSizing || ((ww == dw) && (wh == dh)))
	{
		y = y + mfc->yCurrentScroll;

		if (wh != dh)
		{
			y -= (dh - wh);
		}

		freerdp_input_send_mouse_event(input, flags, x + mfc->xCurrentScroll, y);
	}
	else
	{
		y = y * dh / wh + mfc->yCurrentScroll;
		freerdp_input_send_mouse_event(input, flags, x * dw / ww + mfc->xCurrentScroll, y);
	}
}

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->Version = 1;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = mfreerdp_client_global_init;
	pEntryPoints->GlobalUninit = mfreerdp_client_global_uninit;
	pEntryPoints->ContextSize = sizeof(mfContext);
	pEntryPoints->ClientNew = mfreerdp_client_new;
	pEntryPoints->ClientFree = mfreerdp_client_free;
	pEntryPoints->ClientStart = mfreerdp_client_start;
	pEntryPoints->ClientStop = mfreerdp_client_stop;
	return 0;
}
