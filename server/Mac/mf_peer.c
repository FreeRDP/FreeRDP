/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Mac OS X Server
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#include <freerdp/listener.h>
#include <freerdp/codec/rfx.h>
#include <winpr/stream.h>
#include <freerdp/peer.h>
#include <freerdp/codec/color.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include "mf_peer.h"
#include "mf_info.h"
#include "mf_input.h"
#include "mf_event.h"
#include "mf_rdpsnd.h"
#include "mf_audin.h"

#include <mach/clock.h>
#include <mach/mach.h>
#include <dispatch/dispatch.h>

#include "OpenGL/OpenGL.h"
#include "OpenGL/gl.h"

#include "CoreVideo/CoreVideo.h"

#include <freerdp/log.h>
#define TAG SERVER_TAG("mac")

// refactor these
static int info_last_sec = 0;
static int info_last_nsec = 0;

static dispatch_source_t info_timer;
static dispatch_queue_t info_queue;

static mfEventQueue* info_event_queue;

static CGLContextObj glContext;
static CGContextRef bmp;
static CGImageRef img;

static void mf_peer_context_free(freerdp_peer* client, rdpContext* context);

static BOOL mf_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	if (info_event_queue->pipe_fd[0] == -1)
		return TRUE;

	rfds[*rcount] = (void*)(long)info_event_queue->pipe_fd[0];
	(*rcount)++;
	return TRUE;
}

static void mf_peer_rfx_update(freerdp_peer* client)
{
	// check
	mfInfo* mfi = mf_info_get_instance();
	mf_info_find_invalid_region(mfi);

	if (mf_info_have_invalid_region(mfi) == false)
	{
		return;
	}

	long width;
	long height;
	int pitch;
	BYTE* dataBits = NULL;
	mf_info_getScreenData(mfi, &width, &height, &dataBits, &pitch);
	mf_info_clear_invalid_region(mfi);
	// encode
	wStream* s;
	RFX_RECT rect;
	rdpUpdate* update;
	mfPeerContext* mfp;
	SURFACE_BITS_COMMAND cmd = { 0 };

	WINPR_ASSERT(client);

	mfp = (mfPeerContext*)client->context;
	WINPR_ASSERT(mfp);

	update = client->context->update;
	WINPR_ASSERT(update);

	s = mfp->s;
	WINPR_ASSERT(s);

	Stream_Clear(s);
	Stream_SetPosition(s, 0);
	UINT32 x = mfi->invalid.x / mfi->scale;
	UINT32 y = mfi->invalid.y / mfi->scale;
	rect.x = 0;
	rect.y = 0;
	rect.width = width;
	rect.height = height;
	mfp->rfx_context->width = mfi->servscreen_width;
	mfp->rfx_context->height = mfi->servscreen_height;

	if (!(rfx_compose_message(mfp->rfx_context, s, &rect, 1, (BYTE*)dataBits, rect.width,
	                          rect.height, pitch)))
	{
		return;
	}

	cmd.destLeft = x;
	cmd.destTop = y;
	cmd.destRight = x + rect.width;
	cmd.destBottom = y + rect.height;
	cmd.bmp.bpp = 32;
	cmd.bmp.codecID = 3;
	cmd.bmp.width = rect.width;
	cmd.bmp.height = rect.height;
	cmd.bmp.bitmapDataLength = Stream_GetPosition(s);
	cmd.bmp.bitmapData = Stream_Buffer(s);
	// send
	update->SurfaceBits(update->context, &cmd);
	// clean up... maybe?
}

static BOOL mf_peer_check_fds(freerdp_peer* client)
{
	mfPeerContext* context = (mfPeerContext*)client->context;
	mfEvent* event;

	if (context->activated == FALSE)
		return TRUE;

	event = mf_event_peek(info_event_queue);

	if (event != NULL)
	{
		if (event->type == FREERDP_SERVER_MAC_EVENT_TYPE_REGION)
		{
		}
		else if (event->type == FREERDP_SERVER_MAC_EVENT_TYPE_FRAME_TICK)
		{
			event = mf_event_pop(info_event_queue);
			mf_peer_rfx_update(client);
			mf_event_free(event);
		}
	}

	return TRUE;
}

