/*
 RDP run-loop
 
 Copyright 2013 Thincast Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <freerdp/gdi/gdi.h>
#import <freerdp/channels/channels.h>
#import <freerdp/client/channels.h>
#import <freerdp/client/cmdline.h>
#import <freerdp/freerdp.h>

#import "ios_freerdp.h"
#import "ios_freerdp_ui.h"
#import "ios_freerdp_events.h"

#import "RDPSession.h"
#import "Utils.h"

#pragma mark Connection helpers

static BOOL ios_pre_connect(freerdp* instance)
{	
	rdpSettings* settings = instance->settings;	

	settings->AutoLogonEnabled = settings->Password && (strlen(settings->Password) > 0);
	
	// Verify screen width/height are sane
	if ((settings->DesktopWidth < 64) || (settings->DesktopHeight < 64) || (settings->DesktopWidth > 4096) || (settings->DesktopHeight > 4096))
	{
		NSLog(@"%s: invalid dimensions %d %d", __func__, settings->DesktopWidth, settings->DesktopHeight);
		return FALSE;
	}
	
	BOOL bitmap_cache = settings->BitmapCacheEnabled;
	
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
	settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;
	
	settings->FrameAcknowledge = 10;

	if (!freerdp_client_load_addins(instance->context->channels, instance->settings))
		return FALSE;

	if (freerdp_channels_pre_connect(instance->context->channels, instance) != CHANNEL_RC_OK)
		return FALSE;

	return TRUE;
}

static BOOL ios_post_connect(freerdp* instance)
{
	mfInfo* mfi = MFI_FROM_INSTANCE(instance);

	instance->context->cache = cache_new(instance->settings);
    
	// Graphics callbacks
	ios_allocate_display_buffer(mfi);
	instance->update->BeginPaint = ios_ui_begin_paint;
	instance->update->EndPaint = ios_ui_end_paint;
	instance->update->DesktopResize = ios_ui_resize_window;
		
	// Channel allocation
	if (freerdp_channels_post_connect(instance->context->channels, instance) != CHANNEL_RC_OK)
		return FALSE;

	[mfi->session performSelectorOnMainThread:@selector(sessionDidConnect) withObject:nil waitUntilDone:YES];
	return TRUE;
}

#pragma mark -
#pragma mark Running the connection

int ios_run_freerdp(freerdp* instance)
{
	mfContext* context = (mfContext*)instance->context;
	mfInfo* mfi = context->mfi;
	rdpChannels* channels = instance->context->channels;
		
	mfi->connection_state = TSXConnectionConnecting;
	
	if (!freerdp_connect(instance))
	{
		NSLog(@"%s: inst->rdp_connect failed", __func__);
		return mfi->unwanted ? MF_EXIT_CONN_CANCELED : MF_EXIT_CONN_FAILED;
	}
	
	if (mfi->unwanted)
		return MF_EXIT_CONN_CANCELED;

	mfi->connection_state = TSXConnectionConnected;
			
	// Connection main loop
	NSAutoreleasePool* pool;
	int i;
	int fds;
	int max_fds;
	int rcount;
	int wcount;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	struct timeval timeout;
	int select_status;
	
	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	while (!freerdp_shall_disconnect(instance))
	{
		rcount = wcount = 0;
		
		pool = [[NSAutoreleasePool alloc] init];

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			NSLog(@"%s: inst->rdp_get_fds failed", __func__);
			break;
		}

		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			NSLog(@"%s: freerdp_chanman_get_fds failed", __func__);
			break;
		}

		if (ios_events_get_fds(mfi, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			NSLog(@"%s: ios_events_get_fds", __func__);
			break;
		}
		
		max_fds = 0;
		FD_ZERO(&rfds_set);
		FD_ZERO(&wfds_set);
		
		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);
			
			if (fds > max_fds)
				max_fds = fds;
			
			FD_SET(fds, &rfds_set);
		}
        
		if (max_fds == 0)
			break;
	
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        select_status = select(max_fds + 1, &rfds_set, NULL, NULL, &timeout);
        
        // timeout?
        if (select_status == 0)
	{
            continue;
	}
        else if (select_status == -1)
	{
		/* these are not really errors */
		if (!((errno == EAGAIN) ||
			(errno == EWOULDBLOCK) ||
			(errno == EINPROGRESS) ||
			(errno == EINTR))) /* signal occurred */
		{
				NSLog(@"%s: select failed!", __func__);
				break;
			}
		}
		
		// Check the libfreerdp fds
		if (freerdp_check_fds(instance) != true)
		{
			NSLog(@"%s: inst->rdp_check_fds failed.", __func__);
			break;
		}
		
		// Check input event fds
		if (ios_events_check_fds(mfi, &rfds_set) != TRUE)
		{
			// This event will fail when the app asks for a disconnect.
			//NSLog(@"%s: ios_events_check_fds failed: terminating connection.", __func__);
			break;
		}
		
		// Check channel fds
		if (freerdp_channels_check_fds(channels, instance) != TRUE)
		{
			NSLog(@"%s: freerdp_chanman_check_fds failed", __func__);
			break;
		}

		[pool release]; pool = nil;
	}	

	CGContextRelease(mfi->bitmap_context);
	mfi->bitmap_context = NULL;	
	mfi->connection_state = TSXConnectionDisconnected;
	
	// Cleanup
	freerdp_channels_disconnect(channels, instance);
	freerdp_disconnect(instance);
	gdi_free(instance);
	cache_free(instance->context->cache);
	
	[pool release]; pool = nil;
	return MF_EXIT_SUCCESS;
}

