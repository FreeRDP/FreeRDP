/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/select.h>
#include <sys/signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/image.h>
#include <winpr/sysinfo.h>

#include <freerdp/log.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include "x11_shadow.h"

#define TAG SERVER_TAG("shadow.x11")

static UINT32 x11_shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors);

#ifdef WITH_PAM

#include <security/pam_appl.h>

struct _SHADOW_PAM_AUTH_DATA
{
	const char* user;
	const char* domain;
	const char* password;
};
typedef struct _SHADOW_PAM_AUTH_DATA SHADOW_PAM_AUTH_DATA;

struct _SHADOW_PAM_AUTH_INFO
{
	char* service_name;
	pam_handle_t* handle;
	struct pam_conv pamc;
	SHADOW_PAM_AUTH_DATA appdata;
};
typedef struct _SHADOW_PAM_AUTH_INFO SHADOW_PAM_AUTH_INFO;

static int x11_shadow_pam_conv(int num_msg, const struct pam_message** msg,
                               struct pam_response** resp, void* appdata_ptr)
{
	int index;
	int pam_status = PAM_CONV_ERR;
	SHADOW_PAM_AUTH_DATA* appdata;
	struct pam_response* response;
	appdata = (SHADOW_PAM_AUTH_DATA*)appdata_ptr;

	if (!(response = (struct pam_response*)calloc(num_msg, sizeof(struct pam_response))))
		return PAM_BUF_ERR;

	for (index = 0; index < num_msg; index++)
	{
		switch (msg[index]->msg_style)
		{
			case PAM_PROMPT_ECHO_ON:
				response[index].resp = _strdup(appdata->user);

				if (!response[index].resp)
					goto out_fail;

				response[index].resp_retcode = PAM_SUCCESS;
				break;

			case PAM_PROMPT_ECHO_OFF:
				response[index].resp = _strdup(appdata->password);

				if (!response[index].resp)
					goto out_fail;

				response[index].resp_retcode = PAM_SUCCESS;
				break;

			default:
				pam_status = PAM_CONV_ERR;
				goto out_fail;
		}
	}

	*resp = response;
	return PAM_SUCCESS;
out_fail:

	for (index = 0; index < num_msg; ++index)
	{
		if (response[index].resp)
		{
			memset(response[index].resp, 0, strlen(response[index].resp));
			free(response[index].resp);
		}
	}

	memset(response, 0, sizeof(struct pam_response) * num_msg);
	free(response);
	*resp = NULL;
	return pam_status;
}

static BOOL x11_shadow_pam_get_service_name(SHADOW_PAM_AUTH_INFO* info)
{
	size_t x;
	const char* base = "/etc/pam.d";
	const char* hints[] = { "lightdm", "gdm", "xdm", "login", "sshd" };

	for (x = 0; x < ARRAYSIZE(hints); x++)
	{
		char path[MAX_PATH];
		const char* hint = hints[x];

		_snprintf(path, sizeof(path), "%s/%s", base, hint);
		if (PathFileExistsA(path))
		{

			info->service_name = _strdup(hint);
			return info->service_name != NULL;
		}
	}
	WLog_WARN(TAG, "Could not determine PAM service name");
	return FALSE;
}

static int x11_shadow_pam_authenticate(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                       const char* user, const char* domain, const char* password)
{
	int pam_status;
	SHADOW_PAM_AUTH_INFO info = { 0 };
	WINPR_UNUSED(subsystem);
	WINPR_UNUSED(client);

	if (!x11_shadow_pam_get_service_name(&info))
		return -1;

	info.appdata.user = user;
	info.appdata.domain = domain;
	info.appdata.password = password;
	info.pamc.conv = &x11_shadow_pam_conv;
	info.pamc.appdata_ptr = &info.appdata;
	pam_status = pam_start(info.service_name, 0, &info.pamc, &info.handle);

	if (pam_status != PAM_SUCCESS)
	{
		WLog_ERR(TAG, "pam_start failure: %s", pam_strerror(info.handle, pam_status));
		return -1;
	}

	pam_status = pam_authenticate(info.handle, 0);

	if (pam_status != PAM_SUCCESS)
	{
		WLog_ERR(TAG, "pam_authenticate failure: %s", pam_strerror(info.handle, pam_status));
		return -1;
	}

	pam_status = pam_acct_mgmt(info.handle, 0);

	if (pam_status != PAM_SUCCESS)
	{
		WLog_ERR(TAG, "pam_acct_mgmt failure: %s", pam_strerror(info.handle, pam_status));
		return -1;
	}

	return 1;
}

#endif

static BOOL x11_shadow_input_synchronize_event(rdpShadowSubsystem* subsystem,
                                               rdpShadowClient* client, UINT32 flags)
{
	/* TODO: Implement */
	WLog_WARN(TAG, "%s not implemented", __FUNCTION__);
	return TRUE;
}

