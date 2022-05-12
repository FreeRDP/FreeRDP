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

#include <freerdp/config.h>

#include "mfreerdp.h"

#include <winpr/assert.h>

#include <freerdp/constants.h>
#include <freerdp/utils/signal.h>
#include <freerdp/client/cmdline.h>

#include "MRDPView.h"

/**
 * Client Interface
 */

static BOOL mfreerdp_client_global_init(void)
{
	freerdp_handle_signals();
	return TRUE;
}

static void mfreerdp_client_global_uninit(void)
{
}

static int mfreerdp_client_start(rdpContext *context)
{
	MRDPView *view;
	mfContext *mfc = (mfContext *)context;

	if (mfc->view == NULL)
	{
		// view not specified beforehand. Create view dynamically
		mfc->view =
		    [[MRDPView alloc] initWithFrame:NSMakeRect(0, 0, context->settings->DesktopWidth,
		                                               context->settings->DesktopHeight)];
		mfc->view_ownership = TRUE;
	}

	view = (MRDPView *)mfc->view;
	return [view rdpStart:context];
}

static int mfreerdp_client_stop(rdpContext *context)
{
	mfContext *mfc = (mfContext *)context;

	freerdp_client_common_stop(context);

	if (mfc->view_ownership)
	{
		MRDPView *view = (MRDPView *)mfc->view;
		[view releaseResources];
		[view release];
		mfc->view = nil;
	}

	return 0;
}

static BOOL mfreerdp_client_new(freerdp *instance, rdpContext *context)
{
	mfContext *mfc;

	WINPR_ASSERT(instance);

	mfc = (mfContext *)instance->context;
	WINPR_ASSERT(mfc);

	mfc->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	context->instance->PreConnect = mac_pre_connect;
	context->instance->PostConnect = mac_post_connect;
	context->instance->PostDisconnect = mac_post_disconnect;
	context->instance->Authenticate = mac_authenticate;
	context->instance->GatewayAuthenticate = mac_gw_authenticate;
	context->instance->VerifyCertificateEx = mac_verify_certificate_ex;
	context->instance->VerifyChangedCertificateEx = mac_verify_changed_certificate_ex;
	context->instance->LogonErrorInfo = mac_logon_error_info;
	return TRUE;
}

static void mfreerdp_client_free(freerdp *instance, rdpContext *context)
{
	mfContext *mfc;

	if (!instance || !context)
		return;

	mfc = (mfContext *)instance->context;
	CloseHandle(mfc->stopEvent);
}

static void mf_scale_mouse_coordinates(mfContext *mfc, UINT16 *px, UINT16 *py)
{
	UINT16 x = *px;
	UINT16 y = *py;
	UINT32 ww = mfc->client_width;
	UINT32 wh = mfc->client_height;
	UINT32 dw = mfc->common.context.settings->DesktopWidth;
	UINT32 dh = mfc->common.context.settings->DesktopHeight;

	if (!mfc->common.context.settings->SmartSizing || ((ww == dw) && (wh == dh)))
	{
		y = y + mfc->yCurrentScroll;
		x = x + mfc->xCurrentScroll;

		y -= (dh - wh);
		x -= (dw - ww);
	}
	else
	{
		y = y * dh / wh + mfc->yCurrentScroll;
		x = x * dw / ww + mfc->xCurrentScroll;
	}

	*px = x;
	*py = y;
}

void mf_scale_mouse_event(void *context, UINT16 flags, UINT16 x, UINT16 y)
{
	mfContext *mfc = (mfContext *)context;
	MRDPView *view = (MRDPView *)mfc->view;
	// Convert to windows coordinates
	y = [view frame].size.height - y;

	if ((flags & (PTR_FLAGS_WHEEL | PTR_FLAGS_HWHEEL)) == 0)
		mf_scale_mouse_coordinates(mfc, &x, &y);
	freerdp_client_send_button_event(&mfc->common, FALSE, flags, x, y);
}

void mf_scale_mouse_event_ex(void *context, UINT16 flags, UINT16 x, UINT16 y)
{
	mfContext *mfc = (mfContext *)context;
	MRDPView *view = (MRDPView *)mfc->view;
	// Convert to windows coordinates
	y = [view frame].size.height - y;

	mf_scale_mouse_coordinates(mfc, &x, &y);
	freerdp_client_send_extended_button_event(&mfc->common, FALSE, flags, x, y);
}

void mf_press_mouse_button(void *context, int button, int x, int y, BOOL down)
{
	UINT16 flags = 0;
	UINT16 xflags = 0;

	if (down)
	{
		flags |= PTR_FLAGS_DOWN;
		xflags |= PTR_XFLAGS_DOWN;
	}

	switch (button)
	{
		case 0:
			mf_scale_mouse_event(context, flags | PTR_FLAGS_BUTTON1, x, y);
			break;

		case 1:
			mf_scale_mouse_event(context, flags | PTR_FLAGS_BUTTON2, x, y);
			break;

		case 2:
			mf_scale_mouse_event(context, flags | PTR_FLAGS_BUTTON3, x, y);
			break;

		case 3:
			mf_scale_mouse_event_ex(context, xflags | PTR_XFLAGS_BUTTON1, x, y);
			break;

		case 4:
			mf_scale_mouse_event_ex(context, xflags | PTR_XFLAGS_BUTTON2, x, y);
			break;

		default:
			break;
	}
}

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS *pEntryPoints)
{
	WINPR_ASSERT(pEntryPoints);

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
