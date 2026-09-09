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
#include <X11/extensions/Xrandr.h>
#include "../xfreerdp.h"
#include "../xf_monitor.h"

#include "../xf_client.h"
#include "../xf_disp.h"
#include <freerdp/client/cmdline.h>
static BOOL layout_seen;

static UINT capture_layout(DispClientContext* disp, UINT32 count,
                           DISPLAY_CONTROL_MONITOR_LAYOUT* layouts)
{
	WINPR_UNUSED(disp);
	layout_seen = TRUE;
	if (count != 2 || layouts[0].DesktopScaleFactor != 150 || layouts[0].DeviceScaleFactor != 140 ||
	    layouts[1].DesktopScaleFactor != 100 || layouts[1].DeviceScaleFactor != 100)
		return ERROR_INVALID_DATA;
	return CHANNEL_RC_OK;
}

static BOOL check_scale(Display* display)
{
	BOOL ok = FALSE;
	MONITOR_INFO monitors[16] = { 0 };
	RDP_CLIENT_ENTRY_POINTS entry = { 0 };
	entry.Size = sizeof(entry);
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	RdpClientEntry(&entry);
	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return FALSE;
	xfContext* xfc = (xfContext*)context;
	rdpSettings* settings = context->settings;
	xfc->display = display;
	xfc->screen = DefaultScreenOfDisplay(display);
	xfc->vscreen.monitors = monitors;
	char* argv[] = { "xfreerdp", "/v:localhost", "/multimon", "/scale-desktop:125",
		             "/monitor-scale:0:150:140,1:100:100" };
	size_t count = 0;
	COMMAND_LINE_ARGUMENT_A* args = xf_monitor_arguments(&count);
	if (freerdp_client_settings_parse_command_line_ex(settings, ARRAYSIZE(argv), argv, FALSE, args,
	                                                  count, xf_monitor_handle_option, xfc) != 0)
		goto out;
	UINT32 width = 0, height = 0;
	if (!xf_detect_monitors(xfc, &width, &height))
		goto out;
	if (freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount) != 2)
		goto out;
	const rdpMonitor* result =
	    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, 0);
	if (!result || result[0].attributes.desktopScaleFactor != 150 ||
	    result[0].attributes.deviceScaleFactor != 140 ||
	    result[1].attributes.desktopScaleFactor != 100 ||
	    result[1].attributes.deviceScaleFactor != 100)
		goto out;
	xfDispContext* xfdisp = xf_disp_new(xfc);
	if (!xfdisp)
		goto out;
	DispClientContext disp = { 0 };
	disp.custom = xfdisp;
	disp.SendMonitorLayout = capture_layout;
	xfc->xfDisp = xfdisp;
	int eventBase = 0, errorBase = 0;
	XRRQueryExtension(display, &eventBase, &errorBase);
	XEvent event = { 0 };
	event.type = eventBase + RRScreenChangeNotify;
	const BOOL rc = xf_disp_init(xfdisp, &disp) && xf_disp_handle_xevent(xfc, &event);
	xfc->xfDisp = nullptr;
	xf_disp_free(xfdisp);
	if (!rc || !layout_seen)
		goto out;
	const char* bad[] = { "",
		                  "0:99:100",
		                  "0:501:100",
		                  "0:150:150",
		                  "16:150:100",
		                  "-1:150:100",
		                  "0:150:100,",
		                  "0:150:100,,1:100:100",
		                  "0:150:100,0:100:100",
		                  "0:150:100x",
		                  "0:150",
		                  "0:9999999999999999999999999:100" };
	for (size_t i = 0; i < ARRAYSIZE(bad); i++)
	{
		COMMAND_LINE_ARGUMENT_A arg = { 0 };
		arg.Name = "monitor-scale";
		arg.Value = (char*)bad[i];
		if (xf_monitor_handle_option(&arg, xfc) >= 0 || xfc->monitorScales[0][0] != 150)
			goto out;
	}
	rdpMonitor fallback = { 0 };
	fallback.orig_screen = 2;
	xf_monitor_apply_scale(xfc, &fallback);
	if (fallback.attributes.desktopScaleFactor != 125)
		goto out;
	ok = TRUE;
out:
	xfc->display = nullptr;
	xfc->vscreen.monitors = nullptr;
	freerdp_client_context_free(context);
	return ok;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	Display* display = XOpenDisplay(nullptr);
	if (!display)
		return 1;
	const Window root = DefaultRootWindow(display);
	XRRScreenResources* resources = XRRGetScreenResourcesCurrent(display, root);
	XRRMonitorInfo* left = XRRAllocateMonitor(display, 1);
	XRRMonitorInfo* right = XRRAllocateMonitor(display, 0);
	int rc = 1;
	if (!resources || resources->noutput < 1 || !left || !right)
		goto out;

	left->name = XInternAtom(display, "left", False);
	left->primary = True;
	left->width = 1200;
	left->height = 1920;
	left->mwidth = 300;
	left->mheight = 480;
	left->outputs[0] = resources->outputs[0];
	right->name = XInternAtom(display, "right", False);
	right->x = 1200;
	right->y = 600;
	right->width = 1920;
	right->height = 1200;
	right->mwidth = 480;
	right->mheight = 300;
	XRRSetMonitor(display, root, left);
	XRRSetMonitor(display, root, right);
	XWarpPointer(display, None, root, 0, 0, 0, 0, 100, 100);
	XSync(display, False);

	rc = check_scale(display) ? 0 : 1;
out:
	if (resources)
		XRRFreeScreenResources(resources);
	if (left)
		XRRFreeMonitors(left);
	if (right)
		XRRFreeMonitors(right);
	XCloseDisplay(display);
	return rc;
}