static BOOL x11_shadow_input_keyboard_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                            UINT16 flags, UINT16 code)
{
#ifdef WITH_XTEST
	x11ShadowSubsystem* x11 = (x11ShadowSubsystem*)subsystem;
	DWORD vkcode;
	DWORD keycode;
	BOOL extended = FALSE;

	if (!client || !subsystem)
		return FALSE;

	if (flags & KBD_FLAGS_EXTENDED)
		extended = TRUE;

	if (extended)
		code |= KBDEXT;

	vkcode = GetVirtualKeyCodeFromVirtualScanCode(code, 4);

	if (extended)
		vkcode |= KBDEXT;

	keycode = GetKeycodeFromVirtualKeyCode(vkcode, KEYCODE_TYPE_EVDEV);

	if (keycode != 0)
	{
		XLockDisplay(x11->display);
		XTestGrabControl(x11->display, True);

		if (flags & KBD_FLAGS_DOWN)
			XTestFakeKeyEvent(x11->display, keycode, True, CurrentTime);
		else if (flags & KBD_FLAGS_RELEASE)
			XTestFakeKeyEvent(x11->display, keycode, False, CurrentTime);

		XTestGrabControl(x11->display, False);
		XFlush(x11->display);
		XUnlockDisplay(x11->display);
	}

#endif
	return TRUE;
}

static BOOL x11_shadow_input_unicode_keyboard_event(rdpShadowSubsystem* subsystem,
                                                    rdpShadowClient* client, UINT16 flags,
                                                    UINT16 code)
{
	/* TODO: Implement */
	WLog_WARN(TAG, "%s not implemented", __FUNCTION__);
	return TRUE;
}

static BOOL x11_shadow_input_mouse_event(rdpShadowSubsystem* subsystem, rdpShadowClient* client,
                                         UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	x11ShadowSubsystem* x11 = (x11ShadowSubsystem*)subsystem;
	int button = 0;
	BOOL down = FALSE;
	rdpShadowServer* server;
	rdpShadowSurface* surface;

	if (!subsystem || !client)
		return FALSE;

	server = subsystem->server;

	if (!server)
		return FALSE;

	surface = server->surface;

	if (!surface)
		return FALSE;

	x11->lastMouseClient = client;
	x += surface->x;
	y += surface->y;
	XLockDisplay(x11->display);
	XTestGrabControl(x11->display, True);

	if (flags & PTR_FLAGS_WHEEL)
	{
		BOOL negative = FALSE;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			negative = TRUE;

		button = (negative) ? 5 : 4;
		XTestFakeButtonEvent(x11->display, button, True, CurrentTime);
		XTestFakeButtonEvent(x11->display, button, False, CurrentTime);
	}
	else
	{
		if (flags & PTR_FLAGS_MOVE)
			XTestFakeMotionEvent(x11->display, 0, x, y, CurrentTime);

		if (flags & PTR_FLAGS_BUTTON1)
			button = 1;
		else if (flags & PTR_FLAGS_BUTTON2)
			button = 3;
		else if (flags & PTR_FLAGS_BUTTON3)
			button = 2;

		if (flags & PTR_FLAGS_DOWN)
			down = TRUE;

		if (button)
			XTestFakeButtonEvent(x11->display, button, down, CurrentTime);
	}

	XTestGrabControl(x11->display, False);
	XFlush(x11->display);
	XUnlockDisplay(x11->display);
#endif
	return TRUE;
}

static BOOL x11_shadow_input_extended_mouse_event(rdpShadowSubsystem* subsystem,
                                                  rdpShadowClient* client, UINT16 flags, UINT16 x,
                                                  UINT16 y)
{
#ifdef WITH_XTEST
	x11ShadowSubsystem* x11 = (x11ShadowSubsystem*)subsystem;
	int button = 0;
	BOOL down = FALSE;
	rdpShadowServer* server;
	rdpShadowSurface* surface;

	if (!subsystem || !client)
		return FALSE;

	server = subsystem->server;

	if (!server)
		return FALSE;

	surface = server->surface;

	if (!surface)
		return FALSE;

	x11->lastMouseClient = client;
	x += surface->x;
	y += surface->y;
	XLockDisplay(x11->display);
	XTestGrabControl(x11->display, True);
	XTestFakeMotionEvent(x11->display, 0, x, y, CurrentTime);

	if (flags & PTR_XFLAGS_BUTTON1)
		button = 8;
	else if (flags & PTR_XFLAGS_BUTTON2)
		button = 9;

	if (flags & PTR_XFLAGS_DOWN)
		down = TRUE;

	if (button)
		XTestFakeButtonEvent(x11->display, button, down, CurrentTime);

	XTestGrabControl(x11->display, False);
	XFlush(x11->display);
	XUnlockDisplay(x11->display);
#endif
	return TRUE;
}

static void x11_shadow_message_free(UINT32 id, SHADOW_MSG_OUT* msg)
{
	switch (id)
	{
		case SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID:
			free(msg);
			break;

		case SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID:
			free(((SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE*)msg)->xorMaskData);
			free(((SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE*)msg)->andMaskData);
			free(msg);
			break;

		default:
			WLog_ERR(TAG, "Unknown message id: %" PRIu32 "", id);
			free(msg);
			break;
	}
}

