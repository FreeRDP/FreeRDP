#include <freerdp/config.h>
#include <X11/extensions/Xrandr.h>
#include "../xfreerdp.h"
#include "../xf_monitor.h"

static BOOL check_size(Display* display, const char* name, BOOL fullscreen, BOOL workarea,
                       UINT32 percent, UINT32 width, UINT32 height)
{
	MONITOR_INFO monitors[16] = WINPR_C_ARRAY_INIT;
	xfContext xfc = WINPR_C_ARRAY_INIT;
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		return FALSE;

	xfc.display = display;
	xfc.screen = DefaultScreenOfDisplay(display);
	xfc.log = WLog_Get("com.freerdp.client.x11.test");
	xfc.vscreen.monitors = monitors;
	xfc.common.context.settings = settings;
	BOOL rc = freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosX, 1200) &&
	          freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosY, 600) &&
	          freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1900) &&
	          freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 1200) &&
	          freerdp_settings_set_bool(settings, FreeRDP_Fullscreen, fullscreen) &&
	          freerdp_settings_set_bool(settings, FreeRDP_Workarea, workarea) &&
	          freerdp_settings_set_uint32(settings, FreeRDP_PercentScreen, percent) &&
	          freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseWidth, percent != 0) &&
	          freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseHeight, percent != 0);
	UINT32 actual_width = 0;
	UINT32 actual_height = 0;
	if (rc)
		rc = xf_detect_monitors(&xfc, &actual_width, &actual_height);
	if (!rc || actual_width != width || actual_height != height || xfc.vscreen.nmonitors != 2)
	{
		fprintf(stderr, "%s: expected %ux%u, got %ux%u (%u monitors)\n", name, width, height,
		        actual_width, actual_height, xfc.vscreen.nmonitors);
		rc = FALSE;
	}
	freerdp_settings_free(settings);
	return rc;
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

	rc = 0;
	if (!check_size(display, "windowed", FALSE, FALSE, 0, 1900, 1200))
		rc = 1;
	if (!check_size(display, "fullscreen", TRUE, FALSE, 0, 1200, 1920))
		rc = 1;
	if (!check_size(display, "workarea", FALSE, TRUE, 0, 1200, 1920))
		rc = 1;
	if (!check_size(display, "50 percent", FALSE, FALSE, 50, 600, 960))
		rc = 1;
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
