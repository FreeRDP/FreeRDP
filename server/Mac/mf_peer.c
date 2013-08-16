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

#include <winpr/crt.h>

#include "mf_peer.h"
#include "mf_info.h"
#include "mf_input.h"
#include "mf_event.h"
#include "mf_rdpsnd.h"

#include <mach/clock.h>
#include <mach/mach.h>
#include <dispatch/dispatch.h>

#include "OpenGL/OpenGL.h"
#include "OpenGL/gl.h"

#include "CoreVideo/CoreVideo.h"

//refactor these
int info_last_sec = 0;
int info_last_nsec = 0;

dispatch_source_t info_timer;
dispatch_queue_t info_queue;

mfEventQueue* info_event_queue;


CGLContextObj glContext;
CGContextRef bmp;
CGImageRef img;



BOOL mf_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	if (info_event_queue->pipe_fd[0] == -1)
		return TRUE;
	
	rfds[*rcount] = (void *)(long) info_event_queue->pipe_fd[0];
	(*rcount)++;
	
	return TRUE;
}

BOOL mf_peer_check_fds(freerdp_peer* client)
{
	mfPeerContext* context = (mfPeerContext*) client->context;
	mfEvent* event;
	
	if (context->activated == FALSE)
		return TRUE;
	
	event = mf_event_peek(info_event_queue);
	
	if (event != NULL)
	{
		if (event->type == MF_EVENT_TYPE_REGION)
		{
			fprintf(stderr, "unhandled event\n");
		}
		else if (event->type == MF_EVENT_TYPE_FRAME_TICK)
		{
			event = mf_event_pop(info_event_queue);
                        
			mf_peer_rfx_update(client);
			
			mf_event_free(event);
		}
	}
	
	return TRUE;
}

void mf_peer_rfx_update(freerdp_peer* client)
{
	//check
	mfInfo* mfi = mf_info_get_instance();
	
	mf_info_find_invalid_region(mfi);
	
	if (mf_info_have_invalid_region(mfi) == false) {
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
	
	rfx_compose_message(mfp->rfx_context, s, &rect, 1,
			    (BYTE*) dataBits, rect.width, rect.height, pitch);
	
	cmd->destLeft = x;
	cmd->destTop = y;
	cmd->destRight = x + rect.width;
	cmd->destBottom = y + rect.height;
	
	
	cmd->bpp = 32;
	cmd->codecID = 3;
	cmd->width = rect.width;
	cmd->height = rect.height;
	cmd->bitmapDataLength = Stream_GetPosition(s);
	cmd->bitmapData = Stream_Buffer(s);
	
	//send
	
	update->SurfaceBits(update->context, cmd);
	
	//clean up... maybe?
	
}

/* Called when we have a new peer connecting */
int mf_peer_context_new(freerdp_peer* client, mfPeerContext* context)
{
	context->info = mf_info_get_instance();
	context->rfx_context = rfx_context_new(TRUE);
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = client->settings->DesktopWidth;
	context->rfx_context->height = client->settings->DesktopHeight;
	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);
	
	//context->nsc_context = nsc_context_new();
	//nsc_context_set_pixel_format(context->nsc_context, RDP_PIXEL_FORMAT_B8G8R8A8);
	
	context->s = Stream_New(NULL, 0xFFFF);
	
	//#ifdef WITH_SERVER_CHANNELS
	context->vcm = WTSCreateVirtualChannelManager(client);
	//#endif
	
	mf_info_peer_register(context->info, context);

	return 0;
}


/* Called after a peer disconnects */
void mf_peer_context_free(freerdp_peer* client, mfPeerContext* context)
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
		
		//#ifdef CHANNEL_RDPSND_SERVER
		mf_peer_rdpsnd_stop();
		if (context->rdpsnd)
			rdpsnd_server_context_free(context->rdpsnd);
		//#endif
		
		//#ifdef WITH_SERVER_CHANNELS
		WTSDestroyVirtualChannelManager(context->vcm);
		//#endif
	}
}

