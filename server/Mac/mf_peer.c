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
#include <freerdp/utils/sleep.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/utils/stream.h>

#include "mf_peer.h"
#include "mf_info.h"
#include "mf_event.h"

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
    //HGDI_RGN invalid_region;
    
	if (context->activated == FALSE)
		return TRUE;
    
	event = mf_event_peek(info_event_queue);
    
	if (event != NULL)
	{
		if (event->type == MF_EVENT_TYPE_REGION)
		{
            printf("unhandled event\n");
			/*mfEventRegion* region = (mfEventRegion*) mf_event_pop(info_event_queue);
             gdi_InvalidateRegion(xfp->hdc, region->x, region->y, region->width, region->height);
             xf_event_region_free(region);*/
		}
		else if (event->type == MF_EVENT_TYPE_FRAME_TICK)
		{
			event = mf_event_pop(info_event_queue);
            
            printf("Tick\n");
            
            mf_peer_rfx_update(client);
			
            /*invalid_region = xfp->hdc->hwnd->invalid;
             
             if (invalid_region->null == FALSE)
             {
             xf_peer_rfx_update(client, invalid_region->x, invalid_region->y,
             invalid_region->w, invalid_region->h);
             }
             
             invalid_region->null = 1;
             xfp->hdc->hwnd->ninvalid = 0;
             */
            
            
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
    /*
    printf("\tinvalid -> (%d,%d), (%d,%d)\n",
           mfi->invalid.x,
           mfi->invalid.y,
           mfi->invalid.x + mfi->invalid.width,
           mfi->invalid.y + mfi->invalid.height);
            
    int bytewidth;
    
    CGRect invRect;
    
    invRect.origin.x = mfi->invalid.x /2;
    invRect.origin.y = mfi->invalid.y / 2;
    invRect.size.height = mfi->invalid.height / 2;
    invRect.size.width = mfi->invalid.width / 2;
    
    //CGImageRef image = CGDisplayCreateImage(kCGDirectMainDisplay);    // Main screenshot capture call
    CGImageRef image = CGDisplayCreateImageForRect(kCGDirectMainDisplay, invRect);
    //CGSize frameSize = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));    // Get screenshot bounds
    
    if (image == NULL) {
        printf("image = null!!\n\n\n");
    }
    
    CGSize frameSize;
    frameSize.width = 2880 / 2;
    frameSize.height = 1800/ 2;
    
    CFDictionaryRef opts;
    
    long ImageCompatibility;
    long BitmapContextCompatibility;
    
    void * keys[3];
    keys[0] = (void *) kCVPixelBufferCGImageCompatibilityKey;
    keys[1] = (void *) kCVPixelBufferCGBitmapContextCompatibilityKey;
    keys[2] = NULL;
    
    void * values[3];
    values[0] = (void *) &ImageCompatibility;
    values[1] = (void *) &BitmapContextCompatibility;
    values[2] = NULL;
    
    opts = CFDictionaryCreate(kCFAllocatorDefault, (const void **) keys, (const void **) values, 2, NULL, NULL);
    
    if (opts == NULL)
    {
        printf("failed to create dictionary\n");
        //return 1;
    }
    
    CVPixelBufferRef pxbuffer = NULL;
    
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, invRect.size.width,
                                          invRect.size.height,  kCVPixelFormatType_32ARGB, opts,
                                          &pxbuffer);
    
    if (status != kCVReturnSuccess)
    {
        printf("Failed to create pixel buffer! \n");
        //return 1;
    }
    
    CFRelease(opts);
    
    CVPixelBufferLockBaseAddress(pxbuffer, 0);
    void *pxdata = CVPixelBufferGetBaseAddress(pxbuffer);
    
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    
    CGContextRef context = CGBitmapContextCreate(pxdata,
                                                 invRect.size.width,
                                                 invRect.size.height, 8, 4*frameSize.width, rgbColorSpace,
                                                 kCGImageAlphaNoneSkipLast);
    
    if (context == NULL) {
        printf("context = null!!!\n\n\n");
    }
    
    printf("context = [%p], image = [%p]\n%fx%f\n",
           context,
           image,
           invRect.size.width,
           invRect.size.height);
    
    CGContextDrawImage(context,
                       CGRectMake(0, 0, invRect.size.width, invRect.size.height),
                       //CGRectMake(invRect.origin.x, frameSize.height - invRect.origin.y, invRect.size.width, invRect.size.height),
                       //invRect,
                       image);
    
    bytewidth = frameSize.width * 4; // Assume 4 bytes/pixel for now
    bytewidth = (bytewidth + 3) & ~3; // Align to 4 bytes
    */
    
    long width;
    long height;
    int pitch;
    BYTE* dataBits = NULL;
    
    mf_info_getScreenData(mfi, &width, &height, &dataBits, &pitch);
    
    //encode
    
    STREAM* s;
	RFX_RECT rect;
	rdpUpdate* update;
	mfPeerContext* mfp;
	SURFACE_BITS_COMMAND* cmd;
    
	update = client->update;
	mfp = (mfPeerContext*) client->context;
	cmd = &update->surface_bits_command;
    
    
    s = mfp->s;
    stream_clear(s);
	stream_set_pos(s, 0);
    
    
    rect.x = 0;
    rect.y = 0;
    rect.width = width;
    rect.height = height;
    
    rfx_compose_message(mfp->rfx_context, s, &rect, 1,
                        (BYTE*) dataBits, rect.width, rect.height, pitch);
    
    UINT32 x = mfi->invalid.x / 2;
    UINT32 y = mfi->invalid.y / 2;
    
    cmd->destLeft = x;
    cmd->destTop = y;
    cmd->destRight = x + rect.width;
    cmd->destBottom = y + rect.height;
    
    
	cmd->bpp = 32;
	cmd->codecID = 3;
	cmd->width = rect.width;
	cmd->height = rect.height;
	cmd->bitmapDataLength = stream_get_length(s);
	cmd->bitmapData = stream_get_head(s);
    
    //send
    printf("send\n");
    
	update->SurfaceBits(update->context, cmd);
    
    //clean up
    
    mf_info_clear_invalid_region(mfi);
    // note: need to stop getting new dirty rects until here
    /*
    CGColorSpaceRelease(rgbColorSpace);
    CGImageRelease(image);
    CGContextRelease(context);
    
    CVPixelBufferUnlockBaseAddress(pxbuffer, 0);
    CVPixelBufferRelease(pxbuffer);
    */
}

/* Called when we have a new peer connecting */
void mf_peer_context_new(freerdp_peer* client, mfPeerContext* context)
{
    context->info = mf_info_get_instance();
	context->rfx_context = rfx_context_new();
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = client->settings->DesktopWidth;
	context->rfx_context->height = client->settings->DesktopHeight;
	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_R8G8B8A8);
    
	context->nsc_context = nsc_context_new();
	nsc_context_set_pixel_format(context->nsc_context, RDP_PIXEL_FORMAT_R8G8B8A8);
    
	context->s = stream_new(0xFFFF);
    
#ifdef WITH_SERVER_CHANNELS
	context->vcm = WTSCreateVirtualChannelManager(client);
#endif
    
    mf_info_peer_register(context->info, context);
}


