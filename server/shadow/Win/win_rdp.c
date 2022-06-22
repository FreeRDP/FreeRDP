/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/print.h>
#include <winpr/assert.h>

#include <freerdp/log.h>

#include "win_rdp.h"

#define TAG SERVER_TAG("shadow.win")

static void shw_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e)
{
	shwContext* shw = (shwContext*)context;
	WINPR_ASSERT(e);
	WLog_INFO(TAG, "OnChannelConnected: %s", e->name);
}

static void shw_OnChannelDisconnectedEventHandler(void* context,
                                                  const ChannelDisconnectedEventArgs* e)
{
	shwContext* shw = (shwContext*)context;
	WINPR_ASSERT(e);
	WLog_INFO(TAG, "OnChannelDisconnected: %s", e->name);
}

static BOOL shw_begin_paint(rdpContext* context)
{
	shwContext* shw;
	rdpGdi* gdi;

	WINPR_ASSERT(context);
	gdi = context->gdi;
	WINPR_ASSERT(gdi);
	shw = (shwContext*)context;
	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	gdi->primary->hdc->hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL shw_end_paint(rdpContext* context)
{
	int index;
	int ninvalid;
	HGDI_RGN cinvalid;
	RECTANGLE_16 invalidRect;
	rdpGdi* gdi = context->gdi;
	shwContext* shw = (shwContext*)context;
	winShadowSubsystem* subsystem = shw->subsystem;
	rdpShadowSurface* surface = subsystem->base.server->surface;
	ninvalid = gdi->primary->hdc->hwnd->ninvalid;
	cinvalid = gdi->primary->hdc->hwnd->cinvalid;

	for (index = 0; index < ninvalid; index++)
	{
		invalidRect.left = cinvalid[index].x;
		invalidRect.top = cinvalid[index].y;
		invalidRect.right = cinvalid[index].x + cinvalid[index].w;
		invalidRect.bottom = cinvalid[index].y + cinvalid[index].h;
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
	}

	SetEvent(subsystem->RdpUpdateEnterEvent);
	WaitForSingleObject(subsystem->RdpUpdateLeaveEvent, INFINITE);
	ResetEvent(subsystem->RdpUpdateLeaveEvent);
	return TRUE;
}

BOOL shw_desktop_resize(rdpContext* context)
{
	WLog_WARN(TAG, "Desktop resizing not implemented!");
	return TRUE;
}

static BOOL shw_surface_frame_marker(rdpContext* context,
                                     const SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	shwContext* shw = (shwContext*)context;
	return TRUE;
}

static BOOL shw_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	WLog_WARN(TAG, "Authentication not implemented, access granted to everyone!");
	return TRUE;
}

static int shw_verify_x509_certificate(freerdp* instance, const BYTE* data, size_t length,
                                       const char* hostname, UINT16 port, DWORD flags)
{
	WLog_WARN(TAG, "Certificate checks not implemented, access granted to everyone!");
	return 1;
}

static void shw_OnConnectionResultEventHandler(void* context, const ConnectionResultEventArgs* e)
{
	shwContext* shw = (shwContext*)context;
	WINPR_ASSERT(e);
	WLog_INFO(TAG, "OnConnectionResult: %d", e->result);
}

static BOOL shw_pre_connect(freerdp* instance)
{
	shwContext* shw;
	rdpContext* context = instance->context;
	shw = (shwContext*)context;
	PubSub_SubscribeConnectionResult(context->pubSub, shw_OnConnectionResultEventHandler);
	PubSub_SubscribeChannelConnected(context->pubSub, shw_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(context->pubSub, shw_OnChannelDisconnectedEventHandler);

	return TRUE;
}

static BOOL shw_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	shwContext* shw;
	rdpUpdate* update;
	rdpSettings* settings;

	WINPR_ASSERT(instance);

	shw = (shwContext*)instance->context;
	WINPR_ASSERT(shw);

	update = instance->context->update;
	WINPR_ASSERT(update);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	if (!gdi_init(instance, PIXEL_FORMAT_BGRX32))
		return FALSE;

	gdi = instance->context->gdi;
	update->BeginPaint = shw_begin_paint;
	update->EndPaint = shw_end_paint;
	update->DesktopResize = shw_desktop_resize;
	update->SurfaceFrameMarker = shw_surface_frame_marker;
	return TRUE;
}

static DWORD WINAPI shw_client_thread(LPVOID arg)
{
	int index;
	BOOL bSuccess;
	shwContext* shw;
	rdpContext* context;
	rdpChannels* channels;

	freerdp* instance = (freerdp*)arg;
	WINPR_ASSERT(instance);

	context = (rdpContext*)instance->context;
	WINPR_ASSERT(context);

	shw = (shwContext*)context;

	bSuccess = freerdp_connect(instance);
	WLog_INFO(TAG, "freerdp_connect: %d", bSuccess);

	if (!bSuccess)
	{
		ExitThread(0);
		return 0;
	}

	channels = context->channels;
	WINPR_ASSERT(channels);

	while (1)
	{
		DWORD status;
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
		DWORD count = freerdp_get_event_handles(instance->context, handles, ARRAYSIZE(handles));

		if ((count == 0) || (count == MAXIMUM_WAIT_OBJECTS))
		{
			WLog_ERR(TAG, "Failed to get FreeRDP event handles");
			break;
		}

		handles[count++] = freerdp_channels_get_event_handle(instance);

		if (MsgWaitForMultipleObjects(count, handles, FALSE, 1000, QS_ALLINPUT) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "MsgWaitForMultipleObjects failure: 0x%08lX", GetLastError());
			break;
		}

		if (!freerdp_check_fds(instance))
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}

		if (freerdp_shall_disconnect_context(instance->context))
		{
			break;
		}

		if (!freerdp_channels_check_fds(channels, instance))
		{
			WLog_ERR(TAG, "Failed to check channels file descriptor");
			break;
		}
	}

	freerdp_free(instance);
	ExitThread(0);
	return 0;
}

