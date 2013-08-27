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
#import "MRDPView.h"

/**
 * Client Interface
 */

void mfreerdp_client_global_init()
{
	freerdp_handle_signals();
	freerdp_channels_global_init();
}

void mfreerdp_client_global_uninit()
{
	freerdp_channels_global_uninit();
}

int mfreerdp_client_start(rdpContext* context)
{
	MRDPView* view;
	mfContext* mfc = (mfContext*) context;

    if (mfc->view == NULL)
    {
        // view not specified beforehand. Create view dynamically
        mfc->view = [[MRDPView alloc] initWithFrame : NSMakeRect(0, 0, context->settings->DesktopWidth, context->settings->DesktopHeight)];
        mfc->view_ownership = TRUE;
    }

	view = (MRDPView*) mfc->view;
	[view rdpStart:context];

	return 0;
}

int mfreerdp_client_stop(rdpContext* context)
{
	mfContext* mfc = (mfContext*) context;

	if (context->settings->AsyncUpdate)
	{
		wMessageQueue* queue;
		queue = freerdp_get_message_queue(context->instance, FREERDP_UPDATE_MESSAGE_QUEUE);
        if (queue)
        {
            MessageQueue_PostQuit(queue, 0);
        }
	}
	else if (context->settings->AsyncInput)
	{
		wMessageQueue* queue;
		queue = freerdp_get_message_queue(context->instance, FREERDP_INPUT_MESSAGE_QUEUE);
        if (queue)
        {
            MessageQueue_PostQuit(queue, 0);
        }
    }
	else
	{
		mfc->disconnect = TRUE;
	}
	
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

int mfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	mfContext* mfc;
	rdpSettings* settings;

	mfc = (mfContext*) instance->context;

    mfc->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    context->instance->PreConnect = mac_pre_connect;
    context->instance->PostConnect = mac_post_connect;
    context->instance->ReceiveChannelData = mac_receive_channel_data;
    context->instance->Authenticate = mac_authenticate;

    context->channels = freerdp_channels_new();

	settings = instance->settings;

    settings->AsyncUpdate = TRUE;
    settings->AsyncInput = TRUE;
	settings->AsyncChannels = TRUE;
	settings->AsyncTransport = TRUE;
	settings->RedirectClipboard = TRUE;

	settings->OsMajorType = OSMAJORTYPE_MACINTOSH;
	settings->OsMinorType = OSMINORTYPE_MACINTOSH;

	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = settings->BitmapCacheEnabled;

	settings->OrderSupport[NEG_MEM3BLT_INDEX] = (settings->SoftwareGdi) ? TRUE : FALSE;

	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = settings->BitmapCacheEnabled;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;

	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;

	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

	return 0;
}

void mfreerdp_client_free(freerdp* instance, rdpContext* context)
{

}

void freerdp_client_mouse_event(rdpContext* cfc, DWORD flags, int x, int y)
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

	input->MouseEvent(input, flags, x, y);
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
