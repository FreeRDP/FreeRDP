/*
 RDP run-loop

 Copyright 2013 Thincast Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#include <winpr/assert.h>

#import <freerdp/gdi/gdi.h>
#import <freerdp/channels/channels.h>
#import <freerdp/client/channels.h>
#import <freerdp/client/cmdline.h>
#import <freerdp/freerdp.h>
#import <freerdp/gdi/gfx.h>

#import "ios_freerdp.h"
#import "ios_freerdp_ui.h"
#import "ios_freerdp_events.h"

#import "RDPSession.h"
#import "Utils.h"

#include <errno.h>

#define TAG FREERDP_TAG("iOS")

#pragma mark Connection helpers

static void ios_OnChannelConnectedEventHandler(void *context, const ChannelConnectedEventArgs *e)
{
	rdpSettings *settings;
	mfContext *afc;

	if (!context || !e)
	{
		WLog_FATAL(TAG, "%s(context=%p, EventArgs=%p", __FUNCTION__, context, (void *)e);
		return;
	}

	afc = (mfContext *)context;
	settings = afc->_p.settings;

	if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
		{
			gdi_graphics_pipeline_init(afc->_p.gdi, (RdpgfxClientContext *)e->pInterface);
		}
		else
		{
			WLog_WARN(TAG, "GFX without software GDI requested. "
			               " This is not supported, add /gdi:sw");
		}
	}
}

static void ios_OnChannelDisconnectedEventHandler(void *context,
                                                  const ChannelDisconnectedEventArgs *e)
{
	rdpSettings *settings;
	mfContext *afc;

	if (!context || !e)
	{
		WLog_FATAL(TAG, "%s(context=%p, EventArgs=%p", __FUNCTION__, context, (void *)e);
		return;
	}

	afc = (mfContext *)context;
	settings = afc->_p.settings;

	if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
		{
			gdi_graphics_pipeline_uninit(afc->_p.gdi, (RdpgfxClientContext *)e->pInterface);
		}
		else
		{
			WLog_WARN(TAG, "GFX without software GDI requested. "
			               " This is not supported, add /gdi:sw");
		}
	}
}

static BOOL ios_pre_connect(freerdp *instance)
{
	int rc;
	rdpSettings *settings;

	if (!instance || !instance->context)
		return FALSE;

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	settings->AutoLogonEnabled = settings->Password && (strlen(settings->Password) > 0);

	// Verify screen width/height are sane
	if ((settings->DesktopWidth < 64) || (settings->DesktopHeight < 64) ||
	    (settings->DesktopWidth > 4096) || (settings->DesktopHeight > 4096))
	{
		NSLog(@"%s: invalid dimensions %d %d", __func__, settings->DesktopWidth,
		      settings->DesktopHeight);
		return FALSE;
	}

	rc = PubSub_SubscribeChannelConnected(instance->context->pubSub,
	                                      ios_OnChannelConnectedEventHandler);

	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "Could not subscribe to connect event handler [%l08X]", rc);
		return FALSE;
	}

	rc = PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                         ios_OnChannelDisconnectedEventHandler);

	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "Could not subscribe to disconnect event handler [%l08X]", rc);
		return FALSE;
	}

	if (!freerdp_client_load_addins(instance->context->channels, settings))
	{
		WLog_ERR(TAG, "Failed to load addins [%l08X]", GetLastError());
		return FALSE;
	}

	return TRUE;
}

static BOOL ios_Pointer_New(rdpContext *context, rdpPointer *pointer)
{
	if (!context || !pointer || !context->gdi)
		return FALSE;

	return TRUE;
}

static void ios_Pointer_Free(rdpContext *context, rdpPointer *pointer)
{
	if (!context || !pointer)
		return;
}

static BOOL ios_Pointer_Set(rdpContext *context, rdpPointer *pointer)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL ios_Pointer_SetPosition(rdpContext *context, UINT32 x, UINT32 y)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL ios_Pointer_SetNull(rdpContext *context)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL ios_Pointer_SetDefault(rdpContext *context)
{
	if (!context)
		return FALSE;

	return TRUE;
}

static BOOL ios_register_pointer(rdpGraphics *graphics)
{
	rdpPointer pointer;

	if (!graphics)
		return FALSE;

	pointer.size = sizeof(pointer);
	pointer.New = ios_Pointer_New;
	pointer.Free = ios_Pointer_Free;
	pointer.Set = ios_Pointer_Set;
	pointer.SetNull = ios_Pointer_SetNull;
	pointer.SetDefault = ios_Pointer_SetDefault;
	pointer.SetPosition = ios_Pointer_SetPosition;
	graphics_register_pointer(graphics, &pointer);
	return TRUE;
}

static BOOL ios_post_connect(freerdp *instance)
{
	mfInfo *mfi;

	if (!instance)
		return FALSE;

	mfi = MFI_FROM_INSTANCE(instance);

	if (!mfi)
		return FALSE;

	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
		return FALSE;

	if (!ios_register_pointer(instance->context->graphics))
		return FALSE;

	ios_allocate_display_buffer(mfi);
	instance->context->update->BeginPaint = ios_ui_begin_paint;
	instance->context->update->EndPaint = ios_ui_end_paint;
	instance->context->update->DesktopResize = ios_ui_resize_window;
	[mfi->session performSelectorOnMainThread:@selector(sessionDidConnect)
	                               withObject:nil
	                            waitUntilDone:YES];
	return TRUE;
}

static void ios_post_disconnect(freerdp *instance)
{
	gdi_free(instance);
}

#pragma mark -
#pragma mark Running the connection

int ios_run_freerdp(freerdp *instance)
{
	mfContext *context = (mfContext *)instance->context;
	mfInfo *mfi = context->mfi;
	rdpChannels *channels = instance->context->channels;
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
	NSAutoreleasePool *pool;

	while (!freerdp_shall_disconnect_context(instance->context))
	{
		DWORD status;
		DWORD nCount = 0;
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
		pool = [[NSAutoreleasePool alloc] init];

		nCount = freerdp_get_event_handles(instance->context, handles, ARRAYSIZE(handles));
		if (nCount == 0)
		{
			NSLog(@"%s: freerdp_get_event_handles failed", __func__);
			break;
		}

		handles[nCount++] = ios_events_get_handle(mfi);

		status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);

		if (WAIT_FAILED == status)
		{
			NSLog(@"%s: WaitForMultipleObjects failed!", __func__);
			break;
		}

		// Check the libfreerdp fds
		if (!freerdp_check_event_handles(instance->context))
		{
			NSLog(@"%s: freerdp_check_event_handles failed.", __func__);
			break;
		}

		// Check input event fds
		if (ios_events_check_handle(mfi) != TRUE)
		{
			// This event will fail when the app asks for a disconnect.
			// NSLog(@"%s: ios_events_check_fds failed: terminating connection.", __func__);
			break;
		}

		[pool release];
		pool = nil;
	}

	CGContextRelease(mfi->bitmap_context);
	mfi->bitmap_context = NULL;
	mfi->connection_state = TSXConnectionDisconnected;
	// Cleanup
	freerdp_disconnect(instance);
	gdi_free(instance);
	cache_free(instance->context->cache);
	[pool release];
	pool = nil;
	return MF_EXIT_SUCCESS;
}

#pragma mark -
#pragma mark Context callbacks

static BOOL ios_client_new(freerdp *instance, rdpContext *context)
{
	mfContext *ctx = (mfContext *)context;

	if (!instance || !context)
		return FALSE;

	if ((ctx->mfi = calloc(1, sizeof(mfInfo))) == NULL)
		return FALSE;

	ctx->mfi->context = (mfContext *)context;
	ctx->mfi->_context = context;
	ctx->mfi->instance = instance;

	if (!ios_events_create_pipe(ctx->mfi))
		return FALSE;

	instance->PreConnect = ios_pre_connect;
	instance->PostConnect = ios_post_connect;
	instance->PostDisconnect = ios_post_disconnect;
	instance->Authenticate = ios_ui_authenticate;
	instance->GatewayAuthenticate = ios_ui_gw_authenticate;
	instance->VerifyCertificateEx = ios_ui_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = ios_ui_verify_changed_certificate_ex;
	instance->LogonErrorInfo = NULL;
	return TRUE;
}

static void ios_client_free(freerdp *instance, rdpContext *context)
{
	mfInfo *mfi;

	if (!context)
		return;

	mfi = ((mfContext *)context)->mfi;
	ios_events_free_pipe(mfi);
	free(mfi);
}

static int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS *pEntryPoints)
{
	WINPR_ASSERT(pEntryPoints);

	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = NULL;
	pEntryPoints->GlobalUninit = NULL;
	pEntryPoints->ContextSize = sizeof(mfContext);
	pEntryPoints->ClientNew = ios_client_new;
	pEntryPoints->ClientFree = ios_client_free;
	pEntryPoints->ClientStart = NULL;
	pEntryPoints->ClientStop = NULL;
	return 0;
}

#pragma mark -
#pragma mark Initialization and cleanup

freerdp *ios_freerdp_new()
{
	rdpContext *context;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		return NULL;

	return context->instance;
}

void ios_freerdp_free(freerdp *instance)
{
	if (!instance || !instance->context)
		return;

	freerdp_client_context_free(instance->context);
}

void ios_init_freerdp()
{
	signal(SIGPIPE, SIG_IGN);
}

void ios_uninit_freerdp()
{
}

/* compatibilty functions */
size_t fwrite$UNIX2003(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	return fwrite(ptr, size, nmemb, stream);
}
