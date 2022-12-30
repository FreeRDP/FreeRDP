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

#include <freerdp/config.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#endif

#include <math.h>
#include <float.h>
#include <limits.h>

#include "xf_event.h"
#include "xf_input.h"

#include <winpr/assert.h>
#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

#ifdef WITH_XI

#define PAN_THRESHOLD 50
#define ZOOM_THRESHOLD 10

#define MIN_FINGER_DIST 5

static int xf_input_event(xfContext* xfc, const XEvent* xevent, XIDeviceEvent* event, int evtype);

static const char* xf_input_get_class_string(int class)
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

static BOOL register_input_events(xfContext* xfc, Window window)
{
	int i, ndevices;
	int nmasks = 0;
	XIDeviceInfo* info;
	rdpSettings* settings;
	XIEventMask evmasks[64] = { 0 };
	BYTE masks[8][XIMaskLen(XI_LASTEVENT)] = { 0 };

	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	info = XIQueryDevice(xfc->display, XIAllDevices, &ndevices);

	for (i = 0; i < MIN(ndevices, 64); i++)
	{
		int j;
		BOOL used = FALSE;
		XIDeviceInfo* dev = &info[i];

		evmasks[nmasks].mask = masks[nmasks];
		evmasks[nmasks].mask_len = sizeof(masks[0]);
		evmasks[nmasks].deviceid = dev->deviceid;

		/* Ignore virtual core pointer */
		if (strcmp(dev->name, "Virtual core pointer") == 0)
			continue;

		for (j = 0; j < dev->num_classes; j++)
		{
			const XIAnyClassInfo* class = dev->classes[j];

			switch (class->type)
			{
				case XITouchClass:
					if (settings->MultiTouchInput)
					{
						const XITouchClassInfo* t = (const XITouchClassInfo*)class;
						if (t->mode == XIDirectTouch)
						{
							WLog_DBG(
							    TAG,
							    "%s %s touch device (id: %d, mode: %d), supporting %d touches.",
							    dev->name, (t->mode == XIDirectTouch) ? "direct" : "dependent",
							    dev->deviceid, t->mode, t->num_touches);
							XISetMask(masks[nmasks], XI_TouchBegin);
							XISetMask(masks[nmasks], XI_TouchUpdate);
							XISetMask(masks[nmasks], XI_TouchEnd);
						}
					}
					break;
				case XIButtonClass:
				{
					const XIButtonClassInfo* t = (const XIButtonClassInfo*)class;
					WLog_DBG(TAG, "%s button device (id: %d, mode: %d)", dev->name, dev->deviceid,
					         t->num_buttons);
					XISetMask(masks[nmasks], XI_ButtonPress);
					XISetMask(masks[nmasks], XI_ButtonRelease);
					XISetMask(masks[nmasks], XI_Motion);
					used = TRUE;
				}
				break;
				default:
					break;
			}
		}
		if (used)
			nmasks++;
	}

	XIFreeDeviceInfo(info);

	if (nmasks > 0)
	{
		Status xstatus = XISelectEvents(xfc->display, window, evmasks, nmasks);
		if (xstatus != 0)
			WLog_WARN(TAG, "XISelectEvents returned %d", xstatus);
	}

	return TRUE;
}

static BOOL register_raw_events(xfContext* xfc, Window window)
{
	XIEventMask mask;
	unsigned char mask_bytes[XIMaskLen(XI_LASTEVENT)] = { 0 };
	rdpSettings* settings;

	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (freerdp_settings_get_bool(settings, FreeRDP_MouseUseRelativeMove))
	{
		XISetMask(mask_bytes, XI_RawMotion);
		XISetMask(mask_bytes, XI_RawButtonPress);
		XISetMask(mask_bytes, XI_RawButtonRelease);

		mask.deviceid = XIAllMasterDevices;
		mask.mask_len = sizeof(mask_bytes);
		mask.mask = mask_bytes;

		XISelectEvents(xfc->display, window, &mask, 1);
	}

	return TRUE;
}

