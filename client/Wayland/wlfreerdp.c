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

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <locale.h>
#include <float.h>

#include <winpr/sysinfo.h>

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

static BOOL wl_update_buffer(wlfContext* context_w, INT32 ix, INT32 iy, INT32 iw, INT32 ih)
{
	BOOL res = FALSE;
	rdpGdi* gdi = NULL;
	char* data = NULL;
	UINT32 x = 0;
	UINT32 y = 0;
	UINT32 w = 0;
	UINT32 h = 0;
	UwacSize geometry;
	size_t stride = 0;
	UwacReturnCode rc = UWAC_ERROR_INTERNAL;
	RECTANGLE_16 area;

	if (!context_w)
		return FALSE;

	if ((ix < 0) || (iy < 0) || (iw < 0) || (ih < 0))
		return FALSE;

	EnterCriticalSection(&context_w->critical);
	x = (UINT32)ix;
	y = (UINT32)iy;
	w = (UINT32)iw;
	h = (UINT32)ih;
	rc = UwacWindowGetDrawingBufferGeometry(context_w->window, &geometry, &stride);
	data = UwacWindowGetDrawingBuffer(context_w->window);

	if (!data || (rc != UWAC_SUCCESS))
		goto fail;

	gdi = context_w->common.context.gdi;

	if (!gdi)
		goto fail;

	/* Ignore output if the surface size does not match. */
	if (((INT64)x > geometry.width) || ((INT64)y > geometry.height))
	{
		res = TRUE;
		goto fail;
	}

	area.left = x;
	area.top = y;
	area.right = x + w;
	area.bottom = y + h;

	if (!wlf_copy_image(
	        gdi->primary_buffer, gdi->stride, gdi->width, gdi->height, data, stride, geometry.width,
	        geometry.height, &area,
	        freerdp_settings_get_bool(context_w->common.context.settings, FreeRDP_SmartSizing)))
		goto fail;

	if (!wlf_scale_coordinates(&context_w->common.context, &x, &y, FALSE))
		goto fail;

	if (!wlf_scale_coordinates(&context_w->common.context, &w, &h, FALSE))
		goto fail;

	if (UwacWindowAddDamage(context_w->window, x, y, w, h) != UWAC_SUCCESS)
		goto fail;

	if (UwacWindowSubmitBuffer(context_w->window, false) != UWAC_SUCCESS)
		goto fail;

	res = TRUE;
fail:
	LeaveCriticalSection(&context_w->critical);
	return res;
}

static BOOL wl_end_paint(rdpContext* context)
{
	rdpGdi* gdi = NULL;
	wlfContext* context_w = NULL;
	INT32 x = 0;
	INT32 y = 0;
	INT32 w = 0;
	INT32 h = 0;

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
	if (!wl_update_buffer(context_w, x, y, w, h))
	{
		return FALSE;
	}

	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	gdi->primary->hdc->hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL wl_refresh_display(wlfContext* context)
{
	rdpGdi* gdi = NULL;

	if (!context || !context->common.context.gdi)
		return FALSE;

	gdi = context->common.context.gdi;
	return wl_update_buffer(context, 0, 0, gdi->width, gdi->height);
}

static BOOL wl_resize_display(rdpContext* context)
{
	wlfContext* wlc = (wlfContext*)context;
	rdpGdi* gdi = context->gdi;
	rdpSettings* settings = context->settings;

	if (!gdi_resize(gdi, freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
	                freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight)))
		return FALSE;

	return wl_refresh_display(wlc);
}