static int x11_shadow_pointer_position_update(x11ShadowSubsystem* subsystem)
{
	UINT32 msgId = SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID;
	rdpShadowServer* server;
	SHADOW_MSG_OUT_POINTER_POSITION_UPDATE templateMsg;
	int count = 0;
	size_t index = 0;

	if (!subsystem || !subsystem->common.server || !subsystem->common.server->clients)
		return -1;

	templateMsg.xPos = subsystem->common.pointerX;
	templateMsg.yPos = subsystem->common.pointerY;
	templateMsg.common.Free = x11_shadow_message_free;
	server = subsystem->common.server;
	ArrayList_Lock(server->clients);

	for (index = 0; index < ArrayList_Count(server->clients); index++)
	{
		SHADOW_MSG_OUT_POINTER_POSITION_UPDATE* msg;
		rdpShadowClient* client = (rdpShadowClient*)ArrayList_GetItem(server->clients, index);

		/* Skip the client which send us the latest mouse event */
		if (client == subsystem->lastMouseClient)
			continue;

		msg = malloc(sizeof(templateMsg));

		if (!msg)
		{
			count = -1;
			break;
		}

		memcpy(msg, &templateMsg, sizeof(templateMsg));

		if (shadow_client_post_msg(client, NULL, msgId, (SHADOW_MSG_OUT*)msg, NULL))
			count++;
	}

	ArrayList_Unlock(server->clients);
	return count;
}

static int x11_shadow_pointer_alpha_update(x11ShadowSubsystem* subsystem)
{
	SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* msg;
	UINT32 msgId = SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID;
	msg = (SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE*)calloc(1,
	                                                   sizeof(SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE));

	if (!msg)
		return -1;

	msg->xHot = subsystem->cursorHotX;
	msg->yHot = subsystem->cursorHotY;
	msg->width = subsystem->cursorWidth;
	msg->height = subsystem->cursorHeight;

	if (shadow_subsystem_pointer_convert_alpha_pointer_data(subsystem->cursorPixels, TRUE,
	                                                        msg->width, msg->height, msg) < 0)
	{
		free(msg);
		return -1;
	}

	msg->common.Free = x11_shadow_message_free;
	return shadow_client_boardcast_msg(subsystem->common.server, NULL, msgId, (SHADOW_MSG_OUT*)msg,
	                                   NULL)
	           ? 1
	           : -1;
}

static int x11_shadow_query_cursor(x11ShadowSubsystem* subsystem, BOOL getImage)
{
	int x = 0, y = 0, n, k;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	server = subsystem->common.server;
	surface = server->surface;

	if (getImage)
	{
#ifdef WITH_XFIXES
		UINT32* pDstPixel;
		XFixesCursorImage* ci;
		XLockDisplay(subsystem->display);
		ci = XFixesGetCursorImage(subsystem->display);
		XUnlockDisplay(subsystem->display);

		if (!ci)
			return -1;

		x = ci->x;
		y = ci->y;

		if (ci->width > subsystem->cursorMaxWidth)
			return -1;

		if (ci->height > subsystem->cursorMaxHeight)
			return -1;

		subsystem->cursorHotX = ci->xhot;
		subsystem->cursorHotY = ci->yhot;
		subsystem->cursorWidth = ci->width;
		subsystem->cursorHeight = ci->height;
		subsystem->cursorId = ci->cursor_serial;
		n = ci->width * ci->height;
		pDstPixel = (UINT32*)subsystem->cursorPixels;

		for (k = 0; k < n; k++)
		{
			/* XFixesCursorImage.pixels is in *unsigned long*, which may be 8 bytes */
			*pDstPixel++ = (UINT32)ci->pixels[k];
		}

		XFree(ci);
		x11_shadow_pointer_alpha_update(subsystem);
#endif
	}
	else
	{
		UINT32 mask;
		int win_x, win_y;
		int root_x, root_y;
		Window root, child;
		XLockDisplay(subsystem->display);

		if (!XQueryPointer(subsystem->display, subsystem->root_window, &root, &child, &root_x,
		                   &root_y, &win_x, &win_y, &mask))
		{
			XUnlockDisplay(subsystem->display);
			return -1;
		}

		XUnlockDisplay(subsystem->display);
		x = root_x;
		y = root_y;
	}

	/* Convert to offset based on current surface */
	if (surface)
	{
		x -= surface->x;
		y -= surface->y;
	}

	if ((x != (INT64)subsystem->common.pointerX) || (y != (INT64)subsystem->common.pointerY))
	{
		subsystem->common.pointerX = x;
		subsystem->common.pointerY = y;
		x11_shadow_pointer_position_update(subsystem);
	}

	return 1;
}

static int x11_shadow_handle_xevent(x11ShadowSubsystem* subsystem, XEvent* xevent)
{
	if (xevent->type == MotionNotify)
	{
	}

#ifdef WITH_XFIXES
	else if (xevent->type == subsystem->xfixes_cursor_notify_event)
	{
		x11_shadow_query_cursor(subsystem, TRUE);
	}

#endif
	else
	{
	}

	return 1;
}

static void x11_shadow_validate_region(x11ShadowSubsystem* subsystem, int x, int y, int width,
                                       int height)
{
	XRectangle region;

	if (!subsystem->use_xfixes || !subsystem->use_xdamage)
		return;

	region.x = x;
	region.y = y;
	region.width = width;
	region.height = height;
#ifdef WITH_XFIXES
	XLockDisplay(subsystem->display);
	XFixesSetRegion(subsystem->display, subsystem->xdamage_region, &region, 1);
	XDamageSubtract(subsystem->display, subsystem->xdamage, subsystem->xdamage_region, None);
	XUnlockDisplay(subsystem->display);
#endif
}