#pragma mark -
#pragma mark Context callbacks

BOOL ios_context_new(freerdp* instance, rdpContext* context)
{
	mfInfo* mfi;

	if (!(mfi = (mfInfo*)calloc(1, sizeof(mfInfo))))
		goto fail_mfi;

	if (!(context->channels = freerdp_channels_new()))
		goto fail_channels;
	
	if (!ios_events_create_pipe(mfi))
		goto fail_events;

	((mfContext*) context)->mfi = mfi;
	mfi->_context = context;
	mfi->context = (mfContext*)context;
	mfi->context->settings = instance->settings;
	mfi->instance = instance;
	return TRUE;

fail_events:
	freerdp_channels_free(context->channels);
	context->channels = NULL;
fail_channels:
	free(mfi);
fail_mfi:
	return FALSE;
}

void ios_context_free(freerdp* instance, rdpContext* context)
{
	mfInfo* mfi = ((mfContext*) context)->mfi;
	freerdp_channels_close(context->channels, instance);
	freerdp_channels_free(context->channels);
	ios_events_free_pipe(mfi);
	free(mfi);
}

#pragma mark -
#pragma mark Initialization and cleanup

freerdp* ios_freerdp_new()
{
	freerdp* inst = freerdp_new();
	if (!inst)
		return NULL;
	
	inst->PreConnect = ios_pre_connect;
	inst->PostConnect = ios_post_connect;
	inst->Authenticate = ios_ui_authenticate;
	inst->VerifyCertificate = ios_ui_check_certificate;
	inst->VerifyChangedCertificate = ios_ui_check_changed_certificate;
    
	inst->ContextSize = sizeof(mfContext);
	inst->ContextNew = ios_context_new;
	inst->ContextFree = ios_context_free;
	freerdp_context_new(inst);
    
	// determine new home path
	NSString* home_path = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) lastObject];
	free(inst->settings->HomePath);
	free(inst->settings->ConfigPath);
	inst->settings->HomePath = strdup([home_path UTF8String]);
	inst->settings->ConfigPath = strdup([[home_path stringByAppendingPathComponent:@".freerdp"] UTF8String]);
	if (!inst->settings->HomePath || !inst->settings->ConfigPath)
	{
		free(inst->settings->HomePath);
		free(inst->settings->ConfigPath);
		freerdp_context_free(inst);
		freerdp_free(inst);
		return NULL;
	}

	return inst;
}

void ios_freerdp_free(freerdp* instance)
{
	freerdp_context_free(instance);
	freerdp_free(instance);
}

void ios_init_freerdp()
{
	signal(SIGPIPE, SIG_IGN);
	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
}

void ios_uninit_freerdp()
{

}

/* compatibilty functions */
size_t fwrite$UNIX2003( const void *ptr, size_t size, size_t nmemb, FILE *stream )
{
	return fwrite(ptr, size , nmemb, stream);
}