/* Called after a peer disconnects */
void mf_peer_context_free(freerdp_peer* client, mfPeerContext* context)
{
	if (context)
	{
        mf_info_peer_unregister(context->info, context);
        
        dispatch_suspend(info_timer);
        
		stream_free(context->s);
        
		rfx_context_free(context->rfx_context);
		nsc_context_free(context->nsc_context);
        
#ifdef CHANNEL_AUDIN_SERVER
		if (context->audin)
			audin_server_context_free(context->audin);
#endif
        
#ifdef CHANNEL_RDPSND_SERVER
		if (context->rdpsnd)
			rdpsnd_server_context_free(context->rdpsnd);
#endif
        
#ifdef WITH_SERVER_CHANNELS
		WTSDestroyVirtualChannelManager(context->vcm);
#endif
	}
}

/* Called when a new client connects */
void mf_peer_init(freerdp_peer* client)
{
	client->context_size = sizeof(mfPeerContext);
	client->ContextNew = (psPeerContextNew) mf_peer_context_new;
	client->ContextFree = (psPeerContextFree) mf_peer_context_free;
	freerdp_peer_context_new(client);
    
    info_event_queue = mf_event_queue_new();
    
    info_queue = dispatch_queue_create("testing.101", DISPATCH_QUEUE_SERIAL);
    info_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, info_queue);
    
    if(info_timer)
    {
        //printf("created timer\n");
        dispatch_source_set_timer(info_timer, DISPATCH_TIME_NOW, 500ull * NSEC_PER_MSEC, 100ull * NSEC_PER_MSEC);
        dispatch_source_set_event_handler(info_timer, ^{
            printf("dispatch\n");
            mfEvent* event = mf_event_new(MF_EVENT_TYPE_FRAME_TICK);
            mf_event_push(info_event_queue, (mfEvent*) event);}
                                          );
        dispatch_resume(info_timer);
    }
}