/* Called when we have a new peer connecting */
static BOOL mf_peer_context_new(freerdp_peer* client, rdpContext* context)
{
	rdpSettings* settings;
	mfPeerContext* peer = (mfPeerContext*)context;

	WINPR_ASSERT(client);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!(peer->info = mf_info_get_instance()))
		return FALSE;

	if (!(peer->rfx_context = rfx_context_new_ex(TRUE, settings->ThreadingFlags)))
		goto fail;

	peer->rfx_context->mode = RLGR3;
	peer->rfx_context->width = settings->DesktopWidth;
	peer->rfx_context->height = settings->DesktopHeight;
	rfx_context_set_pixel_format(peer->rfx_context, PIXEL_FORMAT_BGRA32);

	if (!(peer->s = Stream_New(NULL, 0xFFFF)))
		goto fail;

	peer->vcm = WTSOpenServerA((LPSTR)client->context);

	if (!peer->vcm || (peer->vcm == INVALID_HANDLE_VALUE))
		goto fail;

	mf_info_peer_register(peer->info, peer);
	return TRUE;
fail:
	mf_peer_context_free(client, context);
	return FALSE;
}

/* Called after a peer disconnects */
static void mf_peer_context_free(freerdp_peer* client, rdpContext* context)
{
	mfPeerContext* peer = (mfPeerContext*)context;
	if (context)
	{
		mf_info_peer_unregister(peer->info, peer);
		dispatch_suspend(info_timer);
		Stream_Free(peer->s, TRUE);
		rfx_context_free(peer->rfx_context);
		// nsc_context_free(peer->nsc_context);
#ifdef CHANNEL_AUDIN_SERVER

		if (peer->audin)
			audin_server_context_free(peer->audin);

#endif
#ifdef CHANNEL_RDPSND_SERVER
		mf_peer_rdpsnd_stop();

		if (peer->rdpsnd)
			rdpsnd_server_context_free(peer->rdpsnd);

#endif
		WTSCloseServer(peer->vcm);
	}
}

/* Called when a new client connects */
static BOOL mf_peer_init(freerdp_peer* client)
{
	client->ContextSize = sizeof(mfPeerContext);
	client->ContextNew = mf_peer_context_new;
	client->ContextFree = mf_peer_context_free;

	if (!freerdp_peer_context_new(client))
		return FALSE;

	info_event_queue = mf_event_queue_new();
	info_queue = dispatch_queue_create("FreeRDP.update.timer", DISPATCH_QUEUE_SERIAL);
	info_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, info_queue);

	if (info_timer)
	{
		// DEBUG_WARN( "created timer\n");
		dispatch_source_set_timer(info_timer, DISPATCH_TIME_NOW, 42ull * NSEC_PER_MSEC,
		                          100ull * NSEC_PER_MSEC);
		dispatch_source_set_event_handler(info_timer, ^{
		  // DEBUG_WARN( "dispatch\n");
		  mfEvent* event = mf_event_new(FREERDP_SERVER_MAC_EVENT_TYPE_FRAME_TICK);
		  mf_event_push(info_event_queue, (mfEvent*)event);
		});
		dispatch_resume(info_timer);
	}

	return TRUE;
}

static BOOL mf_peer_post_connect(freerdp_peer* client)
{
	mfInfo* mfi = mf_info_get_instance();

	WINPR_ASSERT(client);

	mfPeerContext* context = (mfPeerContext*)client->context;
	WINPR_ASSERT(context);

	rdpSettings* settings = client->context->settings;
	WINPR_ASSERT(settings);

	mfi->scale = 1;
	// mfi->servscreen_width = 2880 / mfi->scale;
	// mfi->servscreen_height = 1800 / mfi->scale;
	UINT32 bitsPerPixel = 32;

	if ((settings->DesktopWidth != mfi->servscreen_width) ||
	    (settings->DesktopHeight != mfi->servscreen_height))
	{
	}

	settings->DesktopWidth = mfi->servscreen_width;
	settings->DesktopHeight = mfi->servscreen_height;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, bitsPerPixel))
		return FALSE;

	WINPR_ASSERT(client->context->update);
	WINPR_ASSERT(client->context->update->DesktopResize);
	client->context->update->DesktopResize(client->context);

	mfi->mouse_down_left = FALSE;
	mfi->mouse_down_right = FALSE;
	mfi->mouse_down_other = FALSE;
