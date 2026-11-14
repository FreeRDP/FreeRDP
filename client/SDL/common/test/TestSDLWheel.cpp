#include "../../SDL3/sdl_wheel.hpp"

#include <iostream>

/* Sends n events of `notches` each (SDL's mouse wheel y) and checks the RDP rotation they
 * add up to. 120 is one notch ([MS-RDPBCGR] 2.2.8.1.1.3.1.1.3). */
static bool check(const char* what, float notches, int n, INT32 expect)
{
	SdlWheelAxis axis;
	INT32 sum = 0;
	for (int i = 0; i < n; i++)
		sum += axis.add(notches);
	if (sum != expect)
	{
		std::cerr << "[TestSDLWheel] " << what << ": " << n << " x " << notches << " notches gave "
		          << sum << ", expected " << expect << std::endl;
		return false;
	}
	return true;
}

int TestSDLWheel(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	bool ok = true;
	/* a mouse wheel: one notch per event, nothing changes */
	ok &= check("mouse wheel", 1.0f, 3, 360);
	ok &= check("mouse wheel up", -1.0f, 3, -360);
	/* a browser touchpad: SDL makes 100 pixels one notch, so 4 px is 0.04 (4.8 units) */
	ok &= check("touchpad 4 px", 4.0f / 100.0f, 50, 240);
	ok &= check("touchpad 4 px up", -4.0f / 100.0f, 50, -240);
	/* half a pixel is 0.6 units: alone it is nothing, 100 of them are half a notch */
	ok &= check("touchpad 0.5 px", 0.5f / 100.0f, 100, 60);
	/* a high resolution wheel: an eighth of a notch per event */
	ok &= check("high resolution wheel", 1.0f / 8.0f, 8, 120);
	return ok ? 0 : 1;
}