static BOOL wl_pre_connect(freerdp* instance)
{
	rdpSettings* settings = NULL;
	wlfContext* context = NULL;
	const UwacOutput* output = NULL;
	UwacSize resolution;

	if (!instance)
		return FALSE;

	context = (wlfContext*)instance->context;
	WINPR_ASSERT(context);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	if (!freerdp_settings_set_bool(settings, FreeRDP_CertificateCallbackPreferPEM, TRUE))
		return FALSE;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMAJORTYPE_UNIX))
		return FALSE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMinorType, OSMINORTYPE_NATIVE_WAYLAND))
		return FALSE;
	PubSub_SubscribeChannelConnected(instance->context->pubSub, wlf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    wlf_OnChannelDisconnectedEventHandler);

	if (freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
	{
		// Use the resolution of the first display output
		output = UwacDisplayGetOutput(context->display, 0);

		if ((output != NULL) && (UwacOutputGetResolution(output, &resolution) == UWAC_SUCCESS))
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth,
			                                 (UINT32)resolution.width))
				return FALSE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight,
			                                 (UINT32)resolution.height))
				return FALSE;
		}
		else
		{
			WLog_WARN(TAG, "Failed to get output resolution! Check your display settings");
		}
	}

	return TRUE;
}

static BOOL wl_post_connect(freerdp* instance)
{
	if (!instance || !instance->context)
		return FALSE;

	wlfContext* context = (wlfContext*)instance->context;
	WINPR_ASSERT(context);

	rdpSettings* settings = instance->context->settings;
	WINPR_ASSERT(settings);

	const char* title = "FreeRDP";
	const char* wtitle = freerdp_settings_get_string(settings, FreeRDP_WindowTitle);
	if (wtitle)
		title = wtitle;

	const char* app_id = "wlfreerdp";
	const char* wmclass = freerdp_settings_get_string(settings, FreeRDP_WmClass);
	if (wmclass)
		app_id = wmclass;

	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
		return FALSE;

	rdpGdi* gdi = instance->context->gdi;

	if (!gdi || (gdi->width < 0) || (gdi->height < 0))
		return FALSE;

	if (!wlf_register_pointer(instance->context->graphics))
		return FALSE;

	UINT32 w = (UINT32)gdi->width;
	UINT32 h = (UINT32)gdi->height;

	if (freerdp_settings_get_bool(settings, FreeRDP_SmartSizing) && !context->fullscreen)
	{
		const UINT32 sw = freerdp_settings_get_uint32(settings, FreeRDP_SmartSizingWidth);
		if (sw > 0)
			w = sw;

		const UINT32 sh = freerdp_settings_get_uint32(settings, FreeRDP_SmartSizingHeight);
		if (sh > 0)
			h = sh;
	}

	context->window = UwacCreateWindowShm(context->display, w, h, WL_SHM_FORMAT_XRGB8888);

	if (!context->window)
		return FALSE;

	UwacWindowSetFullscreenState(
	    context->window, NULL,
	    freerdp_settings_get_bool(instance->context->settings, FreeRDP_Fullscreen));
	UwacWindowSetTitle(context->window, title);
	UwacWindowSetAppId(context->window, app_id);
	UwacWindowSetOpaqueRegion(context->window, 0, 0, w, h);
	instance->context->update->EndPaint = wl_end_paint;
	instance->context->update->DesktopResize = wl_resize_display;
	UINT32 KeyboardLayout =
	    freerdp_settings_get_uint32(instance->context->settings, FreeRDP_KeyboardLayout);
	const char* KeyboardRemappingList =
	    freerdp_settings_get_string(instance->context->settings, FreeRDP_KeyboardRemappingList);

	freerdp_keyboard_init_ex(KeyboardLayout, KeyboardRemappingList);

	if (!(context->disp = wlf_disp_new(context)))
		return FALSE;

	context->clipboard = wlf_clipboard_new(context);

	if (!context->clipboard)
		return FALSE;

	return wl_refresh_display(context);
}

