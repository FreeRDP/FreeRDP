/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Client
 *
 * Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
 * Copyright 2016 Thincast Technologies GmbH
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
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

#include <stdio.h>
#include <errno.h>
#include <locale.h>
#include <float.h>

#include <freerdp/client/cmdline.h>
#include <freerdp/channels/channels.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client.h>
#include <freerdp/utils/signal.h>
#include <freerdp/locale/keyboard.h>

#include <linux/input.h>

#include <uwac/uwac.h>

#include "wlfreerdp.h"
#include "wlf_input.h"
#include "wlf_cliprdr.h"
#include "wlf_disp.h"
#include "wlf_channels.h"
#include "wlf_pointer.h"

#define TAG CLIENT_TAG("wayland")

static BOOL wl_begin_paint(rdpContext* context)
{
	rdpGdi* gdi;

	if (!context || !context->gdi)
		return FALSE;

	gdi = context->gdi;

	if (!gdi->primary)
		return FALSE;

	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	return TRUE;
}

static BOOL wl_update_buffer(wlfContext* context_w, INT32 ix, INT32 iy, INT32 iw, INT32 ih)
{
	rdpGdi* gdi;
	char* data;
	UINT32 x, y, w, h;
	UwacSize geometry;
	size_t stride;
	UwacReturnCode rc;
	RECTANGLE_16 area;

	if (!context_w)
		return FALSE;

	if ((ix < 0) || (iy < 0) || (iw < 0) || (ih < 0))
		return FALSE;

	x = (UINT32)ix;
	y = (UINT32)iy;
	w = (UINT32)iw;
	h = (UINT32)ih;
	rc = UwacWindowGetDrawingBufferGeometry(context_w->window, &geometry, &stride);
	data = UwacWindowGetDrawingBuffer(context_w->window);

	if (!data || (rc != UWAC_SUCCESS))
		return FALSE;

	gdi = context_w->context.gdi;

	if (!gdi)
		return FALSE;

	/* Ignore output if the surface size does not match. */
	if (((INT64)x > geometry.width) || ((INT64)y > geometry.height))
		return TRUE;

	area.left = x;
	area.top = y;
	area.right = x + w;
	area.bottom = y + h;

	if (!wlf_copy_image(gdi->primary_buffer, gdi->stride, gdi->width, gdi->height, data, stride,
	                    geometry.width, geometry.height, &area,
	                    context_w->context.settings->SmartSizing))
		return FALSE;

	if (!wlf_scale_coordinates(&context_w->context, &x, &y, FALSE))
		return FALSE;

	if (!wlf_scale_coordinates(&context_w->context, &w, &h, FALSE))
		return FALSE;

	if (UwacWindowAddDamage(context_w->window, x, y, w, h) != UWAC_SUCCESS)
		return FALSE;

	if (UwacWindowSubmitBuffer(context_w->window, false) != UWAC_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL wl_end_paint(rdpContext* context)
{
	rdpGdi* gdi;
	wlfContext* context_w;
	INT32 x, y;
	INT32 w, h;

	if (!context || !context->gdi || !context->gdi->primary)
		return FALSE;

	gdi = context->gdi;

	if (gdi->primary->hdc->hwnd->invalid->null)
		return TRUE;

	x = gdi->primary->hdc->hwnd->invalid->x;
	y = gdi->primary->hdc->hwnd->invalid->y;
	w = gdi->primary->hdc->hwnd->invalid->w;
	h = gdi->primary->hdc->hwnd->invalid->h;
	context_w = (wlfContext*)context;
	return wl_update_buffer(context_w, x, y, w, h);
}

static BOOL wl_refresh_display(wlfContext* context)
{
	rdpGdi* gdi;

	if (!context || !context->context.gdi)
		return FALSE;

	gdi = context->context.gdi;
	return wl_update_buffer(context, 0, 0, gdi->width, gdi->height);
}

static BOOL wl_resize_display(rdpContext* context)
{
	wlfContext* wlc = (wlfContext*)context;
	rdpGdi* gdi = context->gdi;
	rdpSettings* settings = context->settings;

	if (!gdi_resize(gdi, settings->DesktopWidth, settings->DesktopHeight))
		return FALSE;

	return wl_refresh_display(wlc);
}

static BOOL wl_pre_connect(freerdp* instance)
{
	rdpSettings* settings;
	wlfContext* context;
	UwacOutput* output;
	UwacSize resolution;

	if (!instance)
		return FALSE;

	context = (wlfContext*)instance->context;
	settings = instance->settings;

	if (!context || !settings)
		return FALSE;

	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_WAYLAND;
	PubSub_SubscribeChannelConnected(instance->context->pubSub, wlf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    wlf_OnChannelDisconnectedEventHandler);

	if (settings->Fullscreen)
	{
		// Use the resolution of the first display output
		output = UwacDisplayGetOutput(context->display, 1);

		if (output != NULL && UwacOutputGetResolution(output, &resolution) == UWAC_SUCCESS)
		{
			settings->DesktopWidth = (UINT32)resolution.width;
			settings->DesktopHeight = (UINT32)resolution.height;
		}
		else
		{
			WLog_WARN(TAG, "Failed to get output resolution! Check your display settings");
		}
	}

	if (!freerdp_client_load_addins(instance->context->channels, instance->settings))
		return FALSE;

	return TRUE;
}

static BOOL wl_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	UwacWindow* window;
	wlfContext* context;
	rdpSettings* settings;
	UINT32 w, h;

	if (!instance || !instance->context)
		return FALSE;

	context = (wlfContext*)instance->context;
	settings = instance->context->settings;

	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
		return FALSE;

	gdi = instance->context->gdi;

	if (!gdi || (gdi->width < 0) || (gdi->height < 0))
		return FALSE;

	if (!wlf_register_pointer(instance->context->graphics))
		return FALSE;

	w = (UINT32)gdi->width;
	h = (UINT32)gdi->height;

	if (settings->SmartSizing && !context->fullscreen)
	{
		if (settings->SmartSizingWidth > 0)
			w = settings->SmartSizingWidth;

		if (settings->SmartSizingHeight > 0)
			h = settings->SmartSizingHeight;
	}

	context->window = window = UwacCreateWindowShm(context->display, w, h, WL_SHM_FORMAT_XRGB8888);

	if (!window)
		return FALSE;

	UwacWindowSetFullscreenState(window, NULL, instance->context->settings->Fullscreen);
	UwacWindowSetTitle(window, "FreeRDP");
	UwacWindowSetOpaqueRegion(context->window, 0, 0, w, h);
	instance->update->BeginPaint = wl_begin_paint;
	instance->update->EndPaint = wl_end_paint;
	instance->update->DesktopResize = wl_resize_display;
	freerdp_keyboard_init(instance->context->settings->KeyboardLayout);

	if (!(context->disp = wlf_disp_new(context)))
		return FALSE;

	context->clipboard = wlf_clipboard_new(context);

	if (!context->clipboard)
		return FALSE;

	return wl_refresh_display(context);
}

static void wl_post_disconnect(freerdp* instance)
{
	wlfContext* context;

	if (!instance)
		return;

	if (!instance->context)
		return;

	context = (wlfContext*)instance->context;
	gdi_free(instance);
	wlf_clipboard_free(context->clipboard);
	wlf_disp_free(context->disp);

	if (context->window)
		UwacDestroyWindow(&context->window);
}

static BOOL handle_uwac_events(freerdp* instance, UwacDisplay* display)
{
	UwacEvent event;
	wlfContext* context;

	if (UwacDisplayDispatch(display, 1) < 0)
		return FALSE;

	context = (wlfContext*)instance->context;

	while (UwacHasEvent(display))
	{
		if (UwacNextEvent(display, &event) != UWAC_SUCCESS)
			return FALSE;

		/*printf("UWAC event type %d\n", event.type);*/
		switch (event.type)
		{
			case UWAC_EVENT_NEW_SEAT:
				context->seat = event.seat_new.seat;
				break;

			case UWAC_EVENT_REMOVED_SEAT:
				context->seat = NULL;
				break;

			case UWAC_EVENT_FRAME_DONE:
				if (UwacWindowSubmitBuffer(context->window, false) != UWAC_SUCCESS)
					return FALSE;

				break;

			case UWAC_EVENT_POINTER_ENTER:
				if (!wlf_handle_pointer_enter(instance, &event.mouse_enter_leave))
					return FALSE;

				break;

			case UWAC_EVENT_POINTER_MOTION:
				if (!wlf_handle_pointer_motion(instance, &event.mouse_motion))
					return FALSE;

				break;

			case UWAC_EVENT_POINTER_BUTTONS:
				if (!wlf_handle_pointer_buttons(instance, &event.mouse_button))
					return FALSE;

				break;

			case UWAC_EVENT_POINTER_AXIS:
				if (!wlf_handle_pointer_axis(instance, &event.mouse_axis))
					return FALSE;

				break;

			case UWAC_EVENT_KEY:
				if (!wlf_handle_key(instance, &event.key))
					return FALSE;

				break;

			case UWAC_EVENT_TOUCH_UP:
				if (!wlf_handle_touch_up(instance, &event.touchUp))
					return FALSE;

				break;

			case UWAC_EVENT_TOUCH_DOWN:
				if (!wlf_handle_touch_down(instance, &event.touchDown))
					return FALSE;

				break;

			case UWAC_EVENT_TOUCH_MOTION:
				if (!wlf_handle_touch_motion(instance, &event.touchMotion))
					return FALSE;

				break;

			case UWAC_EVENT_KEYBOARD_ENTER:
				if (instance->context->settings->GrabKeyboard)
					UwacSeatInhibitShortcuts(event.keyboard_enter_leave.seat, true);

				if (!wlf_keyboard_enter(instance, &event.keyboard_enter_leave))
					return FALSE;

				break;

			case UWAC_EVENT_CONFIGURE:
				if (!wlf_disp_handle_configure(context->disp, event.configure.width,
				                               event.configure.height))
					return FALSE;

				if (!wl_refresh_display(context))
					return FALSE;

				break;

			case UWAC_EVENT_CLIPBOARD_AVAILABLE:
			case UWAC_EVENT_CLIPBOARD_OFFER:
			case UWAC_EVENT_CLIPBOARD_SELECT:
				if (!wlf_cliprdr_handle_event(context->clipboard, &event.clipboard))
					return FALSE;

				break;

			default:
				break;
		}
	}

	return TRUE;
}

static BOOL handle_window_events(freerdp* instance)
{
	rdpSettings* settings;

	if (!instance || !instance->settings)
		return FALSE;

	settings = instance->settings;

	if (!settings->AsyncInput)
	{
	}

	return TRUE;
}

static int wlfreerdp_run(freerdp* instance)
{
	wlfContext* context;
	DWORD count;
	HANDLE handles[64];
	DWORD status = WAIT_ABANDONED;

	if (!instance)
		return -1;

	context = (wlfContext*)instance->context;

	if (!context)
		return -1;

	if (!freerdp_connect(instance))
	{
		WLog_Print(context->log, WLOG_ERROR, "Failed to connect");
		return -1;
	}

	while (!freerdp_shall_disconnect(instance))
	{
		handles[0] = context->displayHandle;
		count = freerdp_get_event_handles(instance->context, &handles[1], 63) + 1;

		if (count <= 1)
		{
			WLog_Print(context->log, WLOG_ERROR, "Failed to get FreeRDP file descriptor");
			break;
		}

		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);

		if (WAIT_FAILED == status)
		{
			WLog_Print(context->log, WLOG_ERROR, "%s: WaitForMultipleObjects failed", __FUNCTION__);
			break;
		}

		if (!handle_uwac_events(instance, context->display))
		{
			WLog_Print(context->log, WLOG_ERROR, "error handling UWAC events");
			break;
		}

		if (freerdp_check_event_handles(instance->context) != TRUE)
		{
			if (client_auto_reconnect_ex(instance, handle_window_events))
				continue;
			else
			{
				/*
				 * Indicate an unsuccessful connection attempt if reconnect
				 * did not succeed and no other error was specified.
				 */
				if (freerdp_error_info(instance) == 0)
					status = 42;
			}

			if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_SUCCESS)
				WLog_Print(context->log, WLOG_ERROR, "Failed to check FreeRDP file descriptor");

			break;
		}
	}

	freerdp_disconnect(instance);
	return status;
}

