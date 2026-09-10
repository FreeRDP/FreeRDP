/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Windows
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
#include <freerdp/client.h>
#include "xf_reconnect.h"

static void xf_reconnect_draw(xfContext* xfc)
{
	const int screen = DefaultScreen(xfc->display);
	GC gc = XCreateGC(xfc->display, xfc->reconnectWindow, 0, nullptr);
	if (!gc)
		return;
	XSetForeground(xfc->display, gc, BlackPixel(xfc->display, screen));
	XClearWindow(xfc->display, xfc->reconnectWindow);
	XDrawString(xfc->display, xfc->reconnectWindow, gc, 16, 28, xfc->reconnectMessage,
	            (int)strlen(xfc->reconnectMessage));
	const char cancel[] = "Close this window to cancel.";
	XDrawString(xfc->display, xfc->reconnectWindow, gc, 16, 54, cancel, sizeof(cancel) - 1);
	XFreeGC(xfc->display, gc);
	XFlush(xfc->display);
}

SSIZE_T xf_retry_dialog(freerdp* instance, const char* what, size_t current, void* userarg)
{
	const SSIZE_T delay = client_common_retry_dialog(instance, what, current, userarg);
	xfContext* xfc = (xfContext*)instance->context;
	if ((delay < 0) || !xfc->display || strcmp(what, "connection") != 0)
		return delay;

	xf_lock_x11(xfc);
	if (!xfc->reconnectWindow)
	{
		const int screen = DefaultScreen(xfc->display);
		xfc->reconnectWindow =
		    XCreateSimpleWindow(xfc->display, DefaultRootWindow(xfc->display), 0, 0, 360, 80, 1,
		                        BlackPixel(xfc->display, screen), WhitePixel(xfc->display, screen));
		XStoreName(xfc->display, xfc->reconnectWindow, "FreeRDP - Reconnecting");
		XSelectInput(xfc->display, xfc->reconnectWindow, ExposureMask);
		XSetWMProtocols(xfc->display, xfc->reconnectWindow, &xfc->WM_DELETE_WINDOW, 1);
		XMapRaised(xfc->display, xfc->reconnectWindow);
	}
	const UINT32 max =
	    freerdp_settings_get_uint32(instance->context->settings, FreeRDP_AutoReconnectMaxRetries);
	if (max)
		(void)snprintf(xfc->reconnectMessage, sizeof(xfc->reconnectMessage),
		               "Reconnecting: attempt %zu of %" PRIu32, current + 1, max);
	else
		(void)snprintf(xfc->reconnectMessage, sizeof(xfc->reconnectMessage),
		               "Reconnecting: attempt %zu", current + 1);
	xf_reconnect_draw(xfc);
	xf_unlock_x11(xfc);
	return delay;
}

BOOL xf_reconnect_event(xfContext* xfc, const XEvent* event)
{
	if (!xfc->reconnectWindow || event->xany.window != xfc->reconnectWindow)
		return FALSE;
	if (event->type == Expose)
		xf_reconnect_draw(xfc);
	else if (event->type == ClientMessage && event->xclient.format == 32 &&
	         event->xclient.message_type == xfc->WM_PROTOCOLS &&
	         (Atom)event->xclient.data.l[0] == xfc->WM_DELETE_WINDOW)
	{
		(void)freerdp_abort_connect_context(&xfc->common.context);
	}
	return TRUE;
}

void xf_reconnect_close(xfContext* xfc)
{
	if (xfc->display && xfc->reconnectWindow)
	{
		xf_lock_x11(xfc);
		XDestroyWindow(xfc->display, xfc->reconnectWindow);
		xfc->reconnectWindow = 0;
		XFlush(xfc->display);
		xf_unlock_x11(xfc);
	}
}