static void wl_post_disconnect(freerdp* instance)
{
	wlfContext* context = NULL;

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
	wlfContext* context = NULL;

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
			{
				EnterCriticalSection(&context->critical);
				UwacReturnCode r = UwacWindowSubmitBuffer(context->window, false);
				LeaveCriticalSection(&context->critical);
				if (r != UWAC_SUCCESS)
					return FALSE;
			}
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

			case UWAC_EVENT_POINTER_AXIS_DISCRETE:
				if (!wlf_handle_pointer_axis_discrete(instance, &event.mouse_axis))
					return FALSE;
				break;

			case UWAC_EVENT_POINTER_FRAME:
				if (!wlf_handle_pointer_frame(instance, &event.mouse_frame))
					return FALSE;
				break;
			case UWAC_EVENT_POINTER_SOURCE:
				if (!wlf_handle_pointer_source(instance, &event.mouse_source))
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
				if (freerdp_settings_get_bool(instance->context->settings, FreeRDP_GrabKeyboard))
					UwacSeatInhibitShortcuts(event.keyboard_enter_leave.seat, true);

				if (!wlf_keyboard_enter(instance, &event.keyboard_enter_leave))
					return FALSE;

				break;

			case UWAC_EVENT_KEYBOARD_MODIFIERS:
				if (!wlf_keyboard_modifiers(instance, &event.keyboard_modifiers))
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

			case UWAC_EVENT_CLOSE:
				context->closed = TRUE;

				break;

			default:
				break;
		}
	}

	return TRUE;
}

static BOOL handle_window_events(freerdp* instance)
{
	if (!instance)
		return FALSE;

	return TRUE;
}

static int wlfreerdp_run(freerdp* instance)
{
	wlfContext* context = NULL;
	HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
	DWORD status = WAIT_ABANDONED;
	HANDLE timer = NULL;
	LARGE_INTEGER due;

	TimerEventArgs timerEvent;
	EventArgsInit(&timerEvent, "xfreerdp");

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

	timer = CreateWaitableTimerA(NULL, FALSE, "mainloop-periodic-timer");

	if (!timer)
	{
		WLog_ERR(TAG, "failed to create timer");
		goto disconnect;
	}

	due.QuadPart = 0;

	if (!SetWaitableTimer(timer, &due, 20, NULL, NULL, FALSE))
	{
		goto disconnect;
	}

	while (!freerdp_shall_disconnect_context(instance->context))
	{
		DWORD count = 0;
		handles[count++] = timer;
		handles[count++] = context->displayHandle;
		count += freerdp_get_event_handles(instance->context, &handles[count],
		                                   ARRAYSIZE(handles) - count);

		if (count <= 2)
		{
			WLog_Print(context->log, WLOG_ERROR, "Failed to get FreeRDP file descriptor");
			break;
		}

		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);

		if (WAIT_FAILED == status)
		{
			WLog_Print(context->log, WLOG_ERROR, "WaitForMultipleObjects failed");
			break;
		}

		if (!handle_uwac_events(instance, context->display))
		{
			WLog_Print(context->log, WLOG_ERROR, "error handling UWAC events");
			break;
		}

		if (context->closed)
		{
			WLog_Print(context->log, WLOG_INFO, "Closed from Wayland");
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

		if ((status != WAIT_TIMEOUT) && (status == WAIT_OBJECT_0))
		{
			timerEvent.now = GetTickCount64();
			PubSub_OnTimer(context->common.context.pubSub, context, &timerEvent);
		}
	}

disconnect:
	if (timer)
		(void)CloseHandle(timer);
	freerdp_disconnect(instance);
	return status;
}

static BOOL wlf_client_global_init(void)
{
	(void)setlocale(LC_ALL, "");

	if (freerdp_handle_signals() != 0)
		return FALSE;

	return TRUE;
}

static void wlf_client_global_uninit(void)
{
}

static int wlf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	wlfContext* wlf = NULL;
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	wlf = (wlfContext*)instance->context;
	WLog_Print(wlf->log, WLOG_INFO, "Logon Error Info %s [%s]", str_data, str_type);
	return 1;
}

static void wlf_client_free(freerdp* instance, rdpContext* context)
{
	wlfContext* wlf = (wlfContext*)instance->context;

	if (!context)
		return;

	if (wlf->display)
		UwacCloseDisplay(&wlf->display);

	if (wlf->displayHandle)
		(void)CloseHandle(wlf->displayHandle);
	ArrayList_Free(wlf->events);
	DeleteCriticalSection(&wlf->critical);
}

static void* uwac_event_clone(const void* val)
{
	UwacEvent* copy = NULL;
	const UwacEvent* ev = (const UwacEvent*)val;

	copy = calloc(1, sizeof(UwacEvent));
	if (!copy)
		return NULL;
	*copy = *ev;
	return copy;
}