static BOOL wlf_client_global_init(void)
{
	setlocale(LC_ALL, "");

	if (freerdp_handle_signals() != 0)
		return FALSE;

	return TRUE;
}

static void wlf_client_global_uninit(void)
{
}

static int wlf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	wlfContext* wlf;
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	wlf = (wlfContext*)instance->context;
	WLog_Print(wlf->log, WLOG_INFO, "Logon Error Info %s [%s]", str_data, str_type);
	return 1;
}

static BOOL wlf_client_new(freerdp* instance, rdpContext* context)
{
	UwacReturnCode status;
	wlfContext* wfl = (wlfContext*)context;

	if (!instance || !context)
		return FALSE;

	instance->PreConnect = wl_pre_connect;
	instance->PostConnect = wl_post_connect;
	instance->PostDisconnect = wl_post_disconnect;
	instance->Authenticate = client_cli_authenticate;
	instance->GatewayAuthenticate = client_cli_gw_authenticate;
	instance->VerifyCertificateEx = client_cli_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
	instance->LogonErrorInfo = wlf_logon_error_info;
	wfl->log = WLog_Get(TAG);
	wfl->display = UwacOpenDisplay(NULL, &status);

	if (!wfl->display || (status != UWAC_SUCCESS) || !wfl->log)
		return FALSE;

	wfl->displayHandle = CreateFileDescriptorEvent(NULL, FALSE, FALSE,
	                                               UwacDisplayGetFd(wfl->display), WINPR_FD_READ);

	if (!wfl->displayHandle)
		return FALSE;

	return TRUE;
}

