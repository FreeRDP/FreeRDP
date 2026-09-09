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
#include <winpr/synch.h>
#include <freerdp/client.h>
#include "../xf_reconnect.h"
#include "../xf_client.h"

int main(void)
{
	int rc = 1;
	RDP_CLIENT_ENTRY_POINTS entry = { 0 };
	entry.Size = sizeof(entry);
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	RdpClientEntry(&entry);
	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return 1;
	xfContext* xfc = (xfContext*)context;
	xfc->display = XOpenDisplay(nullptr);
	xfc->mutex = CreateMutex(nullptr, FALSE, nullptr);
	if (!xfc->display || !xfc->mutex)
		goto out;
	xfc->WM_PROTOCOLS = XInternAtom(xfc->display, "WM_PROTOCOLS", False);
	xfc->WM_DELETE_WINDOW = XInternAtom(xfc->display, "WM_DELETE_WINDOW", False);
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_AutoReconnectionEnabled, TRUE) ||
	    !freerdp_settings_set_uint32(context->settings, FreeRDP_AutoReconnectMaxRetries, 3))
		goto out;
	if (xf_retry_dialog(context->instance, "connection", 0, nullptr) < 0 || !xfc->reconnectWindow)
		goto out;
	XSync(xfc->display, False);
	const Window window = xfc->reconnectWindow;
	XWindowAttributes attrs = { 0 };
	if (!XGetWindowAttributes(xfc->display, window, &attrs) || attrs.map_state != IsViewable)
		goto out;
	if (strcmp(xfc->reconnectMessage, "Reconnecting: attempt 1 of 3") != 0)
		goto out;
	if (xf_retry_dialog(context->instance, "connection", 1, nullptr) < 0 ||
	    xfc->reconnectWindow != window)
		goto out;
	if (strcmp(xfc->reconnectMessage, "Reconnecting: attempt 2 of 3") != 0)
		goto out;
	XEvent event = { 0 };
	event.type = Expose;
	event.xany.window = window;
	if (!xf_reconnect_event(xfc, &event))
		goto out;
	event.type = ClientMessage;
	event.xclient.format = 32;
	event.xclient.message_type = XInternAtom(xfc->display, "WM_PROTOCOLS", False);
	event.xclient.data.l[0] = (long)xfc->WM_DELETE_WINDOW;
	if (!xf_reconnect_event(xfc, &event) ||
	    WaitForSingleObject(freerdp_abort_event(context), 0) != WAIT_OBJECT_0)
		goto out;
	xf_reconnect_close(xfc);
	if (xfc->reconnectWindow)
		goto out;
	if (xf_retry_dialog(context->instance, "connection", 3, nullptr) >= 0 || xfc->reconnectWindow)
		goto out;
	rc = 0;
out:
	xf_reconnect_close(xfc);
	if (xfc->display)
	{
		XCloseDisplay(xfc->display);
		xfc->display = nullptr;
	}
	if (xfc->mutex)
	{
		CloseHandle(xfc->mutex);
		xfc->mutex = nullptr;
	}
	freerdp_client_context_free(context);
	return rc;
}
