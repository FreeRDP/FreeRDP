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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/listener.h>
#include <freerdp/codec/rfx.h>
#include <winpr/stream.h>
#include <freerdp/peer.h>
#include <freerdp/codec/color.h>

#include <winpr/crt.h>

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

//refactor these
static int info_last_sec = 0;
static int info_last_nsec = 0;

static dispatch_source_t info_timer;
static dispatch_queue_t info_queue;

static mfEventQueue* info_event_queue;

static CGLContextObj glContext;
static CGContextRef bmp;
static CGImageRef img;

static BOOL mf_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	if (info_event_queue->pipe_fd[0] == -1)
		return TRUE;

	rfds[*rcount] = (void*)(long) info_event_queue->pipe_fd[0];
	(*rcount)++;
	return TRUE;
}


static void mf_peer_rfx_update(freerdp_peer* client)
{
	//check
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
	//encode
	wStream* s;
	RFX_RECT rect;
	rdpUpdate* update;
	mfPeerContext* mfp;
	SURFACE_BITS_COMMAND* cmd;
	update = client->update;
	mfp = (mfPeerContext*) client->context;
	cmd = &update->surface_bits_command;
	s = mfp->s;
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

	if (!(rfx_compose_message(mfp->rfx_context, s, &rect, 1,
	                          (BYTE*) dataBits, rect.width, rect.height, pitch)))
	{
		return;
	}

	memset(cmd, 0, sizeof(SURFACE_BITS_COMMAND));
	cmd->destLeft = x;
	cmd->destTop = y;
	cmd->destRight = x + rect.width;
	cmd->destBottom = y + rect.height;
	cmd->bmp.bpp = 32;
	cmd->bmp.codecID = 3;
	cmd->bmp.width = rect.width;
	cmd->bmp.height = rect.height;
	cmd->bmp.bitmapDataLength = Stream_GetPosition(s);
	cmd->bmp.bitmapData = Stream_Buffer(s);
	//send
	update->SurfaceBits(update->context, cmd);
	//clean up... maybe?
}

static BOOL mf_peer_check_fds(freerdp_peer* client)
{
	mfPeerContext* context = (mfPeerContext*) client->context;
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
static BOOL mf_peer_context_new(freerdp_peer* client, mfPeerContext* context)
{
	if (!(context->info = mf_info_get_instance()))
		return FALSE;

	if (!(context->rfx_context = rfx_context_new(TRUE)))
		goto fail_rfx_context;

	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = client->settings->DesktopWidth;
	context->rfx_context->height = client->settings->DesktopHeight;
	rfx_context_set_pixel_format(context->rfx_context, PIXEL_FORMAT_BGRA32);

	if (!(context->s = Stream_New(NULL, 0xFFFF)))
		goto fail_stream_new;

	context->vcm = WTSOpenServerA((LPSTR) client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto fail_open_server;

	mf_info_peer_register(context->info, context);
	return TRUE;
fail_open_server:
	Stream_Free(context->s, TRUE);
	context->s = NULL;
fail_stream_new:
	rfx_context_free(context->rfx_context);
	context->rfx_context = NULL;
fail_rfx_context:
	return FALSE;
}

/* Called after a peer disconnects */
static void mf_peer_context_free(freerdp_peer* client, mfPeerContext* context)
{
	if (context)
	{
		mf_info_peer_unregister(context->info, context);
		dispatch_suspend(info_timer);
		Stream_Free(context->s, TRUE);
		rfx_context_free(context->rfx_context);
		//nsc_context_free(context->nsc_context);
#ifdef CHANNEL_AUDIN_SERVER

		if (context->audin)
			audin_server_context_free(context->audin);

#endif
#ifdef CHANNEL_RDPSND_SERVER
		mf_peer_rdpsnd_stop();

		if (context->rdpsnd)
			rdpsnd_server_context_free(context->rdpsnd);

#endif
		WTSCloseServer(context->vcm);
	}
}

/* Called when a new client connects */
static BOOL mf_peer_init(freerdp_peer* client)
{
	client->ContextSize = sizeof(mfPeerContext);
	client->ContextNew = (psPeerContextNew) mf_peer_context_new;
	client->ContextFree = (psPeerContextFree) mf_peer_context_free;

	if (!freerdp_peer_context_new(client))
		return FALSE;

	info_event_queue = mf_event_queue_new();
	info_queue = dispatch_queue_create("FreeRDP.update.timer",
	                                   DISPATCH_QUEUE_SERIAL);
	info_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
	                                    info_queue);

	if (info_timer)
	{
		//DEBUG_WARN( "created timer\n");
		dispatch_source_set_timer(info_timer, DISPATCH_TIME_NOW, 42ull * NSEC_PER_MSEC,
		                          100ull * NSEC_PER_MSEC);
		dispatch_source_set_event_handler(info_timer, ^
		{
			//DEBUG_WARN( "dispatch\n");
			mfEvent* event = mf_event_new(FREERDP_SERVER_MAC_EVENT_TYPE_FRAME_TICK);
			mf_event_push(info_event_queue, (mfEvent*) event);
		}
		                                 );
		dispatch_resume(info_timer);
	}

	return TRUE;
}

static BOOL mf_peer_post_connect(freerdp_peer* client)
{
	mfPeerContext* context = (mfPeerContext*) client->context;
	rdpSettings* settings = client->settings;
	mfInfo* mfi = mf_info_get_instance();
	mfi->scale = 1;
	//mfi->servscreen_width = 2880 / mfi->scale;
	//mfi->servscreen_height = 1800 / mfi->scale;
	UINT32 bitsPerPixel = 32;

	if ((settings->DesktopWidth != mfi->servscreen_width)
	    || (settings->DesktopHeight != mfi->servscreen_height))
	{
	}

	settings->DesktopWidth = mfi->servscreen_width;
	settings->DesktopHeight = mfi->servscreen_height;
	settings->ColorDepth = bitsPerPixel;
	client->update->DesktopResize(client->update->context);
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
	mfPeerContext* context = (mfPeerContext*) client->context;
	rfx_context_reset(context->rfx_context, client->settings->DesktopWidth,
	                  client->settings->DesktopHeight);
	context->activated = TRUE;
	return TRUE;
}

static BOOL mf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	return TRUE;
}

