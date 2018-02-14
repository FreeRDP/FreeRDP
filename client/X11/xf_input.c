/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Input
 *
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#endif

#include <math.h>

#include "xf_event.h"
#include "xf_input.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

#ifdef WITH_XI

#define MAX_CONTACTS 2

#define PAN_THRESHOLD 50
#define ZOOM_THRESHOLD 10

#define MIN_FINGER_DIST 5

typedef struct touch_contact
{
	int id;
	int count;
	double pos_x;
	double pos_y;
	double last_x;
	double last_y;

} touchContact;

static touchContact contacts[MAX_CONTACTS];

static int active_contacts;
static int lastEvType;
static XIDeviceEvent lastEvent;
static double firstDist = -1.0;
static double lastDist;

static double z_vector;
static double px_vector;
static double py_vector;

const char* xf_input_get_class_string(int class)
{
	if (class == XIKeyClass)
		return "XIKeyClass";
	else if (class == XIButtonClass)
		return "XIButtonClass";
	else if (class == XIValuatorClass)
		return "XIValuatorClass";
	else if (class == XIScrollClass)
		return "XIScrollClass";
	else if (class == XITouchClass)
		return "XITouchClass";

	return "XIUnknownClass";
}


int xf_input_init(xfContext* xfc, Window window)
{
	int i, j;
	int nmasks;
	int ndevices;
	int major = 2;
	int minor = 2;
	Status xstatus;
	XIDeviceInfo* info;
	XIEventMask evmasks[64];
	int opcode, event, error;
	BYTE masks[8][XIMaskLen(XI_LASTEVENT)];
	z_vector = 0;
	px_vector = 0;
	py_vector = 0;
	nmasks = 0;
	ndevices = 0;
	active_contacts = 0;
	ZeroMemory(contacts, sizeof(touchContact) * MAX_CONTACTS);

	if (!XQueryExtension(xfc->display, "XInputExtension", &opcode, &event, &error))
	{
		WLog_WARN(TAG, "XInput extension not available.");
		return -1;
	}

	xfc->XInputOpcode = opcode;
	XIQueryVersion(xfc->display, &major, &minor);

	if (major * 1000 + minor < 2002)
	{
		WLog_WARN(TAG, "Server does not support XI 2.2");
		return -1;
	}

	if (xfc->context.settings->MultiTouchInput)
		xfc->use_xinput = TRUE;

	info = XIQueryDevice(xfc->display, XIAllDevices, &ndevices);

	for (i = 0; i < ndevices; i++)
	{
		BOOL touch = FALSE;
		XIDeviceInfo* dev = &info[i];

		for (j = 0; j < dev->num_classes; j++)
		{
			XIAnyClassInfo* class = dev->classes[j];
			XITouchClassInfo* t = (XITouchClassInfo*) class;

			if ((class->type == XITouchClass) && (t->mode == XIDirectTouch) &&
			    (strcmp(dev->name, "Virtual core pointer") != 0))
			{
				touch = TRUE;
			}
		}

		for (j = 0; j < dev->num_classes; j++)
		{
			XIAnyClassInfo* class = dev->classes[j];
			XITouchClassInfo* t = (XITouchClassInfo*) class;

			if (xfc->context.settings->MultiTouchInput)
			{
				WLog_INFO(TAG, "%s (%d) \"%s\" id: %d",
				          xf_input_get_class_string(class->type),
				          class->type, dev->name, dev->deviceid);
			}

			evmasks[nmasks].mask = masks[nmasks];
			evmasks[nmasks].mask_len = sizeof(masks[0]);
			ZeroMemory(masks[nmasks], sizeof(masks[0]));
			evmasks[nmasks].deviceid = dev->deviceid;

			if ((class->type == XITouchClass) && (t->mode == XIDirectTouch) &&
			    (strcmp(dev->name, "Virtual core pointer") != 0))
			{
				if (xfc->context.settings->MultiTouchInput)
				{
					WLog_INFO(TAG, "%s %s touch device (id: %d, mode: %d), supporting %d touches.",
					          dev->name, (t->mode == XIDirectTouch) ? "direct" : "dependent",
					          dev->deviceid, t->mode, t->num_touches);
				}

				XISetMask(masks[nmasks], XI_TouchBegin);
				XISetMask(masks[nmasks], XI_TouchUpdate);
				XISetMask(masks[nmasks], XI_TouchEnd);
				nmasks++;
			}

			if (xfc->use_xinput)
			{
				if (!touch && (class->type == XIButtonClass)
				    && strcmp(dev->name, "Virtual core pointer"))
				{
					WLog_INFO(TAG, "%s button device (id: %d, mode: %d)",
					          dev->name,
					          dev->deviceid, t->mode);
					XISetMask(masks[nmasks], XI_ButtonPress);
					XISetMask(masks[nmasks], XI_ButtonRelease);
					XISetMask(masks[nmasks], XI_Motion);
					nmasks++;
				}
			}
		}
	}

	XIFreeDeviceInfo(info);

	if (nmasks > 0)
		xstatus = XISelectEvents(xfc->display, window, evmasks, nmasks);

	return 0;
}