static void wlf_client_free(freerdp* instance, rdpContext* context)
{
	wlfContext* wlf = (wlfContext*)instance->context;

	if (!context)
		return;

	if (wlf->display)
		UwacCloseDisplay(&wlf->display);

	if (wlf->displayHandle)
		CloseHandle(wlf->displayHandle);
}

static int wfl_client_start(rdpContext* context)
{
	WINPR_UNUSED(context);
	return 0;
}

static int wfl_client_stop(rdpContext* context)
{
	WINPR_UNUSED(context);
	return 0;
}

static int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = wlf_client_global_init;
	pEntryPoints->GlobalUninit = wlf_client_global_uninit;
	pEntryPoints->ContextSize = sizeof(wlfContext);
	pEntryPoints->ClientNew = wlf_client_new;
	pEntryPoints->ClientFree = wlf_client_free;
	pEntryPoints->ClientStart = wfl_client_start;
	pEntryPoints->ClientStop = wfl_client_stop;
	return 0;
}

int main(int argc, char* argv[])
{
	int rc = -1;
	int status;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* context;
	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		goto fail;

	status = freerdp_client_settings_parse_command_line(context->settings, argc, argv, FALSE);
	status =
	    freerdp_client_settings_command_line_status_print(context->settings, status, argc, argv);

	if (status)
		return 0;

	if (freerdp_client_start(context) != 0)
		goto fail;

	rc = wlfreerdp_run(context->instance);

	if (freerdp_client_stop(context) != 0)
		rc = -1;

fail:
	freerdp_client_context_free(context);
	return rc;
}