/**
 * Client Interface
 */

static BOOL shw_freerdp_client_global_init(void)
{
	return TRUE;
}

static void shw_freerdp_client_global_uninit(void)
{
}

static int shw_freerdp_client_start(rdpContext* context)
{
	shwContext* shw;
	freerdp* instance = context->instance;
	shw = (shwContext*)context;

	if (!(shw->common.thread = CreateThread(NULL, 0, shw_client_thread, instance, 0, NULL)))
	{
		WLog_ERR(TAG, "Failed to create thread");
		return -1;
	}

	return 0;
}

static int shw_freerdp_client_stop(rdpContext* context)
{
	shwContext* shw = (shwContext*)context;
	SetEvent(shw->StopEvent);
	return 0;
}

static BOOL shw_freerdp_client_new(freerdp* instance, rdpContext* context)
{
	shwContext* shw;
	rdpSettings* settings;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(context);

	shw = (shwContext*)instance->context;
	WINPR_ASSERT(shw);

	if (!(shw->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		return FALSE;

	instance->LoadChannels = freerdp_client_load_channels;
	instance->PreConnect = shw_pre_connect;
	instance->PostConnect = shw_post_connect;
	instance->Authenticate = shw_authenticate;
	instance->VerifyX509Certificate = shw_verify_x509_certificate;

	settings = context->settings;
	WINPR_ASSERT(settings);

	shw->settings = settings;
	settings->AsyncChannels = FALSE;
	settings->AsyncUpdate = FALSE;
	settings->IgnoreCertificate = TRUE;
	settings->ExternalCertificateManagement = TRUE;
	settings->RdpSecurity = TRUE;
	settings->TlsSecurity = TRUE;
	settings->NlaSecurity = FALSE;
	settings->BitmapCacheEnabled = FALSE;
	settings->BitmapCacheV3Enabled = FALSE;
	settings->OffscreenSupportLevel = FALSE;
	settings->GlyphSupportLevel = GLYPH_SUPPORT_NONE;
	settings->BrushSupportLevel = FALSE;
	ZeroMemory(settings->OrderSupport, 32);
	settings->FrameMarkerCommandEnabled = TRUE;
	settings->SurfaceFrameMarkerEnabled = TRUE;
	settings->AltSecFrameMarkerSupport = TRUE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32))
		return FALSE;
	settings->NSCodec = TRUE;
	settings->RemoteFxCodec = TRUE;
	settings->FastPathInput = TRUE;
	settings->FastPathOutput = TRUE;
	settings->LargePointerFlag = TRUE;
	settings->CompressionEnabled = FALSE;
	settings->AutoReconnectionEnabled = FALSE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, FALSE))
		return FALSE;
	settings->SupportHeartbeatPdu = FALSE;
	settings->SupportMultitransport = FALSE;
	settings->ConnectionType = CONNECTION_TYPE_LAN;
	settings->AllowFontSmoothing = TRUE;
	settings->AllowDesktopComposition = TRUE;
	settings->DisableWallpaper = FALSE;
	settings->DisableFullWindowDrag = TRUE;
	settings->DisableMenuAnims = TRUE;
	settings->DisableThemes = FALSE;
	settings->DeviceRedirection = TRUE;
	settings->RedirectClipboard = TRUE;
	settings->SupportDynamicChannels = TRUE;
	return TRUE;
}

static void shw_freerdp_client_free(freerdp* instance, rdpContext* context)
{
	shwContext* shw = (shwContext*)instance->context;
}

int shw_RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->Version = 1;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->settings = NULL;
	pEntryPoints->ContextSize = sizeof(shwContext);
	pEntryPoints->GlobalInit = shw_freerdp_client_global_init;
	pEntryPoints->GlobalUninit = shw_freerdp_client_global_uninit;
	pEntryPoints->ClientNew = shw_freerdp_client_new;
	pEntryPoints->ClientFree = shw_freerdp_client_free;
	pEntryPoints->ClientStart = shw_freerdp_client_start;
	pEntryPoints->ClientStop = shw_freerdp_client_stop;
	return 0;
}

int win_shadow_rdp_init(winShadowSubsystem* subsystem)
{
	rdpContext* context;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	ZeroMemory(&clientEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;
	shw_RdpClientEntry(&clientEntryPoints);

	if (!(subsystem->RdpUpdateEnterEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail_enter_event;

	if (!(subsystem->RdpUpdateLeaveEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail_leave_event;

	if (!(context = freerdp_client_context_new(&clientEntryPoints)))
		goto fail_context;

	subsystem->shw = (shwContext*)context;
	subsystem->shw->settings = context->settings;
	subsystem->shw->subsystem = subsystem;
	return 1;
fail_context:
	CloseHandle(subsystem->RdpUpdateLeaveEvent);
fail_leave_event:
	CloseHandle(subsystem->RdpUpdateEnterEvent);
fail_enter_event:
	return -1;
}

int win_shadow_rdp_start(winShadowSubsystem* subsystem)
{
	int status;
	shwContext* shw = subsystem->shw;
	rdpContext* context = (rdpContext*)shw;
	status = freerdp_client_start(context);
	return status;
}

int win_shadow_rdp_stop(winShadowSubsystem* subsystem)
{
	int status;
	shwContext* shw = subsystem->shw;
	rdpContext* context = (rdpContext*)shw;
	status = freerdp_client_stop(context);
	return status;
}

int win_shadow_rdp_uninit(winShadowSubsystem* subsystem)
{
	win_shadow_rdp_stop(subsystem);
	return 1;
}