/* Called when a new client connects */
void mf_peer_init(freerdp_peer* client)
{
	client->ContextSize = sizeof(mfPeerContext);
	client->ContextNew = (psPeerContextNew) mf_peer_context_new;
	client->ContextFree = (psPeerContextFree) mf_peer_context_free;
	freerdp_peer_context_new(client);
	
	info_event_queue = mf_event_queue_new();
	
	info_queue = dispatch_queue_create("FreeRDP.update.timer", DISPATCH_QUEUE_SERIAL);
	info_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, info_queue);
	
	if(info_timer)
	{
		//fprintf(stderr, "created timer\n");
		dispatch_source_set_timer(info_timer, DISPATCH_TIME_NOW, 42ull * NSEC_PER_MSEC, 100ull * NSEC_PER_MSEC);
		dispatch_source_set_event_handler(info_timer, ^{
			//fprintf(stderr, "dispatch\n");
			mfEvent* event = mf_event_new(MF_EVENT_TYPE_FRAME_TICK);
			mf_event_push(info_event_queue, (mfEvent*) event);}
						  );
		dispatch_resume(info_timer);
	}
}


BOOL mf_peer_post_connect(freerdp_peer* client)
{
	mfPeerContext* context = (mfPeerContext*) client->context;
	rdpSettings* settings = client->settings;
	
	fprintf(stderr, "Client %s post connect\n", client->hostname);
	
	if (client->settings->AutoLogonEnabled)
	{
		fprintf(stderr, " and wants to login automatically as %s\\%s",
		       client->settings->Domain ? client->settings->Domain : "",
		       client->settings->Username);
		
		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	fprintf(stderr, "\n");
	
	mfInfo* mfi = mf_info_get_instance();
	mfi->scale = 1;
	
	//mfi->servscreen_width = 2880 / mfi->scale;
	//mfi->servscreen_height = 1800 / mfi->scale;
	UINT32 bitsPerPixel = 32;
	
	if ((settings->DesktopWidth != mfi->servscreen_width) || (settings->DesktopHeight != mfi->servscreen_height))
	{
		fprintf(stderr, "Client requested resolution %dx%d, but will resize to %dx%d\n",
		       settings->DesktopWidth, settings->DesktopHeight, mfi->servscreen_width, mfi->servscreen_height);
	}
	
	settings->DesktopWidth = mfi->servscreen_width;
	settings->DesktopHeight = mfi->servscreen_height;
	settings->ColorDepth = bitsPerPixel;
	
	client->update->DesktopResize(client->update->context);
	
	mfi->mouse_down_left = FALSE;
	mfi->mouse_down_right = FALSE;
	mfi->mouse_down_other = FALSE;

	
	//#ifdef WITH_SERVER_CHANNELS
	/* Iterate all channel names requested by the client and activate those supported by the server */
	int i;
	for (i = 0; i < client->settings->ChannelCount; i++)
	{
		if (client->settings->ChannelDefArray[i].joined)
		{
			//#ifdef CHANNEL_RDPSND_SERVER
			if (strncmp(client->settings->ChannelDefArray[i].Name, "rdpsnd", 6) == 0)
			{
				mf_peer_rdpsnd_init(context); /* Audio Output */
			}
			//#endif
		}
	}
	
	/* Dynamic Virtual Channels */
	//#endif
	
#ifdef CHANNEL_AUDIN_SERVER
	mf_peer_audin_init(context); /* Audio Input */
#endif
	
	return TRUE;
}


BOOL mf_peer_activate(freerdp_peer* client)
{
	mfPeerContext* context = (mfPeerContext*) client->context;
	
	rfx_context_reset(context->rfx_context);
	context->activated = TRUE;
	
	return TRUE;
}

/*BOOL wf_peer_logon(freerdp_peer* client, SEC_WINNT_AUTH_IDENTITY* identity, BOOL automatic)
 {
 fprintf(stderr, "PeerLogon\n");
 
 if (automatic)
 {
 _tprintf(_T("Logon: User:%s Domain:%s Password:%s\n"),
 identity->User, identity->Domain, identity->Password);
 }
 
 
 wfreerdp_server_peer_callback_event(((rdpContext*) client->context)->peer->pId, WF_SRV_CALLBACK_EVENT_AUTH);
 return TRUE;
 }*/

void mf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	fprintf(stderr, "Client sent a synchronize event (flags:0x%08X)\n", flags);
}

void mf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	fprintf(stderr, "Client sent a keyboard event (flags:0x%04X code:0x%04X)\n", flags, code);
	
	UINT16 down = 0x4000;
	//UINT16 up = 0x8000;
	
	bool state_down = FALSE;
	
	if (flags == down)
	{
		state_down = TRUE;
	}
	
	/*
	 CGEventRef event;
	 event = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)code, state_down);
	 CGEventPost(kCGHIDEventTap, event);
	 CFRelease(event);
	 */
}