static BOOL xf_input_is_duplicate(XGenericEventCookie* cookie)
{
	XIDeviceEvent* event;
	event = cookie->data;

	if ((lastEvent.time == event->time) &&
	    (lastEvType == cookie->evtype) &&
	    (lastEvent.detail == event->detail) &&
	    (lastEvent.event_x == event->event_x) &&
	    (lastEvent.event_y == event->event_y))
	{
		return TRUE;
	}

	return FALSE;
}

static void xf_input_save_last_event(XGenericEventCookie* cookie)
{
	XIDeviceEvent* event;
	event = cookie->data;
	lastEvType = cookie->evtype;
	lastEvent.time = event->time;
	lastEvent.detail = event->detail;
	lastEvent.event_x = event->event_x;
	lastEvent.event_y = event->event_y;
}

static void xf_input_detect_pan(xfContext* xfc)
{
	double dx[2];
	double dy[2];
	double px;
	double py;
	double dist_x;
	double dist_y;
	rdpContext* ctx = &xfc->context;

	if (active_contacts != 2)
	{
		return;
	}

	dx[0] = contacts[0].pos_x - contacts[0].last_x;
	dx[1] = contacts[1].pos_x - contacts[1].last_x;
	dy[0] = contacts[0].pos_y - contacts[0].last_y;
	dy[1] = contacts[1].pos_y - contacts[1].last_y;
	px = fabs(dx[0]) < fabs(dx[1]) ? dx[0] : dx[1];
	py = fabs(dy[0]) < fabs(dy[1]) ? dy[0] : dy[1];
	px_vector += px;
	py_vector += py;
	dist_x = fabs(contacts[0].pos_x - contacts[1].pos_x);
	dist_y = fabs(contacts[0].pos_y - contacts[1].pos_y);

	if (dist_y > MIN_FINGER_DIST)
	{
		if (px_vector > PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = 5;
				e.dy = 0;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
			}
			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}
		else if (px_vector < -PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = -5;
				e.dy = 0;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
			}
			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}
	}

	if (dist_x > MIN_FINGER_DIST)
	{
		if (py_vector > PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = 0;
				e.dy = 5;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
			}
			py_vector = 0;
			px_vector = 0;
			z_vector = 0;
		}
		else if (py_vector < -PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = 0;
				e.dy = -5;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
			}
			py_vector = 0;
			px_vector = 0;
			z_vector = 0;
		}
	}
}