#ifdef CHANNEL_RDPSND_SERVER

	if (WTSVirtualChannelManagerIsChannelJoined(context->vcm, "rdpsnd"))
	{
		mf_peer_rdpsnd_init(context); /* Audio Output */
	}

#endif
	/* Dynamic Virtual Channels */
#ifdef CHANNEL_AUDIN_SERVER
	mf_peer_audin_init(context); /* Audio Input */
#endif
	return TRUE;
}

static BOOL mf_peer_activate(freerdp_peer* client)
{
	WINPR_ASSERT(client);

	mfPeerContext* context = (mfPeerContext*)client->context;
	WINPR_ASSERT(context);

	rdpSettings* settings = client->context->settings;
	WINPR_ASSERT(settings);

	rfx_context_reset(context->rfx_context, settings->DesktopWidth, settings->DesktopHeight);
	context->activated = TRUE;
	return TRUE;
}

static BOOL mf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	return TRUE;
}

static BOOL mf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	bool state_down = FALSE;

	if (flags == KBD_FLAGS_DOWN)
	{
		state_down = TRUE;
	}
	return TRUE;
}

static BOOL mf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	return FALSE;
}

static BOOL mf_peer_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16* area)
{
	return FALSE;
}

static void* mf_peer_main_loop(void* arg)
{
	mfPeerContext* context;
	rdpSettings* settings;
	rdpInput* input;
	rdpUpdate* update;
	freerdp_peer* client = (freerdp_peer*)arg;

	if (!mf_peer_init(client))
	{
		freerdp_peer_free(client);
		return NULL;
	}

	WINPR_ASSERT(client->context);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	/* Initialize the real server settings here */
	freerdp_settings_set_string(settings, FreeRDP_CertificateFile, "server.crt");
	freerdp_settings_set_string(settings, FreeRDP_PrivateKeyFile, "server.key");

	if (!settings->CertificateFile || !settings->PrivateKeyFile)
	{
		freerdp_peer_free(client);
		return NULL;
	}

	settings->NlaSecurity = FALSE;
	settings->RemoteFxCodec = TRUE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32))
		return FALSE;
	settings->SuppressOutput = TRUE;
	settings->RefreshRect = FALSE;

	client->PostConnect = mf_peer_post_connect;
	client->Activate = mf_peer_activate;

	input = client->context->input;
	WINPR_ASSERT(input);

	input->SynchronizeEvent = mf_peer_synchronize_event;
	input->KeyboardEvent = mf_input_keyboard_event; // mf_peer_keyboard_event;
	input->UnicodeKeyboardEvent = mf_peer_unicode_keyboard_event;
	input->MouseEvent = mf_input_mouse_event;
	input->ExtendedMouseEvent = mf_input_extended_mouse_event;

	update = client->context->update;
	WINPR_ASSERT(update);

	// update->RefreshRect = mf_peer_refresh_rect;
	update->SuppressOutput = mf_peer_suppress_output;

	client->Initialize(client);
	context = (mfPeerContext*)client->context;

	while (1)
	{
		DWORD status;
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
		DWORD count = client->GetEventHandles(client, handles, ARRAYSIZE(handles));

		if ((count == 0) || (count == MAXIMUM_WAIT_OBJECTS))
		{
			WLog_ERR(TAG, "Failed to get FreeRDP file descriptor");
			break;
		}

		handles[count++] = WTSVirtualChannelManagerGetEventHandle(context->vcm);

		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed");
			break;
		}

		if (client->CheckFileDescriptor(client) != TRUE)
		{
			break;
		}

		if ((mf_peer_check_fds(client)) != TRUE)
		{
			break;
		}

		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
		{
			break;
		}
	}

	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return NULL;
}

BOOL mf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	pthread_t th;

	if (pthread_create(&th, 0, mf_peer_main_loop, client) == 0)
	{
		pthread_detach(th);
		return TRUE;
	}

	return FALSE;
}