int xf_input_init(xfContext* xfc, Window window)
{
	int major = XI_2_Major;
	int minor = XI_2_Minor;
	int opcode, event, error;
	rdpSettings* settings;

	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	xfc->firstDist = -1.0;
	xfc->z_vector = 0;
	xfc->px_vector = 0;
	xfc->py_vector = 0;
	xfc->active_contacts = 0;

	if (!XQueryExtension(xfc->display, "XInputExtension", &opcode, &event, &error))
	{
		WLog_WARN(TAG, "XInput extension not available.");
		return -1;
	}

	xfc->XInputOpcode = opcode;
	XIQueryVersion(xfc->display, &major, &minor);

	if ((major < XI_2_Major) || ((major == XI_2_Major) && (minor < 2)))
	{
		WLog_WARN(TAG, "Server does not support XI 2.2");
		return -1;
	}
	else
	{
		int scr = DefaultScreen(xfc->display);
		Window root = RootWindow(xfc->display, scr);

		if (!register_raw_events(xfc, root))
			return -1;
		if (!register_input_events(xfc, window))
			return -1;
	}

	return 0;
}

static BOOL xf_input_is_duplicate(xfContext* xfc, const XGenericEventCookie* cookie)
{
	const XIDeviceEvent* event;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(cookie);

	event = cookie->data;
	WINPR_ASSERT(event);

	if ((xfc->lastEvent.time == event->time) && (xfc->lastEvType == cookie->evtype) &&
	    (xfc->lastEvent.detail == event->detail) &&
	    (fabs(xfc->lastEvent.event_x - event->event_x) < DBL_EPSILON) &&
	    (fabs(xfc->lastEvent.event_y - event->event_y) < DBL_EPSILON))
	{
		return TRUE;
	}

	return FALSE;
}

static void xf_input_save_last_event(xfContext* xfc, const XGenericEventCookie* cookie)
{
	const XIDeviceEvent* event;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(cookie);

	event = cookie->data;
	WINPR_ASSERT(event);

	xfc->lastEvType = cookie->evtype;
	xfc->lastEvent.time = event->time;
	xfc->lastEvent.detail = event->detail;
	xfc->lastEvent.event_x = event->event_x;
	xfc->lastEvent.event_y = event->event_y;
}

static void xf_input_detect_pan(xfContext* xfc)
{
	double dx[2];
	double dy[2];
	double px;
	double py;
	double dist_x;
	double dist_y;
	rdpContext* ctx;

	WINPR_ASSERT(xfc);
	ctx = &xfc->common.context;
	WINPR_ASSERT(ctx);

	if (xfc->active_contacts != 2)
	{
		return;
	}

	dx[0] = xfc->contacts[0].pos_x - xfc->contacts[0].last_x;
	dx[1] = xfc->contacts[1].pos_x - xfc->contacts[1].last_x;
	dy[0] = xfc->contacts[0].pos_y - xfc->contacts[0].last_y;
	dy[1] = xfc->contacts[1].pos_y - xfc->contacts[1].last_y;
	px = fabs(dx[0]) < fabs(dx[1]) ? dx[0] : dx[1];
	py = fabs(dy[0]) < fabs(dy[1]) ? dy[0] : dy[1];
	xfc->px_vector += px;
	xfc->py_vector += py;
	dist_x = fabs(xfc->contacts[0].pos_x - xfc->contacts[1].pos_x);
	dist_y = fabs(xfc->contacts[0].pos_y - xfc->contacts[1].pos_y);

	if (dist_y > MIN_FINGER_DIST)
	{
		if (xfc->px_vector > PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = 5;
				e.dy = 0;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
			}
			xfc->px_vector = 0;
			xfc->py_vector = 0;
			xfc->z_vector = 0;
		}
		else if (xfc->px_vector < -PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = -5;
				e.dy = 0;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
			}
			xfc->px_vector = 0;
			xfc->py_vector = 0;
			xfc->z_vector = 0;
		}
	}

	if (dist_x > MIN_FINGER_DIST)
	{
		if (xfc->py_vector > PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = 0;
				e.dy = 5;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
			}
			xfc->py_vector = 0;
			xfc->px_vector = 0;
			xfc->z_vector = 0;
		}
		else if (xfc->py_vector < -PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = 0;
				e.dy = -5;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
			}
			xfc->py_vector = 0;
			xfc->px_vector = 0;
			xfc->z_vector = 0;
		}
	}
}