BOOL wlf_copy_image(const void* src, size_t srcStride, size_t srcWidth, size_t srcHeight, void* dst,
                    size_t dstStride, size_t dstWidth, size_t dstHeight, const RECTANGLE_16* area,
                    BOOL scale)
{
	BOOL rc = FALSE;

	if (!src || !dst || !area)
		return FALSE;

	if (scale)
	{
		return freerdp_image_scale(dst, PIXEL_FORMAT_BGRA32, dstStride, 0, 0, dstWidth, dstHeight,
		                           src, PIXEL_FORMAT_BGRA32, srcStride, 0, 0, srcWidth, srcHeight);
	}
	else
	{
		size_t i;
		const size_t baseSrcOffset = area->top * srcStride + area->left * 4;
		const size_t baseDstOffset = area->top * dstStride + area->left * 4;
		const size_t width = MIN((size_t)area->right - area->left, dstWidth - area->left);
		const size_t height = MIN((size_t)area->bottom - area->top, dstHeight - area->top);
		const BYTE* psrc = (const BYTE*)src;
		BYTE* pdst = (BYTE*)dst;

		for (i = 0; i < height; i++)
		{
			const size_t srcOffset = i * srcStride + baseSrcOffset;
			const size_t dstOffset = i * dstStride + baseDstOffset;
			memcpy(&pdst[dstOffset], &psrc[srcOffset], width * 4);
		}

		rc = TRUE;
	}

	return rc;
}

BOOL wlf_scale_coordinates(rdpContext* context, UINT32* px, UINT32* py, BOOL fromLocalToRDP)
{
	wlfContext* wlf = (wlfContext*)context;
	rdpGdi* gdi;
	UwacSize geometry;
	double sx, sy;

	if (!context || !px || !py || !context->gdi)
		return FALSE;

	if (!context->settings->SmartSizing)
		return TRUE;

	gdi = context->gdi;

	if (UwacWindowGetDrawingBufferGeometry(wlf->window, &geometry, NULL) != UWAC_SUCCESS)
		return FALSE;

	sx = geometry.width / (double)gdi->width;
	sy = geometry.height / (double)gdi->height;

	if (!fromLocalToRDP)
	{
		*px *= sx;
		*py *= sy;
	}
	else
	{
		*px /= sx;
		*py /= sy;
	}

	return TRUE;
}
