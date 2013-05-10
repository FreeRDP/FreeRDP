/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Server Monitors
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "xf_monitors.h"

int xf_list_monitors(xfInfo* xfi)
{
#ifdef WITH_XINERAMAZ
	int i, nmonitors = 0;
	int ignored, ignored2;
	XineramaScreenInfo* screen = NULL;

	if (XineramaQueryExtension(xfi->display, &ignored, &ignored2))
	{
		if (XineramaIsActive(xfi->display))
		{
			screen = XineramaQueryScreens(xfi->display, &nmonitors);

			for (i = 0; i < nmonitors; i++)
			{
				printf("      %s [%d] %dx%d\t+%d+%d\n",
				       (i == 0) ? "*" : " ", i,
				       screen[i].width, screen[i].height,
				       screen[i].x_org, screen[i].y_org);
			}

			XFree(screen);
		}
	}
#else
	Screen* screen;

	screen = ScreenOfDisplay(xfi->display, DefaultScreen(xfi->display));
	printf("      * [0] %dx%d\t+%d+%d\n", WidthOfScreen(screen), HeightOfScreen(screen), 0, 0);
#endif

	return 0;
}