static void xf_input_detect_pinch(xfContext* xfc)
{
	double dist;
	double delta;
	ZoomingChangeEventArgs e;
	rdpContext* ctx;

	WINPR_ASSERT(xfc);
	ctx = &xfc->common.context;
	WINPR_ASSERT(ctx);

	if (xfc->active_contacts != 2)
	{
		xfc->firstDist = -1.0;
		return;
	}

	/* first calculate the distance */
	dist = sqrt(pow(xfc->contacts[1].pos_x - xfc->contacts[0].last_x, 2.0) +
	            pow(xfc->contacts[1].pos_y - xfc->contacts[0].last_y, 2.0));

	/* if this is the first 2pt touch */
	if (xfc->firstDist <= 0)
	{
		xfc->firstDist = dist;
		xfc->lastDist = xfc->firstDist;
		xfc->z_vector = 0;
		xfc->px_vector = 0;
		xfc->py_vector = 0;
	}
	else
	{
		delta = xfc->lastDist - dist;

		if (delta > 1.0)
			delta = 1.0;

		if (delta < -1.0)
			delta = -1.0;

		/* compare the current distance to the first one */
		xfc->z_vector += delta;
		xfc->lastDist = dist;

		if (xfc->z_vector > ZOOM_THRESHOLD)
		{
			EventArgsInit(&e, "xfreerdp");
			e.dx = e.dy = -10;
			PubSub_OnZoomingChange(ctx->pubSub, xfc, &e);
			xfc->z_vector = 0;
			xfc->px_vector = 0;
			xfc->py_vector = 0;
		}

		if (xfc->z_vector < -ZOOM_THRESHOLD)
		{
			EventArgsInit(&e, "xfreerdp");
			e.dx = e.dy = 10;
			PubSub_OnZoomingChange(ctx->pubSub, xfc, &e);
			xfc->z_vector = 0;
			xfc->px_vector = 0;
			xfc->py_vector = 0;
		}
	}
}

static void xf_input_touch_begin(xfContext* xfc, const XIDeviceEvent* event)
{
	int i;

	WINPR_UNUSED(xfc);
	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (xfc->contacts[i].id == 0)
		{
			xfc->contacts[i].id = event->detail;
			xfc->contacts[i].count = 1;
			xfc->contacts[i].pos_x = event->event_x;
			xfc->contacts[i].pos_y = event->event_y;
			xfc->active_contacts++;
			break;
		}
	}
}

static void xf_input_touch_update(xfContext* xfc, const XIDeviceEvent* event)
{
	int i;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(event);

	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (xfc->contacts[i].id == event->detail)
		{
			xfc->contacts[i].count++;
			xfc->contacts[i].last_x = xfc->contacts[i].pos_x;
			xfc->contacts[i].last_y = xfc->contacts[i].pos_y;
			xfc->contacts[i].pos_x = event->event_x;
			xfc->contacts[i].pos_y = event->event_y;
			xf_input_detect_pinch(xfc);
			xf_input_detect_pan(xfc);
			break;
		}
	}
}

static void xf_input_touch_end(xfContext* xfc, const XIDeviceEvent* event)
{
	int i;

	WINPR_UNUSED(xfc);
	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (xfc->contacts[i].id == event->detail)
		{
			xfc->contacts[i].id = 0;
			xfc->contacts[i].count = 0;
			xfc->active_contacts--;
			break;
		}
	}
}