static int x11_shadow_blend_cursor(x11ShadowSubsystem* subsystem)
{
	int x, y;
	int nXSrc;
	int nYSrc;
	int nXDst;
	int nYDst;
	int nWidth;
	int nHeight;
	int nSrcStep;
	int nDstStep;
	BYTE* pSrcData;
	BYTE* pDstData;
	BYTE A, R, G, B;
	rdpShadowSurface* surface;

	if (!subsystem)
		return -1;

	surface = subsystem->common.server->surface;
	nXSrc = 0;
	nYSrc = 0;
	nWidth = subsystem->cursorWidth;
	nHeight = subsystem->cursorHeight;
	nXDst = subsystem->common.pointerX - subsystem->cursorHotX;
	nYDst = subsystem->common.pointerY - subsystem->cursorHotY;

	if (nXDst >= surface->width)
		return 1;

	if (nXDst < 0)
	{
		nXDst *= -1;

		if (nXDst >= nWidth)
			return 1;

		nXSrc = nXDst;
		nWidth -= nXDst;
		nXDst = 0;
	}

	if (nYDst >= surface->height)
		return 1;

	if (nYDst < 0)
	{
		nYDst *= -1;

		if (nYDst >= nHeight)
			return 1;

		nYSrc = nYDst;
		nHeight -= nYDst;
		nYDst = 0;
	}

	if ((nXDst + nWidth) > surface->width)
		nWidth = surface->width - nXDst;

	if ((nYDst + nHeight) > surface->height)
		nHeight = surface->height - nYDst;

	pSrcData = subsystem->cursorPixels;
	nSrcStep = subsystem->cursorWidth * 4;
	pDstData = surface->data;
	nDstStep = surface->scanline;

	for (y = 0; y < nHeight; y++)
	{
		const BYTE* pSrcPixel = &pSrcData[((nYSrc + y) * nSrcStep) + (nXSrc * 4)];
		BYTE* pDstPixel = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];

		for (x = 0; x < nWidth; x++)
		{
			B = *pSrcPixel++;
			G = *pSrcPixel++;
			R = *pSrcPixel++;
			A = *pSrcPixel++;

			if (A == 0xFF)
			{
				pDstPixel[0] = B;
				pDstPixel[1] = G;
				pDstPixel[2] = R;
			}
			else
			{
				pDstPixel[0] = B + (pDstPixel[0] * (0xFF - A) + (0xFF / 2)) / 0xFF;
				pDstPixel[1] = G + (pDstPixel[1] * (0xFF - A) + (0xFF / 2)) / 0xFF;
				pDstPixel[2] = R + (pDstPixel[2] * (0xFF - A) + (0xFF / 2)) / 0xFF;
			}

			pDstPixel[3] = 0xFF;
			pDstPixel += 4;
		}
	}

	return 1;
}

static BOOL x11_shadow_check_resize(x11ShadowSubsystem* subsystem)
{
	MONITOR_DEF* virtualScreen;
	XWindowAttributes attr;
	XLockDisplay(subsystem->display);
	XGetWindowAttributes(subsystem->display, subsystem->root_window, &attr);
	XUnlockDisplay(subsystem->display);

	if (attr.width != (INT64)subsystem->width || attr.height != (INT64)subsystem->height)
	{
		/* Screen size changed. Refresh monitor definitions and trigger screen resize */
		subsystem->common.numMonitors = x11_shadow_enum_monitors(subsystem->common.monitors, 16);
		shadow_screen_resize(subsystem->common.server->screen);
		subsystem->width = attr.width;
		subsystem->height = attr.height;
		virtualScreen = &(subsystem->common.virtualScreen);
		virtualScreen->left = 0;
		virtualScreen->top = 0;
		virtualScreen->right = subsystem->width;
		virtualScreen->bottom = subsystem->height;
		virtualScreen->flags = 1;
		return TRUE;
	}

	return FALSE;
}

static int x11_shadow_error_handler_for_capture(Display* display, XErrorEvent* event)
{
	char msg[256];
	XGetErrorText(display, event->error_code, (char*)&msg, sizeof(msg));
	WLog_ERR(TAG, "X11 error: %s Error code: %x, request code: %x, minor code: %x", msg,
	         event->error_code, event->request_code, event->minor_code);

	/* Ignore BAD MATCH error during image capture. Abort in other case */
	if (event->error_code != BadMatch)
	{
		abort();
	}

	return 0;
}