static void xf_input_detect_pinch(xfContext* xfc)
{
	double dist;
	double delta;
	ZoomingChangeEventArgs e;
	rdpContext* ctx = &xfc->context;

	if (active_contacts != 2)
	{
		firstDist = -1.0;
		return;
	}

	/* first calculate the distance */
	dist = sqrt(pow(contacts[1].pos_x - contacts[0].last_x, 2.0) +
	            pow(contacts[1].pos_y - contacts[0].last_y, 2.0));

	/* if this is the first 2pt touch */
	if (firstDist <= 0)
	{
		firstDist = dist;
		lastDist = firstDist;
		z_vector = 0;
		px_vector = 0;
		py_vector = 0;
	}
	else
	{
		delta = lastDist - dist;

		if (delta > 1.0)
			delta = 1.0;

		if (delta < -1.0)
			delta = -1.0;

		/* compare the current distance to the first one */
		z_vector += delta;
		lastDist = dist;

		if (z_vector > ZOOM_THRESHOLD)
		{
			EventArgsInit(&e, "xfreerdp");
			e.dx = e.dy = -10;
			PubSub_OnZoomingChange(ctx->pubSub, xfc, &e);
			z_vector = 0;
			px_vector = 0;
			py_vector = 0;
		}

		if (z_vector < -ZOOM_THRESHOLD)
		{
			EventArgsInit(&e, "xfreerdp");
			e.dx = e.dy = 10;
			PubSub_OnZoomingChange(ctx->pubSub, xfc, &e);
			z_vector = 0;
			px_vector = 0;
			py_vector = 0;
		}
	}
}

static void xf_input_touch_begin(xfContext* xfc, XIDeviceEvent* event)
{
	int i;

	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (contacts[i].id == 0)
		{
			contacts[i].id = event->detail;
			contacts[i].count = 1;
			contacts[i].pos_x = event->event_x;
			contacts[i].pos_y = event->event_y;
			active_contacts++;
			break;
		}
	}
}

static void xf_input_touch_update(xfContext* xfc, XIDeviceEvent* event)
{
	int i;

	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (contacts[i].id == event->detail)
		{
			contacts[i].count++;
			contacts[i].last_x = contacts[i].pos_x;
			contacts[i].last_y = contacts[i].pos_y;
			contacts[i].pos_x = event->event_x;
			contacts[i].pos_y = event->event_y;
			xf_input_detect_pinch(xfc);
			xf_input_detect_pan(xfc);
			break;
		}
	}
}

static void xf_input_touch_end(xfContext* xfc, XIDeviceEvent* event)
{
	int i;

	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (contacts[i].id == event->detail)
		{
			contacts[i].id = 0;
			contacts[i].count = 0;
			active_contacts--;
			break;
		}
	}
}

static int xf_input_handle_event_local(xfContext* xfc, XEvent* event)
{
	XGenericEventCookie* cookie = &event->xcookie;
	XGetEventData(xfc->display, cookie);

	if ((cookie->type == GenericEvent) && (cookie->extension == xfc->XInputOpcode))
	{
		switch (cookie->evtype)
		{
			case XI_TouchBegin:
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_begin(xfc, cookie->data);

				xf_input_save_last_event(cookie);
				break;

			case XI_TouchUpdate:
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_update(xfc, cookie->data);

				xf_input_save_last_event(cookie);
				break;

			case XI_TouchEnd:
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_end(xfc, cookie->data);

				xf_input_save_last_event(cookie);
				break;

			default:
				WLog_ERR(TAG, "unhandled xi type= %d", cookie->evtype);
				break;
		}
	}

	XFreeEventData(xfc->display, cookie);
	return 0;
}

#ifdef WITH_DEBUG_X11
static char* xf_input_touch_state_string(DWORD flags)
{
	if (flags & CONTACT_FLAG_DOWN)
		return "TouchBegin";
	else if (flags & CONTACT_FLAG_UPDATE)
		return "TouchUpdate";
	else if (flags & CONTACT_FLAG_UP)
		return "TouchEnd";
	else
		return "TouchUnknown";
}
#endif

static void xf_input_hide_cursor(xfContext* xfc)
{
#ifdef WITH_XCURSOR

	if (!xfc->cursorHidden)
	{
		XcursorImage ci;
		XcursorPixel xp = 0;
		static Cursor nullcursor = None;
		xf_lock_x11(xfc, FALSE);
		ZeroMemory(&ci, sizeof(ci));
		ci.version = XCURSOR_IMAGE_VERSION;
		ci.size = sizeof(ci);
		ci.width = ci.height = 1;
		ci.xhot = ci.yhot = 0;
		ci.pixels = &xp;
		nullcursor = XcursorImageLoadCursor(xfc->display, &ci);

		if ((xfc->window) && (nullcursor != None))
			XDefineCursor(xfc->display, xfc->window->handle, nullcursor);

		xfc->cursorHidden = TRUE;
		xf_unlock_x11(xfc, FALSE);
	}

#endif
}