void mf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	fprintf(stderr, "Client sent a unicode keyboard event (flags:0x%04X code:0x%04X)\n", flags, code);
}

/*void mf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//fprintf(stderr, "Client sent a mouse event (flags:0x%04X pos: %d,%d)\n", flags, x, y);
}

void mf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//fprintf(stderr, "Client sent an extended mouse event (flags:0x%04X pos: %d,%d)\n", flags, x, y);
}
*/
/*static void mf_peer_refresh_rect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
 {
 BYTE i;
 
 fprintf(stderr, "Client requested to refresh:\n");
 
 for (i = 0; i < count; i++)
 {
 fprintf(stderr, "  (%d, %d) (%d, %d)\n", areas[i].left, areas[i].top, areas[i].right, areas[i].bottom);
 }
 }*/

static void mf_peer_suppress_output(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	if (allow > 0)
	{
		fprintf(stderr, "Client restore output (%d, %d) (%d, %d).\n", area->left, area->top, area->right, area->bottom);
	}
	else
	{
		fprintf(stderr, "Client minimized and suppress output.\n");
	}
}

void mf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	pthread_t th;
	
	pthread_create(&th, 0, mf_peer_main_loop, client);
	pthread_detach(th);
}

/*DWORD WINAPI wf_peer_socket_listener(LPVOID lpParam)
 {
 int i, fds;
 int rcount;
 int max_fds;
 void* rfds[32];
 fd_set rfds_set;
 wfPeerContext* context;
 freerdp_peer* client = (freerdp_peer*) lpParam;
 
 ZeroMemory(rfds, sizeof(rfds));
 context = (wfPeerContext*) client->context;
 
 fprintf(stderr, "PeerSocketListener\n");
 
 while (1)
 {
 rcount = 0;
 
 if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
 {
 fprintf(stderr, "Failed to get peer file descriptor\n");
 break;
 }
 
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
 
 select(max_fds + 1, &rfds_set, NULL, NULL, NULL);
 
 SetEvent(context->socketEvent);
 WaitForSingleObject(context->socketSemaphore, INFINITE);
 
 if (context->socketClose)
 break;
 }
 
 fprintf(stderr, "Exiting Peer Socket Listener Thread\n");
 
 return 0;
 }
 
 void wf_peer_read_settings(freerdp_peer* client)
 {
 if (!wf_settings_read_string_ascii(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), _T("CertificateFile"), &(client->settings->CertificateFile)))
 client->settings->CertificateFile = _strdup("server.crt");
 
 if (!wf_settings_read_string_ascii(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), _T("PrivateKeyFile"), &(client->settings->PrivateKeyFile)))
 client->settings->PrivateKeyFile = _strdup("server.key");
 }*/

void* mf_peer_main_loop(void* arg)
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
	
	mf_peer_init(client);
	
	/* Initialize the real server settings here */
	client->settings->CertificateFile = _strdup("server.crt");
	client->settings->PrivateKeyFile = _strdup("server.key");
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
	
	fprintf(stderr, "We've got a client %s\n", client->local ? "(local)" : client->hostname);
	
	while (1)
	{
		rcount = 0;
		
		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (mf_peer_get_fds(client, rfds, &rcount) != TRUE)
		{
			fprintf(stderr, "Failed to get mfreerdp file descriptor\n");
			break;
		}
		
		//#ifdef WITH_SERVER_CHANNELS
		WTSVirtualChannelManagerGetFileDescriptor(context->vcm, rfds, &rcount);
		//#endif
		
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
				fprintf(stderr, "select failed\n");
				break;
			}
		}
		
		if (client->CheckFileDescriptor(client) != TRUE)
		{
			fprintf(stderr, "Failed to check freerdp file descriptor\n");
			break;
		}
		if ((mf_peer_check_fds(client)) != TRUE)
		{
			fprintf(stderr, "Failed to check mfreerdp file descriptor\n");
			break;
		}
		
		
		//#ifdef WITH_SERVER_CHANNELS
		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;
		//#endif
		
	}
	
	fprintf(stderr, "Client %s disconnected.\n", client->local ? "(local)" : client->hostname);
	
	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	
	return NULL;
}
