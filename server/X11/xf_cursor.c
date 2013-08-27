/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Server Cursor
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#ifdef WITH_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <winpr/crt.h>

#include "xf_cursor.h"

int xf_cursor_init(xfInfo* xfi)
{
#ifdef WITH_XFIXES
	int event;
	int error;

	if (!XFixesQueryExtension(xfi->display, &event, &error))
	{
		fprintf(stderr, "XFixesQueryExtension failed\n");
		return -1;
	}

	xfi->xfixes_notify_event = event + XFixesCursorNotify;

	XFixesSelectCursorInput(xfi->display, DefaultRootWindow(xfi->display), XFixesDisplayCursorNotifyMask);
#endif

	return 0;
}