static void xf_input_show_cursor(xfContext* xfc)
{
#ifdef WITH_XCURSOR
	xf_lock_x11(xfc, FALSE);

	if (xfc->cursorHidden)
	{
		if (xfc->window)
		{
			if (!xfc->pointer)
				XUndefineCursor(xfc->display, xfc->window->handle);
			else
				XDefineCursor(xfc->display, xfc->window->handle, xfc->pointer->cursor);
		}

		xfc->cursorHidden = FALSE;
	}

	xf_unlock_x11(xfc, FALSE);
#endif
}

static int xf_input_touch_remote(xfContext* xfc, XIDeviceEvent* event, int evtype)
{
	int x, y;
	int touchId;
	int contactId;
	RdpeiClientContext* rdpei = xfc->rdpei;

	if (!rdpei)
		return 0;

	xf_input_hide_cursor(xfc);
	touchId = event->detail;
	x = (int) event->event_x;
	y = (int) event->event_y;
	xf_event_adjust_coordinates(xfc, &x, &y);

	if (evtype == XI_TouchBegin)
	{
		WLog_DBG(TAG, "TouchBegin: %d", touchId);
		rdpei->TouchBegin(rdpei, touchId, x, y, &contactId);
	}
	else if (evtype == XI_TouchUpdate)
	{
		WLog_DBG(TAG, "TouchUpdate: %d", touchId);
		rdpei->TouchUpdate(rdpei, touchId, x, y, &contactId);
	}
	else if (evtype == XI_TouchEnd)
	{
		WLog_DBG(TAG, "TouchEnd: %d", touchId);
		rdpei->TouchEnd(rdpei, touchId, x, y, &contactId);
	}

	return 0;
}

static int xf_input_event(xfContext* xfc, XIDeviceEvent* event, int evtype)
{
	xf_input_show_cursor(xfc);

	switch (evtype)
	{
		case XI_ButtonPress:
			xf_generic_ButtonPress(xfc, (int) event->event_x, (int) event->event_y,
			                       event->detail, event->event, xfc->remote_app);
			break;

		case XI_ButtonRelease:
			xf_generic_ButtonRelease(xfc, (int) event->event_x, (int) event->event_y,
			                         event->detail, event->event, xfc->remote_app);
			break;

		case XI_Motion:
			xf_generic_MotionNotify(xfc, (int) event->event_x, (int) event->event_y,
			                        event->detail, event->event, xfc->remote_app);
			break;
	}

	return 0;
}

static int xf_input_handle_event_remote(xfContext* xfc, XEvent* event)
{
	XGenericEventCookie* cookie = &event->xcookie;
	XGetEventData(xfc->display, cookie);

	if ((cookie->type == GenericEvent) && (cookie->extension == xfc->XInputOpcode))
	{
		switch (cookie->evtype)
		{
			case XI_TouchBegin:
				xf_input_touch_remote(xfc, cookie->data, XI_TouchBegin);
				break;

			case XI_TouchUpdate:
				xf_input_touch_remote(xfc, cookie->data, XI_TouchUpdate);
				break;

			case XI_TouchEnd:
				xf_input_touch_remote(xfc, cookie->data, XI_TouchEnd);
				break;

			default:
				xf_input_event(xfc, cookie->data, cookie->evtype);
				break;
		}
	}

	XFreeEventData(xfc->display, cookie);
	return 0;
}

#else

int xf_input_init(xfContext* xfc, Window window)
{
	return 0;
}

#endif

int xf_input_handle_event(xfContext* xfc, XEvent* event)
{
#ifdef WITH_XI

	if (xfc->context.settings->MultiTouchInput)
	{
		return xf_input_handle_event_remote(xfc, event);
	}

	if (xfc->context.settings->MultiTouchGestures)
	{
		return xf_input_handle_event_local(xfc, event);
	}

#endif
	return 0;
}