void mf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	UINT16 down = 0x4000;
	//UINT16 up = 0x8000;
	bool state_down = FALSE;

	if (flags == down)
	{
		state_down = TRUE;
	}
}

static BOOL mf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	return FALSE;
}

static BOOL mf_peer_suppress_output(rdpContext* context, BYTE allow,
                                    const RECTANGLE_16* area)
{
	return FALSE;
}


static void* mf_peer_main_loop(void* arg)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;
	mfPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) arg;
	memset(rfds, 0, sizeof(rfds));

	if (!mf_peer_init(client))
	{
		freerdp_peer_free(client);
		return NULL;
	}

	/* Initialize the real server settings here */
	client->settings->CertificateFile = _strdup("server.crt");
	client->settings->PrivateKeyFile = _strdup("server.key");

	if (!client->settings->CertificateFile || !client->settings->PrivateKeyFile)
	{
		freerdp_peer_free(client);
		return NULL;
	}

	client->settings->NlaSecurity = FALSE;
	client->settings->RemoteFxCodec = TRUE;
	client->settings->ColorDepth = 32;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = FALSE;
	client->PostConnect = mf_peer_post_connect;
	client->Activate = mf_peer_activate;
	client->input->SynchronizeEvent = mf_peer_synchronize_event;
	client->input->KeyboardEvent = mf_input_keyboard_event;//mf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = mf_peer_unicode_keyboard_event;
	client->input->MouseEvent = mf_input_mouse_event;
	client->input->ExtendedMouseEvent = mf_input_extended_mouse_event;
	//client->update->RefreshRect = mf_peer_refresh_rect;
	client->update->SuppressOutput = mf_peer_suppress_output;
	client->Initialize(client);
	context = (mfPeerContext*) client->context;

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			break;
		}

		if (mf_peer_get_fds(client, rfds, &rcount) != TRUE)
		{
			break;
		}

		WTSVirtualChannelManagerGetFileDescriptor(context->vcm, rfds, &rcount);
		max_fds = 0;
		FD_ZERO(&rfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		if (select(max_fds + 1, &rfds_set, NULL, NULL, NULL) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
			      (errno == EWOULDBLOCK) ||
			      (errno == EINPROGRESS) ||
			      (errno == EINTR))) /* signal occurred */
			{
				break;
			}
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