static int x11_shadow_screen_grab(x11ShadowSubsystem* subsystem)
{
	int rc = 0;
	int count;
	int status;
	int x, y;
	int width, height;
	XImage* image;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 invalidRect;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;
	server = subsystem->common.server;
	surface = server->surface;
	count = ArrayList_Count(server->clients);

	if (count < 1)
		return 1;

	EnterCriticalSection(&surface->lock);
	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = surface->width;
	surfaceRect.bottom = surface->height;
	LeaveCriticalSection(&surface->lock);

	XLockDisplay(subsystem->display);
	/*
	 * Ignore BadMatch error during image capture. The screen size may be
	 * changed outside. We will resize to correct resolution at next frame
	 */
	XSetErrorHandler(x11_shadow_error_handler_for_capture);

	if (subsystem->use_xshm)
	{
		image = subsystem->fb_image;
		XCopyArea(subsystem->display, subsystem->root_window, subsystem->fb_pixmap,
		          subsystem->xshm_gc, 0, 0, subsystem->width, subsystem->height, 0, 0);

		EnterCriticalSection(&surface->lock);
		status = shadow_capture_compare(surface->data, surface->scanline, surface->width,
		                                surface->height, (BYTE*)&(image->data[surface->width * 4]),
		                                image->bytes_per_line, &invalidRect);
		LeaveCriticalSection(&surface->lock);
	}
	else
	{
		EnterCriticalSection(&surface->lock);
		image = XGetImage(subsystem->display, subsystem->root_window, surface->x, surface->y,
		                  surface->width, surface->height, AllPlanes, ZPixmap);

		if (image)
		{
			status = shadow_capture_compare(surface->data, surface->scanline, surface->width,
			                                surface->height, (BYTE*)image->data,
			                                image->bytes_per_line, &invalidRect);
		}
		LeaveCriticalSection(&surface->lock);
		if (!image)
		{
			/*
			 * BadMatch error happened. The size may have been changed again.
			 * Give up this frame and we will resize again in next frame
			 */
			goto fail_capture;
		}
	}

	/* Restore the default error handler */
	XSetErrorHandler(NULL);
	XSync(subsystem->display, False);
	XUnlockDisplay(subsystem->display);

	if (status)
	{
		BOOL empty;
		EnterCriticalSection(&surface->lock);
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
		region16_intersect_rect(&(surface->invalidRegion), &(surface->invalidRegion), &surfaceRect);
		empty = region16_is_empty(&(surface->invalidRegion));
		LeaveCriticalSection(&surface->lock);

		if (!empty)
		{
			BOOL success;
			EnterCriticalSection(&surface->lock);
			extents = region16_extents(&(surface->invalidRegion));
			x = extents->left;
			y = extents->top;
			width = extents->right - extents->left;
			height = extents->bottom - extents->top;
			success = freerdp_image_copy(surface->data, surface->format, surface->scanline, x, y,
			                             width, height, (BYTE*)image->data, PIXEL_FORMAT_BGRX32,
			                             image->bytes_per_line, x, y, NULL, FREERDP_FLIP_NONE);
			LeaveCriticalSection(&surface->lock);
			if (!success)
				goto fail_capture;

			// x11_shadow_blend_cursor(subsystem);
			count = ArrayList_Count(server->clients);
			shadow_subsystem_frame_update(&subsystem->common);

			if (count == 1)
			{
				rdpShadowClient* client;
				client = (rdpShadowClient*)ArrayList_GetItem(server->clients, 0);

				if (client)
					subsystem->common.captureFrameRate =
					    shadow_encoder_preferred_fps(client->encoder);
			}

			EnterCriticalSection(&surface->lock);
			region16_clear(&(surface->invalidRegion));
			LeaveCriticalSection(&surface->lock);
		}
	}

	rc = 1;
fail_capture:
	if (!subsystem->use_xshm && image)
		XDestroyImage(image);

	if (rc != 1)
	{
		XSetErrorHandler(NULL);
		XSync(subsystem->display, False);
		XUnlockDisplay(subsystem->display);
	}

	return rc;
}

static int x11_shadow_subsystem_process_message(x11ShadowSubsystem* subsystem, wMessage* message)
{
	switch (message->id)
	{
		case SHADOW_MSG_IN_REFRESH_REQUEST_ID:
			shadow_subsystem_frame_update((rdpShadowSubsystem*)subsystem);
			break;

		default:
			WLog_ERR(TAG, "Unknown message id: %" PRIu32 "", message->id);
			break;
	}

	if (message->Free)
		message->Free(message);

	return 1;
}

static DWORD WINAPI x11_shadow_subsystem_thread(LPVOID arg)
{
	x11ShadowSubsystem* subsystem = (x11ShadowSubsystem*)arg;
	XEvent xevent;
	DWORD status;
	DWORD nCount;
	UINT64 cTime;
	DWORD dwTimeout;
	DWORD dwInterval;
	UINT64 frameTime;
	HANDLE events[32];
	wMessage message;
	wMessagePipe* MsgPipe;
	MsgPipe = subsystem->common.MsgPipe;
	nCount = 0;
	events[nCount++] = subsystem->common.event;
	events[nCount++] = MessageQueue_Event(MsgPipe->In);
	subsystem->common.captureFrameRate = 16;
	dwInterval = 1000 / subsystem->common.captureFrameRate;
	frameTime = GetTickCount64() + dwInterval;

	while (1)
	{
		cTime = GetTickCount64();
		dwTimeout = (cTime > frameTime) ? 0 : frameTime - cTime;
		status = WaitForMultipleObjects(nCount, events, FALSE, dwTimeout);

		if (WaitForSingleObject(MessageQueue_Event(MsgPipe->In), 0) == WAIT_OBJECT_0)
		{
			if (MessageQueue_Peek(MsgPipe->In, &message, TRUE))
			{
				if (message.id == WMQ_QUIT)
					break;

				x11_shadow_subsystem_process_message(subsystem, &message);
			}
		}

		if (WaitForSingleObject(subsystem->common.event, 0) == WAIT_OBJECT_0)
		{
			XLockDisplay(subsystem->display);

			if (XEventsQueued(subsystem->display, QueuedAlready))
			{
				XNextEvent(subsystem->display, &xevent);
				x11_shadow_handle_xevent(subsystem, &xevent);
			}

			XUnlockDisplay(subsystem->display);
		}

		if ((status == WAIT_TIMEOUT) || (GetTickCount64() > frameTime))
		{
			x11_shadow_check_resize(subsystem);
			x11_shadow_screen_grab(subsystem);
			x11_shadow_query_cursor(subsystem, FALSE);
			dwInterval = 1000 / subsystem->common.captureFrameRate;
			frameTime += dwInterval;
		}
	}

	ExitThread(0);
	return 0;
}