static BOOL wlf_client_new(freerdp* instance, rdpContext* context)
{
	wObject* obj = NULL;
	UwacReturnCode status = UWAC_ERROR_INTERNAL;
	wlfContext* wfl = (wlfContext*)context;

	if (!instance || !context)
		return FALSE;

	instance->PreConnect = wl_pre_connect;
	instance->PostConnect = wl_post_connect;
	instance->PostDisconnect = wl_post_disconnect;
	instance->LogonErrorInfo = wlf_logon_error_info;
	wfl->log = WLog_Get(TAG);
	wfl->display = UwacOpenDisplay(NULL, &status);

	if (!wfl->display || (status != UWAC_SUCCESS) || !wfl->log)
		return FALSE;

	wfl->displayHandle = CreateFileDescriptorEvent(NULL, FALSE, FALSE,
	                                               UwacDisplayGetFd(wfl->display), WINPR_FD_READ);

	if (!wfl->displayHandle)
		return FALSE;

	wfl->events = ArrayList_New(FALSE);
	if (!wfl->events)
		return FALSE;

	obj = ArrayList_Object(wfl->events);
	obj->fnObjectNew = uwac_event_clone;
	obj->fnObjectFree = free;

	InitializeCriticalSection(&wfl->critical);

	return TRUE;
}

static int wfl_client_start(rdpContext* context)
{
	WINPR_UNUSED(context);
	return 0;
}

static int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	WINPR_ASSERT(pEntryPoints);
	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = wlf_client_global_init;
	pEntryPoints->GlobalUninit = wlf_client_global_uninit;
	pEntryPoints->ContextSize = sizeof(wlfContext);
	pEntryPoints->ClientNew = wlf_client_new;
	pEntryPoints->ClientFree = wlf_client_free;
	pEntryPoints->ClientStart = wfl_client_start;
	pEntryPoints->ClientStop = freerdp_client_common_stop;
	return 0;
}

int main(int argc, char* argv[])
{
	int rc = -1;
	int status = 0;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* context = NULL;
	rdpSettings* settings = NULL;
	wlfContext* wlc = NULL;

	freerdp_client_warn_deprecated(argc, argv);

	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);
	if (!context)
		goto fail;
	wlc = (wlfContext*)context;
	settings = context->settings;

	status = freerdp_client_settings_parse_command_line(settings, argc, argv, FALSE);
	if (status)
	{
		rc = freerdp_client_settings_command_line_status_print(settings, status, argc, argv);

		if (freerdp_settings_get_bool(settings, FreeRDP_ListMonitors))
			wlf_list_monitors(wlc);

		goto fail;
	}

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
		const size_t baseSrcOffset = 1ULL * area->top * srcStride + 4ULL * area->left;
		const size_t baseDstOffset = 1ULL * area->top * dstStride + 4ULL * area->left;
		const size_t width = MIN((size_t)area->right - area->left, dstWidth - area->left);
		const size_t height = MIN((size_t)area->bottom - area->top, dstHeight - area->top);
		const BYTE* psrc = (const BYTE*)src;
		BYTE* pdst = (BYTE*)dst;

		for (size_t i = 0; i < height; i++)
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
	UwacSize geometry = { 0 };

	if (!context || !px || !py || !context->gdi)
		return FALSE;

	if (!freerdp_settings_get_bool(context->settings, FreeRDP_SmartSizing))
		return TRUE;

	rdpGdi* gdi = context->gdi;

	if (UwacWindowGetDrawingBufferGeometry(wlf->window, &geometry, NULL) != UWAC_SUCCESS)
		return FALSE;

	const double sx = 1.0 * geometry.width / (double)gdi->width;
	const double sy = 1.0 * geometry.height / (double)gdi->height;

	if (!fromLocalToRDP)
	{
		*px *= (UINT32)lround(sx);
		*py *= (UINT32)lround(sy);
	}
	else
	{
		*px /= (UINT32)lround(sx);
		*py /= (UINT32)lround(sy);
	}

	return TRUE;
}