static int xf_input_handle_event_local(xfContext* xfc, const XEvent* event)
{
	union
	{
		const XGenericEventCookie* cc;
		XGenericEventCookie* vc;
	} cookie;
	cookie.cc = &event->xcookie;
	XGetEventData(xfc->display, cookie.vc);

	if ((cookie.cc->type == GenericEvent) && (cookie.cc->extension == xfc->XInputOpcode))
	{
		switch (cookie.cc->evtype)
		{
			case XI_TouchBegin:
				if (xf_input_is_duplicate(xfc, cookie.cc) == FALSE)
					xf_input_touch_begin(xfc, cookie.cc->data);

				xf_input_save_last_event(xfc, cookie.cc);
				break;

			case XI_TouchUpdate:
				if (xf_input_is_duplicate(xfc, cookie.cc) == FALSE)
					xf_input_touch_update(xfc, cookie.cc->data);

				xf_input_save_last_event(xfc, cookie.cc);
				break;

			case XI_TouchEnd:
				if (xf_input_is_duplicate(xfc, cookie.cc) == FALSE)
					xf_input_touch_end(xfc, cookie.cc->data);

				xf_input_save_last_event(xfc, cookie.cc);
				break;

			default:
				xf_input_event(xfc, event, cookie.cc->data, cookie.cc->evtype);
				break;
		}
	}

	XFreeEventData(xfc->display, cookie.vc);
	return 0;
}

#ifdef WITH_DEBUG_X11
static char* xf_input_touch_state_string(DWORD flags)
{
	if (flags & RDPINPUT_CONTACT_FLAG_DOWN)
		return "RDPINPUT_CONTACT_FLAG_DOWN";
	else if (flags & RDPINPUT_CONTACT_FLAG_UPDATE)
		return "RDPINPUT_CONTACT_FLAG_UPDATE";
	else if (flags & RDPINPUT_CONTACT_FLAG_UP)
		return "RDPINPUT_CONTACT_FLAG_UP";
	else if (flags & RDPINPUT_CONTACT_FLAG_INRANGE)
		return "RDPINPUT_CONTACT_FLAG_INRANGE";
	else if (flags & RDPINPUT_CONTACT_FLAG_INCONTACT)
		return "RDPINPUT_CONTACT_FLAG_INCONTACT";
	else if (flags & RDPINPUT_CONTACT_FLAG_CANCELED)
		return "RDPINPUT_CONTACT_FLAG_CANCELED";
	else
		return "RDPINPUT_CONTACT_FLAG_UNKNOWN";
}
#endif

static void xf_input_hide_cursor(xfContext* xfc)
{
#ifdef WITH_XCURSOR

	if (!xfc->cursorHidden)
	{
		XcursorImage ci = { 0 };
		XcursorPixel xp = 0;
		static Cursor nullcursor = None;
		xf_lock_x11(xfc);
		ci.version = XCURSOR_IMAGE_VERSION;
		ci.size = sizeof(ci);
		ci.width = ci.height = 1;
		ci.xhot = ci.yhot = 0;
		ci.pixels = &xp;
		nullcursor = XcursorImageLoadCursor(xfc->display, &ci);

		if ((xfc->window) && (nullcursor != None))
			XDefineCursor(xfc->display, xfc->window->handle, nullcursor);

		xfc->cursorHidden = TRUE;
		xf_unlock_x11(xfc);
	}

#endif
}

static void xf_input_show_cursor(xfContext* xfc)
{
#ifdef WITH_XCURSOR
	xf_lock_x11(xfc);

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

	xf_unlock_x11(xfc);
#endif
}

static int xf_input_touch_remote(xfContext* xfc, XIDeviceEvent* event, int evtype)
{
	int x, y;
	int touchId;
	RdpeiClientContext* rdpei = xfc->common.rdpei;

	if (!rdpei)
		return 0;

	xf_input_hide_cursor(xfc);
	touchId = event->detail;
	x = (int)event->event_x;
	y = (int)event->event_y;
	xf_event_adjust_coordinates(xfc, &x, &y);

	switch (evtype)
	{
		case XI_TouchBegin:
			freerdp_client_handle_touch(&xfc->common, FREERDP_TOUCH_DOWN, touchId, 0, x, y);
			break;
		case XI_TouchUpdate:
			freerdp_client_handle_touch(&xfc->common, FREERDP_TOUCH_MOTION, touchId, 0, x, y);
			break;
		case XI_TouchEnd:
			freerdp_client_handle_touch(&xfc->common, FREERDP_TOUCH_UP, touchId, 0, x, y);
			break;
		default:
			break;
	}

	return 0;
}