static int x11_shadow_subsystem_base_init(x11ShadowSubsystem* subsystem)
{
	if (subsystem->display)
		return 1; /* initialize once */

	if (!getenv("DISPLAY"))
		setenv("DISPLAY", ":0", 1);

	if (!XInitThreads())
		return -1;

	subsystem->display = XOpenDisplay(NULL);

	if (!subsystem->display)
	{
		WLog_ERR(TAG, "failed to open display: %s", XDisplayName(NULL));
		return -1;
	}

	subsystem->xfds = ConnectionNumber(subsystem->display);
	subsystem->number = DefaultScreen(subsystem->display);
	subsystem->screen = ScreenOfDisplay(subsystem->display, subsystem->number);
	subsystem->depth = DefaultDepthOfScreen(subsystem->screen);
	subsystem->width = WidthOfScreen(subsystem->screen);
	subsystem->height = HeightOfScreen(subsystem->screen);
	subsystem->root_window = RootWindow(subsystem->display, subsystem->number);
	return 1;
}

static int x11_shadow_xfixes_init(x11ShadowSubsystem* subsystem)
{
#ifdef WITH_XFIXES
	int xfixes_event;
	int xfixes_error;
	int major, minor;

	if (!XFixesQueryExtension(subsystem->display, &xfixes_event, &xfixes_error))
		return -1;

	if (!XFixesQueryVersion(subsystem->display, &major, &minor))
		return -1;

	subsystem->xfixes_cursor_notify_event = xfixes_event + XFixesCursorNotify;
	XFixesSelectCursorInput(subsystem->display, subsystem->root_window,
	                        XFixesDisplayCursorNotifyMask);
	return 1;
#else
	return -1;
#endif
}

static int x11_shadow_xinerama_init(x11ShadowSubsystem* subsystem)
{
#ifdef WITH_XINERAMA
	int major, minor;
	int xinerama_event;
	int xinerama_error;
	x11_shadow_subsystem_base_init(subsystem);

	if (!XineramaQueryExtension(subsystem->display, &xinerama_event, &xinerama_error))
		return -1;

	if (!XDamageQueryVersion(subsystem->display, &major, &minor))
		return -1;

	if (!XineramaIsActive(subsystem->display))
		return -1;

	return 1;
#else
	return -1;
#endif
}

static int x11_shadow_xdamage_init(x11ShadowSubsystem* subsystem)
{
#ifdef WITH_XDAMAGE
	int major, minor;
	int damage_event;
	int damage_error;

	if (!subsystem->use_xfixes)
		return -1;

	if (!XDamageQueryExtension(subsystem->display, &damage_event, &damage_error))
		return -1;

	if (!XDamageQueryVersion(subsystem->display, &major, &minor))
		return -1;

	if (major < 1)
		return -1;

	subsystem->xdamage_notify_event = damage_event + XDamageNotify;
	subsystem->xdamage =
	    XDamageCreate(subsystem->display, subsystem->root_window, XDamageReportDeltaRectangles);

	if (!subsystem->xdamage)
		return -1;

#ifdef WITH_XFIXES
	subsystem->xdamage_region = XFixesCreateRegion(subsystem->display, NULL, 0);

	if (!subsystem->xdamage_region)
		return -1;

#endif
	return 1;
#else
	return -1;
#endif
}

static int x11_shadow_xshm_init(x11ShadowSubsystem* subsystem)
{
	Bool pixmaps;
	int major, minor;
	XGCValues values;

	if (!XShmQueryExtension(subsystem->display))
		return -1;

	if (!XShmQueryVersion(subsystem->display, &major, &minor, &pixmaps))
		return -1;

	if (!pixmaps)
		return -1;

	subsystem->fb_shm_info.shmid = -1;
	subsystem->fb_shm_info.shmaddr = (char*)-1;
	subsystem->fb_shm_info.readOnly = False;
	subsystem->fb_image =
	    XShmCreateImage(subsystem->display, subsystem->visual, subsystem->depth, ZPixmap, NULL,
	                    &(subsystem->fb_shm_info), subsystem->width, subsystem->height);

	if (!subsystem->fb_image)
	{
		WLog_ERR(TAG, "XShmCreateImage failed");
		return -1;
	}

	subsystem->fb_shm_info.shmid =
	    shmget(IPC_PRIVATE, subsystem->fb_image->bytes_per_line * subsystem->fb_image->height,
	           IPC_CREAT | 0600);

	if (subsystem->fb_shm_info.shmid == -1)
	{
		WLog_ERR(TAG, "shmget failed");
		return -1;
	}

	subsystem->fb_shm_info.shmaddr = shmat(subsystem->fb_shm_info.shmid, 0, 0);
	subsystem->fb_image->data = subsystem->fb_shm_info.shmaddr;

	if (subsystem->fb_shm_info.shmaddr == ((char*)-1))
	{
		WLog_ERR(TAG, "shmat failed");
		return -1;
	}

	if (!XShmAttach(subsystem->display, &(subsystem->fb_shm_info)))
		return -1;

	XSync(subsystem->display, False);
	shmctl(subsystem->fb_shm_info.shmid, IPC_RMID, 0);
	subsystem->fb_pixmap =
	    XShmCreatePixmap(subsystem->display, subsystem->root_window, subsystem->fb_image->data,
	                     &(subsystem->fb_shm_info), subsystem->fb_image->width,
	                     subsystem->fb_image->height, subsystem->fb_image->depth);
	XSync(subsystem->display, False);

	if (!subsystem->fb_pixmap)
		return -1;

	values.subwindow_mode = IncludeInferiors;
	values.graphics_exposures = False;
	subsystem->xshm_gc = XCreateGC(subsystem->display, subsystem->root_window,
	                               GCSubwindowMode | GCGraphicsExposures, &values);
	XSetFunction(subsystem->display, subsystem->xshm_gc, GXcopy);
	XSync(subsystem->display, False);
	return 1;
}