BOOL mf_peer_post_connect(freerdp_peer* client)
{
	//mfPeerContext* context = (mfPeerContext*) client->context;
    rdpSettings* settings = client->settings;
    
	printf("Client %s is activated\n", client->hostname);
    
	if (client->settings->AutoLogonEnabled)
	{
		printf(" and wants to login automatically as %s\\%s",
               client->settings->Domain ? client->settings->Domain : "",
               client->settings->Username);
        
		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	printf("\n");
    
    
    UINT32 servscreen_width = 2880 / 2;
    UINT32 servscreen_height = 1800 / 2;
    UINT32 bitsPerPixel = 32;
    
    if ((settings->DesktopWidth != servscreen_width) || (settings->DesktopHeight != servscreen_height))
	{
		printf("Client requested resolution %dx%d, but will resize to %dx%d\n",
               settings->DesktopWidth, settings->DesktopHeight, servscreen_width, servscreen_height);
    }
    
    settings->DesktopWidth = servscreen_width;
    settings->DesktopHeight = servscreen_height;
    settings->ColorDepth = bitsPerPixel;
    
    client->update->DesktopResize(client->update->context);
	
    
	/*printf("Client requested desktop: %dx%dx%d\n",
     client->settings->DesktopWidth, client->settings->DesktopHeight, client->settings->ColorDepth);
     */
#ifdef WITH_SERVER_CHANNELS
	/* Iterate all channel names requested by the client and activate those supported by the server */
    int i;
	for (i = 0; i < client->settings->ChannelCount; i++)
	{
		if (client->settings->ChannelDefArray[i].joined)
		{
#ifdef CHANNEL_RDPSND_SERVER
			if (strncmp(client->settings->ChannelDefArray[i].Name, "rdpsnd", 6) == 0)
			{
				mf_peer_rdpsnd_init(context); /* Audio Output */
			}
#endif
		}
	}
    
	/* Dynamic Virtual Channels */
#endif
    
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
	printf("PeerLogon\n");

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
	printf("Client sent a synchronize event (flags:0x%08X)\n", flags);
}

void mf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	printf("Client sent a keyboard event (flags:0x%04X code:0x%04X)\n", flags, code);
    
    UINT16 down = 0x4000;
    //UINT16 up = 0x8000;
    
    bool state_down = FALSE;
    
    if (flags == down)
    {
        state_down = TRUE;
    }
    
    CGEventRef event;
    event = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)code, state_down);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

void mf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	printf("Client sent a unicode keyboard event (flags:0x%04X code:0x%04X)\n", flags, code);
}

void mf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//printf("Client sent a mouse event (flags:0x%04X pos: %d,%d)\n", flags, x, y);
}

void mf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//printf("Client sent an extended mouse event (flags:0x%04X pos: %d,%d)\n", flags, x, y);
}

static void mf_peer_refresh_rect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	BYTE i;
    
	printf("Client requested to refresh:\n");
    
	for (i = 0; i < count; i++)
	{
		printf("  (%d, %d) (%d, %d)\n", areas[i].left, areas[i].top, areas[i].right, areas[i].bottom);
	}
}

static void mf_peer_suppress_output(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	if (allow > 0)
	{
		printf("Client restore output (%d, %d) (%d, %d).\n", area->left, area->top, area->right, area->bottom);
	}
	else
	{
		printf("Client minimized and suppress output.\n");
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

	printf("PeerSocketListener\n");

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			printf("Failed to get peer file descriptor\n");
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

	printf("Exiting Peer Socket Listener Thread\n");

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
	client->settings->CertificateFile = "server.crt";
	client->settings->PrivateKeyFile = "server.key";
	client->settings->NlaSecurity = FALSE;
	client->settings->RemoteFxCodec = TRUE;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = TRUE;
    
	client->PostConnect = mf_peer_post_connect;
	client->Activate = mf_peer_activate;
    
	client->input->SynchronizeEvent = mf_peer_synchronize_event;
	client->input->KeyboardEvent = mf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = mf_peer_unicode_keyboard_event;
	client->input->MouseEvent = mf_peer_mouse_event;
	client->input->ExtendedMouseEvent = mf_peer_extended_mouse_event;
    
	client->update->RefreshRect = mf_peer_refresh_rect;
	client->update->SuppressOutput = mf_peer_suppress_output;
    
	client->Initialize(client);
	context = (mfPeerContext*) client->context;
    
	printf("We've got a client %s\n", client->local ? "(local)" : client->hostname);
    
	while (1)
	{
		rcount = 0;
        
		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
        if (mf_peer_get_fds(client, rfds, &rcount) != TRUE)
		{
			printf("Failed to get mfreerdp file descriptor\n");
			break;
		}
        
#ifdef WITH_SERVER_CHANNELS
		WTSVirtualChannelManagerGetFileDescriptor(context->vcm, rfds, &rcount);
#endif
        
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
				printf("select failed\n");
				break;
			}
		}
        
        if (client->CheckFileDescriptor(client) != TRUE)
		{
			printf("Failed to check freerdp file descriptor\n");
			break;
		}
		if ((mf_peer_check_fds(client)) != TRUE)
		{
			printf("Failed to check mfreerdp file descriptor\n");
			break;
		}
        
        
#ifdef WITH_SERVER_CHANNELS
		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;
#endif
        
	}
    
	printf("Client %s disconnected.\n", client->local ? "(local)" : client->hostname);
    
	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
    
	return NULL;
}