int xf_input_event(xfContext* xfc, const XEvent* xevent, XIDeviceEvent* event, int evtype)
{
	const rdpSettings* settings;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xevent);
	WINPR_ASSERT(event);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	xfWindow* window = xfc->window;
	if (window)
	{
		if (xf_floatbar_is_locked(window->floatbar))
			return 0;
	}

	xf_input_show_cursor(xfc);

	switch (evtype)
	{
		case XI_ButtonPress:
			xfc->xi_event = TRUE;
			xf_generic_ButtonEvent(xfc, (int)event->event_x, (int)event->event_y, event->detail,
			                       event->event, xfc->remote_app, TRUE);
			break;

		case XI_ButtonRelease:
			xfc->xi_event = TRUE;
			xf_generic_ButtonEvent(xfc, (int)event->event_x, (int)event->event_y, event->detail,
			                       event->event, xfc->remote_app, FALSE);
			break;

		case XI_Motion:
			xfc->xi_event = TRUE;
			xf_generic_MotionNotify(xfc, (int)event->event_x, (int)event->event_y, event->detail,
			                        event->event, xfc->remote_app);
			break;
		case XI_RawButtonPress:
		case XI_RawButtonRelease:
			xfc->xi_rawevent = xfc->common.mouse_grabbed &&
			                   freerdp_settings_get_bool(settings, FreeRDP_MouseUseRelativeMove);

			if (xfc->xi_rawevent)
			{
				const XIRawEvent* ev = (const XIRawEvent*)event;
				xf_generic_RawButtonEvent(xfc, ev->detail, xfc->remote_app,
				                          evtype == XI_RawButtonPress);
			}
			break;
		case XI_RawMotion:
			xfc->xi_rawevent = xfc->common.mouse_grabbed &&
			                   freerdp_settings_get_bool(settings, FreeRDP_MouseUseRelativeMove);

			if (xfc->xi_rawevent)
			{
				const XIRawEvent* ev = (const XIRawEvent*)event;
				double x = 0.0;
				double y = 0.0;
				if (XIMaskIsSet(ev->valuators.mask, 0))
					x = ev->raw_values[0];
				if (XIMaskIsSet(ev->valuators.mask, 1))
					y = ev->raw_values[1];

				xf_generic_RawMotionNotify(xfc, (int)x, (int)y, event->event, xfc->remote_app);
			}
			break;
		default:
			WLog_WARN(TAG, "[%s] Unhandled event %d: Event was registered but is not handled!",
			          __FUNCTION__, evtype);
			break;
	}

	return 0;
}

static int xf_input_handle_event_remote(xfContext* xfc, const XEvent* event)
{
	union
	{
		const XGenericEventCookie* cc;
		XGenericEventCookie* vc;
	} cookie;
	cookie.cc = &event->xcookie;
	XGetEventData(xfc->display, cookie.vc);

	if ((cookie.cc->type == GenericEvent) && (cookie.cc->extension == xfc->XInputOpcode))
	{
		switch (cookie.cc->evtype)
		{
			case XI_TouchBegin:
			case XI_TouchUpdate:
			case XI_TouchEnd:
				xf_input_touch_remote(xfc, cookie.cc->data, cookie.cc->evtype);
				break;

			default:
				xf_input_event(xfc, event, cookie.cc->data, cookie.cc->evtype);
				break;
		}
	}

	XFreeEventData(xfc->display, cookie.vc);
	return 0;
}

#else

int xf_input_init(xfContext* xfc, Window window)
{
	return 0;
}

#endif

int xf_input_handle_event(xfContext* xfc, const XEvent* event)
{
#ifdef WITH_XI
	const rdpSettings* settings;
	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (settings->MultiTouchInput)
	{
		return xf_input_handle_event_remote(xfc, event);
	}
	else if (settings->MultiTouchGestures)
	{
		return xf_input_handle_event_local(xfc, event);
	}
	else
	{
		return xf_input_handle_event_local(xfc, event);
	}

#else
	return 0;
#endif
}