UINT32 x11_shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors)
{
	int index;
	Display* display;
	int displayWidth;
	int displayHeight;
	int numMonitors = 0;
	MONITOR_DEF* monitor;

	if (!getenv("DISPLAY"))
		setenv("DISPLAY", ":0", 1);

	display = XOpenDisplay(NULL);

	if (!display)
	{
		WLog_ERR(TAG, "failed to open display: %s", XDisplayName(NULL));
		return -1;
	}

	displayWidth = WidthOfScreen(DefaultScreenOfDisplay(display));
	displayHeight = HeightOfScreen(DefaultScreenOfDisplay(display));
#ifdef WITH_XINERAMA
	{
		int major, minor;
		int xinerama_event;
		int xinerama_error;
		XineramaScreenInfo* screen;
		XineramaScreenInfo* screens;

		if (XineramaQueryExtension(display, &xinerama_event, &xinerama_error) &&
		    XDamageQueryVersion(display, &major, &minor) && XineramaIsActive(display))
		{
			screens = XineramaQueryScreens(display, &numMonitors);

			if (numMonitors > (INT64)maxMonitors)
				numMonitors = (int)maxMonitors;

			if (screens && (numMonitors > 0))
			{
				for (index = 0; index < numMonitors; index++)
				{
					screen = &screens[index];
					monitor = &monitors[index];
					monitor->left = screen->x_org;
					monitor->top = screen->y_org;
					monitor->right = monitor->left + screen->width;
					monitor->bottom = monitor->top + screen->height;
					monitor->flags = (index == 0) ? 1 : 0;
				}
			}

			XFree(screens);
		}
	}
#endif
	XCloseDisplay(display);

	if (numMonitors < 1)
	{
		index = 0;
		numMonitors = 1;
		monitor = &monitors[index];
		monitor->left = 0;
		monitor->top = 0;
		monitor->right = displayWidth;
		monitor->bottom = displayHeight;
		monitor->flags = 1;
	}

	errno = 0;
	return numMonitors;
}

static int x11_shadow_subsystem_init(rdpShadowSubsystem* sub)
{
	int i;
	int pf_count;
	int vi_count;
	int nextensions;
	char** extensions;
	XVisualInfo* vi;
	XVisualInfo* vis;
	XVisualInfo template;
	XPixmapFormatValues* pf;
	XPixmapFormatValues* pfs;
	MONITOR_DEF* virtualScreen;
	x11ShadowSubsystem* subsystem = (x11ShadowSubsystem*)sub;

	if (!subsystem)
		return -1;

	subsystem->common.numMonitors = x11_shadow_enum_monitors(subsystem->common.monitors, 16);
	x11_shadow_subsystem_base_init(subsystem);

	if ((subsystem->depth != 24) && (subsystem->depth != 32))
	{
		WLog_ERR(TAG, "unsupported X11 server color depth: %d", subsystem->depth);
		return -1;
	}

	extensions = XListExtensions(subsystem->display, &nextensions);

	if (!extensions || (nextensions < 0))
		return -1;

	for (i = 0; i < nextensions; i++)
	{
		if (strcmp(extensions[i], "Composite") == 0)
			subsystem->composite = TRUE;
	}

	XFreeExtensionList(extensions);

	if (subsystem->composite)
		subsystem->use_xdamage = FALSE;

	pfs = XListPixmapFormats(subsystem->display, &pf_count);

	if (!pfs)
	{
		WLog_ERR(TAG, "XListPixmapFormats failed");
		return -1;
	}

	for (i = 0; i < pf_count; i++)
	{
		pf = pfs + i;

		if (pf->depth == (INT64)subsystem->depth)
		{
			subsystem->bpp = pf->bits_per_pixel;
			subsystem->scanline_pad = pf->scanline_pad;
			break;
		}
	}

	XFree(pfs);
	ZeroMemory(&template, sizeof(template));
	template.class = TrueColor;
	template.screen = subsystem->number;
	vis = XGetVisualInfo(subsystem->display, VisualClassMask | VisualScreenMask, &template,
	                     &vi_count);

	if (!vis)
	{
		WLog_ERR(TAG, "XGetVisualInfo failed");
		return -1;
	}

	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->depth == (INT64)subsystem->depth)
		{
			subsystem->visual = vi->visual;
			break;
		}
	}

	XFree(vis);
	XSelectInput(subsystem->display, subsystem->root_window, SubstructureNotifyMask);
	subsystem->cursorMaxWidth = 256;
	subsystem->cursorMaxHeight = 256;
	subsystem->cursorPixels =
	    _aligned_malloc(subsystem->cursorMaxWidth * subsystem->cursorMaxHeight * 4, 16);

	if (!subsystem->cursorPixels)
		return -1;

	x11_shadow_query_cursor(subsystem, TRUE);

	if (subsystem->use_xfixes)
	{
		if (x11_shadow_xfixes_init(subsystem) < 0)
			subsystem->use_xfixes = FALSE;
	}

	if (subsystem->use_xinerama)
	{
		if (x11_shadow_xinerama_init(subsystem) < 0)
			subsystem->use_xinerama = FALSE;
	}

	if (subsystem->use_xshm)
	{
		if (x11_shadow_xshm_init(subsystem) < 0)
			subsystem->use_xshm = FALSE;
	}

	if (subsystem->use_xdamage)
	{
		if (x11_shadow_xdamage_init(subsystem) < 0)
			subsystem->use_xdamage = FALSE;
	}

	if (!(subsystem->common.event =
	          CreateFileDescriptorEvent(NULL, FALSE, FALSE, subsystem->xfds, WINPR_FD_READ)))
		return -1;

	virtualScreen = &(subsystem->common.virtualScreen);
	virtualScreen->left = 0;
	virtualScreen->top = 0;
	virtualScreen->right = subsystem->width;
	virtualScreen->bottom = subsystem->height;
	virtualScreen->flags = 1;
	WLog_INFO(TAG,
	          "X11 Extensions: XFixes: %" PRId32 " Xinerama: %" PRId32 " XDamage: %" PRId32
	          " XShm: %" PRId32 "",
	          subsystem->use_xfixes, subsystem->use_xinerama, subsystem->use_xdamage,
	          subsystem->use_xshm);
	return 1;
}

