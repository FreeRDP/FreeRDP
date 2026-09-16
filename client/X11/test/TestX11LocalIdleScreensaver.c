#include <winpr/crt.h>
#include <winpr/synch.h>

#include "../xfreerdp.h"

int main(int argc, char* argv[])
{
	xfContext xfc = WINPR_C_ARRAY_INIT;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	xfc.display = XOpenDisplay(NULL);
	if (!xfc.display)
		return -1;

	xfc.local_idle_screensaver_timeout = 1000;
	xf_local_idle_screensaver_input(&xfc);
	xf_local_idle_screensaver_check(&xfc);
	if (xfc.local_idle_screensaver_active)
		return -2;

	Sleep(1100);
	xf_local_idle_screensaver_check(&xfc);
	XCloseDisplay(xfc.display);
	return xfc.local_idle_screensaver_active ? 0 : -3;
}