static int x11_shadow_subsystem_uninit(rdpShadowSubsystem* sub)
{
	x11ShadowSubsystem* subsystem = (x11ShadowSubsystem*)sub;

	if (!subsystem)
		return -1;

	if (subsystem->display)
	{
		XCloseDisplay(subsystem->display);
		subsystem->display = NULL;
	}

	if (subsystem->common.event)
	{
		CloseHandle(subsystem->common.event);
		subsystem->common.event = NULL;
	}

	if (subsystem->cursorPixels)
	{
		_aligned_free(subsystem->cursorPixels);
		subsystem->cursorPixels = NULL;
	}

	return 1;
}

static int x11_shadow_subsystem_start(rdpShadowSubsystem* sub)
{
	x11ShadowSubsystem* subsystem = (x11ShadowSubsystem*)sub;

	if (!subsystem)
		return -1;

	if (!(subsystem->thread =
	          CreateThread(NULL, 0, x11_shadow_subsystem_thread, (void*)subsystem, 0, NULL)))
	{
		WLog_ERR(TAG, "Failed to create thread");
		return -1;
	}

	return 1;
}

static int x11_shadow_subsystem_stop(rdpShadowSubsystem* sub)
{
	x11ShadowSubsystem* subsystem = (x11ShadowSubsystem*)sub;

	if (!subsystem)
		return -1;

	if (subsystem->thread)
	{
		if (MessageQueue_PostQuit(subsystem->common.MsgPipe->In, 0))
			WaitForSingleObject(subsystem->thread, INFINITE);

		CloseHandle(subsystem->thread);
		subsystem->thread = NULL;
	}

	return 1;
}

static rdpShadowSubsystem* x11_shadow_subsystem_new(void)
{
	x11ShadowSubsystem* subsystem;
	subsystem = (x11ShadowSubsystem*)calloc(1, sizeof(x11ShadowSubsystem));

	if (!subsystem)
		return NULL;

#ifdef WITH_PAM
	subsystem->common.Authenticate = x11_shadow_pam_authenticate;
#endif
	subsystem->common.SynchronizeEvent = x11_shadow_input_synchronize_event;
	subsystem->common.KeyboardEvent = x11_shadow_input_keyboard_event;
	subsystem->common.UnicodeKeyboardEvent = x11_shadow_input_unicode_keyboard_event;
	subsystem->common.MouseEvent = x11_shadow_input_mouse_event;
	subsystem->common.ExtendedMouseEvent = x11_shadow_input_extended_mouse_event;
	subsystem->composite = FALSE;
	subsystem->use_xshm = FALSE; /* temporarily disabled */
	subsystem->use_xfixes = TRUE;
	subsystem->use_xdamage = FALSE;
	subsystem->use_xinerama = TRUE;
	return (rdpShadowSubsystem*)subsystem;
}

static void x11_shadow_subsystem_free(rdpShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	x11_shadow_subsystem_uninit(subsystem);
	free(subsystem);
}

FREERDP_API int X11_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	if (!pEntryPoints)
		return -1;

	pEntryPoints->New = x11_shadow_subsystem_new;
	pEntryPoints->Free = x11_shadow_subsystem_free;
	pEntryPoints->Init = x11_shadow_subsystem_init;
	pEntryPoints->Uninit = x11_shadow_subsystem_uninit;
	pEntryPoints->Start = x11_shadow_subsystem_start;
	pEntryPoints->Stop = x11_shadow_subsystem_stop;
	pEntryPoints->EnumMonitors = x11_shadow_enum_monitors;
	return 1;
}
